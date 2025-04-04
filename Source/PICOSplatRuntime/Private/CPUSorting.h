/*
  Copyright (c) 2025 PICO Technology Co., Ltd. See LICENSE.md.
*/

#pragma once

#include <atomic>
#include <memory>

#include "Async/AsyncWork.h"
#include "Containers/ArrayView.h"
#include "Math/Vector.h"
#include "PackedTypes.h"
#include "Rendering/SplatBuffers.h"
#include "RenderingThread.h"

namespace PICO::Splat
{
enum class ESortingState : std::atomic_signed_lock_free::value_type
{
	Ready,
	InProgress,
	TearDown
};
static_assert(std::atomic<ESortingState>::is_always_lock_free);

/**
 * Owns sorting buffers, and handles synchronization with the GPU.
 */
class FMultithreadedSortingBuffers
{
public:
	/**
	 * Creates CPU resources for sorting splats.
	 *
	 * @param NumSplats - Determines how large the sorting buffers will be.
	 */
	FMultithreadedSortingBuffers(uint32 NumSplats)
		: IdxDistA(NumSplats, EPixelFormat::PF_R32G32_UINT)
		, IdxDistB(NumSplats, EPixelFormat::PF_R32G32_UINT)
		, CopyDst(nullptr)
		, DrawSrc(nullptr)
		, DataCPU()
		, CurrentState(ESortingState::Ready)
		, bCopyInProgress()
	{
		DataCPU.SetNumUninitialized(NumSplats);
	}

	/**
	 * Allocate render resources for sorting.
	 *
	 * @param RHICmdList - The RHI command list to create resources with.
	 */
	void InitResources_RenderThread(FRHICommandListBase& RHICmdList)
	{
		check(IsInRenderingThread());
		IdxDistA.InitRHI(RHICmdList);
		IdxDistB.InitRHI(RHICmdList);
	}

	/**
	 * Releases RHI resources created by InitResources_RenderThread().
	 * If called while a copy is in progress, or from off the rendering thread,
	 * this will enqueue a new render command which releases GPU resources.
	 */
	void ReleaseResources()
	{
		ESortingState PreviousState =
			CurrentState.exchange(ESortingState::TearDown);

		auto DeferredRelease = [&]()
		{
			ENQUEUE_RENDER_COMMAND(DestroyCPUSortingResources)(
				[IdxDistA = std::move(IdxDistA),
			     IdxDistB =
			         std::move(IdxDistB)](FRHICommandList& RHICmdList) mutable
				{
					IdxDistA.ReleaseResource();
					IdxDistB.ReleaseResource();
				});
		};

		switch (PreviousState)
		{
		/**
			 * From render thread, when no task active. Handle release here.
			 */
		case ESortingState::Ready:
		{
			check(IsInRenderingThread());

			/**
			 * If a copy command is enqueued on the render thread, we must defer the
			 * release.
			 */
			if (bCopyInProgress.test())
			{
				DeferredRelease();
			}
			else
			{
				IdxDistA.ReleaseResource();
				IdxDistB.ReleaseResource();
			}
			break;
		}

		/**
		 * From render thread, notifies task thread to handle release.
		 * No-op here.
		 */
		case ESortingState::InProgress:
		{
			check(IsInRenderingThread());
			break;
		}

		/**
		 * From task thread, when it has been told to handle the release.
		 */
		case ESortingState::TearDown:
		{
			check(!IsInRenderingThread());
			DeferredRelease();
			break;
		}
		}
	}

	/**
	 * Gets whether the first copy to GPU has occurred. Until this is true, the
	 * index SRV should not be read from.
	 *
	 * @return Whether the GPU index buffer is ready for a draw.
	 */
	bool IsGPUBufferReady() const { return DrawSrc != nullptr; }

	/**
	 * Get SRV for sorted indices (as a buffer of (index, distance) pairs).
	 *
	 * @return SRV reference for index buffer.
	 */
	FShaderResourceViewRHIRef GetIndicesSRV() const
	{
		check(DrawSrc);
		check(DrawSrc->ShaderResourceViewRHI);
		return DrawSrc->ShaderResourceViewRHI;
	}

	/**
	 * Indicates whether a new sorting task can be launched.
	 *
	 * @return - Whether this is ready for a new sorting task.
	 */
	bool IsReadyForSorting()
	{
		ESortingState State = CurrentState.load();
		check(State != ESortingState::TearDown);
		return State == ESortingState::Ready;
	}

	/**
	 * Marks a sort as in progress.
	 */
	void BeginSorting()
	{
		ESortingState ExpectedState = ESortingState::Ready;
		bool bSuccess = CurrentState.compare_exchange_strong(
			ExpectedState, ESortingState::InProgress);

		// Assert that we were in the `Ready` state, and transitioned to `InProgress`.
		check(bSuccess);
	}

	/**
	 * Marks a sort as complete, following a call to `BeginSorting`.
	 */
	bool EndSorting()
	{
		ESortingState ExpectedState = ESortingState::InProgress;
		bool bSuccess = CurrentState.compare_exchange_strong(
			ExpectedState, ESortingState::Ready);

		// If we did not successfully set the state to `Ready`, then we must have
		// seen a `TearDown` message.
		if (!bSuccess)
		{
			check(ExpectedState == ESortingState::TearDown);
			return true;
		}
		return false;
	}

	/**
	 * Marks a copy as in progress, and returns the data need to do so.
	 * A sort must be in progress, as set by a call to `BeginSorting`.
	 *
	 * This will be called from a task thread.
	 *
	 * @param DstBuffer - The RHI buffer which should be copied to.
	 * @param Src - The source to copy from.
	 * @param Size - The number of bytes to copy.
	 */
	void BeginCopy(FRHIBuffer*& DstBuffer, void*& Src, uint32& Size)
	{
		// Must not be copying.
		bool bAlreadyCopying = bCopyInProgress.test_and_set();
		check(!bAlreadyCopying);

		// Could be `InProgress` or `TearDown` depending on if `TearDown` message
		// came through.
		check(CurrentState.load() != ESortingState::Ready);

		check(CopyDst);
		check(CopyDst->VertexBufferRHI);
		DstBuffer = CopyDst->VertexBufferRHI;
		Src = &DataCPU[0];
		Size = DataCPU.Num() * sizeof(DataCPU[0]);
	}

	/**
	 * Marks a copy as finished following a call to `BeginCopy`. Resources
	 * acquired from the former must no longer be accessed after this call.
	 *
	 * This will be called from the render thread, via an enqueued task. It can
	 * outlive a sort in progress (i.e. after a call to `EndSorting`).
	 */
	void EndCopy()
	{
		// Must be copying.
		bool bCopying = bCopyInProgress.test();
		check(bCopying);

		bCopyInProgress.clear();
		bCopyInProgress.notify_one();
	}

	/**
	 * Waits for the previous copy to finish (via a call to `EndCopy`), if one is
	 * in progress. Returns a pointer to the beginning and to one past the end of
	 * the buffer of `FIndexedDistance`'s which the caller should populate and
	 * sort.
	 *
	 * @param Begin - A pointer to the beginning of the buffer.
	 * @param End - A pointer to one past the end of the buffer.
	 */
	void WaitCopy(FIndexedDistance*& Begin, FIndexedDistance*& End)
	{
		// `while` needed in case of spurious unblock.
		while (bCopyInProgress.test())
		{
			bCopyInProgress.wait(true);
		}

		// Swap buffers.
		//
		// First time through, after swap: CopyDst = A, DrawSrc = null.
		// Second time through, after swap: CopyDst = B, DrawSrc = A.
		if (!DrawSrc)
		{
			DrawSrc = CopyDst ? &IdxDistB : &IdxDistA;
		}
		std::swap(CopyDst, DrawSrc);

		Begin = &DataCPU[0];
		End = Begin + DataCPU.Num();
	}

private:
	FSplatCPUToGPUBuffer IdxDistA;
	FSplatCPUToGPUBuffer IdxDistB;
	FSplatCPUToGPUBuffer* CopyDst;
	FSplatCPUToGPUBuffer* DrawSrc;
	TArray<FIndexedDistance> DataCPU;

	// Task -> Render Thread: Sort finished and copy command enqueued.
	// Render Thread -> Task: Task must release GPU resources itself.
	std::atomic<ESortingState> CurrentState;

	// Render Thread -> Task: Copy command finished.
	std::atomic_flag bCopyInProgress;
};

/**
 * CPU splat sorting task, for use as template parameter to FAsyncTask.
 */
class FCPUSortingTask final : public FNonAbandonableTask
{
public:
	/**
	 * Creates a new task for sorting splats on CPU.
	 *
	 * @param PositionsM - Splat positions to sort, in meters.
	 * @param Buffers - CPU sorting buffers.
	 * @param OriginCM - Viewer origin, in centimeters.
	 * @param Forward - Viewer forward, normalized.
	 * @param Transform - Transform to apply to each position.
	 */
	FCPUSortingTask(
		TConstArrayView<FVector3f> PositionsM,
		std::shared_ptr<FMultithreadedSortingBuffers>& Buffers,
		const FVector3f& OriginCM,
		const FVector3f& Forward,
		const FMatrix44f& Transform)
		: PositionsM(PositionsM)
		, BuffersWeakRef(Buffers)
		, OriginCM(OriginCM)
		, Forward(Forward)
		, Transform(Transform)
	{
		Buffers->BeginSorting();
	}

	// Member functions needed for FAsyncTask.
	void DoWork();
	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(
			FCPUSortingTask, STATGROUP_ThreadPoolAsyncTasks);
	}

private:
	friend class FAutoDeleteAsyncTask<FCPUSortingTask>;

	TConstArrayView<FVector3f> PositionsM;
	std::weak_ptr<FMultithreadedSortingBuffers> BuffersWeakRef;
	FVector3f OriginCM;
	FVector3f Forward;
	FMatrix44f Transform;
};
} // namespace PICO::Splat