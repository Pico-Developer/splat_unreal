/*
  Copyright (c) 2025 PICO Technology Co., Ltd. See LICENSE.md.
*/

#pragma once

#include "CPUSorting.h"
#include "DynamicMeshBuilder.h"
#include "Misc/AssertionMacros.h"
#include "PrimitiveSceneProxy.h"
#include "Rendering/SplatBuffers.h"
#include "SplatComponent.h"

#if WITH_EDITOR
#include "StaticMeshResources.h"
#endif

namespace PICO::Splat
{

/**
 * Render-thread proxy to `USplatComponent`.
 *
 * Owns GPU buffers, and submits draws for Editor-only views (e.g. collision).
 * Actual splat rendering is handled by `FSplatSceneViewExtension`, which holds
 * a reference to this.
 */
class FSplatSceneProxy final : public FPrimitiveSceneProxy
{
public:
	/**
	 * Creates a rendering thread proxy to a Splat Component.
	 * The component *must* have a valid asset attached.
	 *
	 * @param Component - Component being mirrored.
	 */
	FSplatSceneProxy(USplatComponent& Component);

	//~ Begin FPrimitiveSceneProxy Interface
	virtual void
	CreateRenderThreadResources(FRHICommandListBase& RHICmdList) override;
	virtual void DestroyRenderThreadResources() override;
#if WITH_EDITOR
	virtual void GetDynamicMeshElements(
		const TArray<const FSceneView*>& Views,
		const FSceneViewFamily& ViewFamily,
		uint32 VisibilityMap,
		class FMeshElementCollector& Collector) const override;
#endif
	virtual uint32 GetMemoryFootprint() const override
	{
		return sizeof(*this) + GetAllocatedSize();
	}
	virtual SIZE_T GetTypeHash() const override
	{
		static size_t UniquePointer;
		return reinterpret_cast<SIZE_T>(&UniquePointer);
	}
#if WITH_EDITOR
	virtual FPrimitiveViewRelevance
	GetViewRelevance(const FSceneView* View) const override;
#endif
	//~ End FPrimitiveSceneProxy Interface

	/**
	 * @return The number of splats.
	 */
	uint32 GetNumSplats() const
	{
		check(Asset);
		return Asset->GetNumSplats();
	}

	/**
	 * Tells whether this splat should be drawn in the current view.
	 *
	 * @param View - View to test against.
	 * @return True, if this splat should be drawn.
	 */
	bool IsVisible(const FSceneView& View) const;

	/**
	 * Enqueues a CPU sort of the splats, if not already active.
	 *
	 * @param OriginCM - Origin to sort relative to, in centimeters.
	 * @param Forward - Forward direction of view, normalized.
	 */
	void TryEnqueueSort(const FVector3f& OriginCM, const FVector3f& Forward);

	/**
	 * @return SRV for the color buffer.
	 */
	FShaderResourceViewRHIRef GetColorsSRV() const
	{
		check(Asset);
		return Asset->GetColorsSRV();
	}

	/**
	 * @return SRV for the covariance matrix buffer.
	 */
	FShaderResourceViewRHIRef GetCovariancesSRV() const
	{
		check(Asset);
		return Asset->GetCovariancesSRV();
	}

	/**
	 * Gets the active index buffer SRV. This works for both CPU and GPU
	 * sorting.
	 *
	 * @return SRV for the index buffer.
	 */
	FShaderResourceViewRHIRef GetIndicesSRV() const
	{
		if (bIsSortingOnGPU)
		{
			check(Indices);
			check(Indices->ShaderResourceViewRHI);
			return Indices->ShaderResourceViewRHI;
		}
		else
		{
			check(CPUSorting);
			return CPUSorting->GetIndicesSRV();
		}
	}

	/**
	 * Get position SRV, and associated element-wise minimum and scaling. These
	 * vectors are needed for the GPU to reconstruct the real value.
	 *
	 * @param OutPosMinCM - Returns the element-wise minimum, in centimeters.
	 * @param OutPosScaleCM - Returns the scaling factor, in centimeters.
	 * @return SRV for the position buffer.
	 */
	FShaderResourceViewRHIRef
	GetPositionsSRV(FVector3f& OutPosMinCM, FVector3f& OutPosScaleCM) const
	{
		check(Asset);
		return Asset->GetPositionsSRV(OutPosMinCM, OutPosScaleCM);
	}

	/**
	 * @return SRV for the transform buffer.
	 */
	FShaderResourceViewRHIRef GetTransformsSRV() const
	{
		check(Transforms.ShaderResourceViewRHI);
		return Transforms.ShaderResourceViewRHI;
	}

	/**
	 * Gets the UAV for the index buffer. *Must* be using GPU sorting.
	 *
	 * @return UAV for the index buffer.
	 */
	FUnorderedAccessViewRHIRef GetIndicesUAV() const
	{
		check(bIsSortingOnGPU);
		check(Indices);
		check(Indices->UnorderedAccessViewRHI);
		return Indices->UnorderedAccessViewRHI;
	}

	/**
	 * @return UAV for the transform buffer.
	 */
	FUnorderedAccessViewRHIRef GetTransformsUAV() const
	{
		check(Transforms.UnorderedAccessViewRHI);
		return Transforms.UnorderedAccessViewRHI;
	}

	/**
	 * Gets a user-friendly name for this proxy, for debugging.
	 *
	 * @return Name of this proxy.
	 */
	FString GetName() const { return Name; }

	/**
	 * @return Reference to fake RDG buffer for this proxy's indices.
	 */
	FRDGBufferRef& GetIndicesFake() { return IndicesFake; }

	/**
	 * @return Reference to fake RDG buffer for this proxy's distances.
	 */
	FRDGBufferRef& GetDistancesFake() { return DistancesFake; }

	/**
	 * @return Whether this proxy needs to have its indices sorted for the first
	 * time, and therefore shouldn't be drawn yet.
	 */
	bool NeedsSort()
	{
		check(bIsSortingOnGPU || CPUSorting);
		return !bIsSortingOnGPU && !CPUSorting->IsGPUBufferReady();
	}

private:
	TObjectPtr<USplatAsset> Asset;
	FSplatGPUToGPUBuffer Transforms;

	bool bIsSortingOnGPU;
	std::optional<FSplatGPUToGPUBuffer> Indices;

	// This is a shared_ptr, as while this proxy "owns" the CPU sorting data, it
	// may be outlived by the sorting task and/or GPU copy command. In either
	// case, we need to keep this data around past the lifetime of the proxy in.
	// If this outlives the proxy:
	//   - Render thread resources will be cleaned up by the sorting task.
	//   - All other resources will be destroyed automatically by whichever of the
	//     sorting task or the copy command completes later.
	std::shared_ptr<FMultithreadedSortingBuffers> CPUSorting;

	FRDGBufferRef IndicesFake;
	FRDGBufferRef DistancesFake;

	FString Name;

#if WITH_EDITOR
	uint32 NumConvexHullTris;
	FStaticMeshVertexBuffers VertexBuffers;
	FDynamicMeshIndexBuffer32 IndexBuffer;
	FLocalVertexFactory VertexFactory;
	UBodySetup* BodySetup;
#endif
};

} // namespace PICO::Splat