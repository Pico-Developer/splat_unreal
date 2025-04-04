/*
  Copyright (c) 2025 PICO Technology Co., Ltd. See LICENSE.md.
*/

#pragma once

#include <memory>
#include <optional>

#include "Logging.h"
#include "PackedTypes.h"
#include "RHICommandList.h"
#include "RHIResources.h"
#include "RenderResource.h"
#include "StaticMeshVertexData.h"

namespace PICO::Splat
{
/**
 * Base class for all splat GPU resource buffers.
 * Do not use this directly, and instead chose a child class.
 */
class FSplatBufferBase : public FVertexBufferWithSRV
{
public:
	//~ Begin FRenderResource Interface
	virtual void InitRHI(FRHICommandListBase& RHICmdList) override;
	//~ End FRenderResource Interface

protected:
	FSplatBufferBase(
		uint32 NumSplats,
		EPixelFormat InFormat,
		bool bInNeedsUAV,
		ERHIAccess InState,
		EBufferUsageFlags InUsage)
		: ResourceArray(nullptr)
		, Format(InFormat)
		, bNeedsUAV(bInNeedsUAV)
		, State(InState)
		, Stride(GPixelFormats[InFormat].BlockBytes)
		, Usage(InUsage)
		, Size(NumSplats * Stride)
	{
	}

	FResourceArrayInterface* ResourceArray;

private:
	EPixelFormat Format;
	bool bNeedsUAV;
	ERHIAccess State;
	uint32 Stride;
	EBufferUsageFlags Usage;
	uint32 Size;
};

/**
 * A GPU resource buffer for data written by CPU and read by GPU.
 */
class FSplatCPUToGPUBuffer final : public FSplatBufferBase
{
public:
	/**
	 * Create buffer.
	 *
	 * @param NumSplats - Size of this buffer in elements.
	 * @param InFormat - GPU buffer format to use.
	 */
	FSplatCPUToGPUBuffer(uint32 NumSplats, EPixelFormat InFormat)
		: FSplatBufferBase(
			  NumSplats,
			  InFormat,
			  false,
			  ERHIAccess::SRVGraphics,
			  EBufferUsageFlags::Dynamic |
				  EBufferUsageFlags::KeepCPUAccessible |
				  EBufferUsageFlags::ShaderResource)
	{
	}

	// Move-only.
	~FSplatCPUToGPUBuffer() = default;
	FSplatCPUToGPUBuffer(const FSplatCPUToGPUBuffer&) = delete;
	FSplatCPUToGPUBuffer(FSplatCPUToGPUBuffer&& Buffer) = default;
	FSplatCPUToGPUBuffer& operator=(const FSplatCPUToGPUBuffer&) = delete;
	FSplatCPUToGPUBuffer& operator=(FSplatCPUToGPUBuffer&&) = default;

	//~ Begin FRenderResource Interface
	virtual FString GetFriendlyName() const override
	{
		return TEXT("FSplatCPUToGPUBuffer");
	}
	//~ End FRenderResource Interface
};

/**
 * A GPU resource buffer for intermediates between GPU stages (e.g. compute output to vertex input).
 */
class FSplatGPUToGPUBuffer final : public FSplatBufferBase
{
public:
	/**
	 * Create buffer.
	 *
	 * @param NumSplats - Size of this buffer in elements.
	 * @param InFormat - GPU buffer format to use.
	 */
	FSplatGPUToGPUBuffer(uint32 NumSplats, EPixelFormat InFormat)
		: FSplatBufferBase(
			  NumSplats,
			  InFormat,
			  true,
			  ERHIAccess::UAVCompute,
			  EBufferUsageFlags::ShaderResource |
				  EBufferUsageFlags::UnorderedAccess)
	{
	}

	// Move-only.
	~FSplatGPUToGPUBuffer() = default;
	FSplatGPUToGPUBuffer(const FSplatGPUToGPUBuffer&) = delete;
	FSplatGPUToGPUBuffer(FSplatGPUToGPUBuffer&& Buffer) = default;
	FSplatGPUToGPUBuffer& operator=(const FSplatGPUToGPUBuffer&) = delete;
	FSplatGPUToGPUBuffer& operator=(FSplatGPUToGPUBuffer&&) = default;

	//~ Begin FRenderResource Interface
	virtual FString GetFriendlyName() const override
	{
		return TEXT("FSplatGPUToGPUBuffer");
	}
	//~ End FRenderResource Interface
};

/**
 * Helper to get the GPU format that will hold C++-defined types.
 */
template <typename T> EPixelFormat GetFormat()
{
	// 128 bits per splat.
	if constexpr (std::is_same_v<T, FVector4f>)
	{
		return PF_A32B32G32R32F;
	}
	// 64 bits per splat.
	else if (std::is_same_v<T, FPackedCovMat>)
	{
		// PF_R64_UINT doesn't work.
		return PF_R32G32_UINT;
	}
	// 32 bits per splat.
	else if (std::is_same_v<T, FColor>)
	{
		return PF_B8G8R8A8;
	}
	else if (std::is_same_v<T, FPackedPos>)
	{
		return PF_R32_UINT;
	}
}

/**
 * A GPU resource buffer for static resources, written by the CPU only at
 * creation time.
 */
template <typename T> class TSplatStaticBuffer final : public FSplatBufferBase
{
public:
	/**
	 * Create buffer.
	 *
	 * @param InData - Data to copy to GPU. This buffer will take ownership of
	 * InData, and release it later, either after RHI initialization or in the
	 * destructor.
	 */
	TSplatStaticBuffer(TStaticMeshVertexData<T>&& InData)
		: FSplatBufferBase(
			  InData.Num(),
			  GetFormat<T>(),
			  false,
			  ERHIAccess::SRVGraphics,
			  EBufferUsageFlags::Dynamic |
				  EBufferUsageFlags::KeepCPUAccessible |
				  EBufferUsageFlags::ShaderResource)
		, Data(std::make_unique<TStaticMeshVertexData<T>>(std::move(InData)))
	{
		ResourceArray = Data->GetResourceArray();
		check(ResourceArray);
	}

	// Move-only, with extra checking to avoid leaks.
	~TSplatStaticBuffer() = default;
	TSplatStaticBuffer(const TSplatStaticBuffer&) = delete;
	TSplatStaticBuffer(TSplatStaticBuffer&& Buffer) : FSplatBufferBase(Buffer)
	{
		check(Buffer.Data);
		Data = std::move(Buffer.Data);
	}
	TSplatStaticBuffer& operator=(const TSplatStaticBuffer&) = delete;
	TSplatStaticBuffer& operator=(TSplatStaticBuffer&& Buffer)
	{
		check(!Data);
		check(Buffer.Data);
		FSplatBufferBase::operator=(Buffer);
		Data = std::move(Buffer.Data);
		return *this;
	}

	//~ Begin FRenderResource Interface
	virtual FString GetFriendlyName() const override
	{
		return TEXT("FSplatStaticBuffer");
	}
	virtual void InitRHI(FRHICommandListBase& RHICmdList) override
	{
		FSplatBufferBase::InitRHI(RHICmdList);

		if (ResourceArray->GetResourceDataSize() == 0)
		{
			Data.reset();
		}
	}
	//~ End FRenderResource Interface

	/**
	 * Saves a buffer to or loads it from an archive.
	 *
	 * Note: This works on an optional, not a TSplatStaticBuffer. This is to
	 * avoid the need for a constructed-but-invalid TSplatStaticBuffer to be
	 * implemented (e.g. with a default constructor).
	 *
	 * @param Ar - The archive.
	 * @param Buffer - A reference to an optional to a buffer to save or load.
	 * When saving, this must be Some(), and when loading this should be None.
	 * On return, when loading, Buffer will be set to Some() holding the read
	 * buffer.
	 */
	friend FArchive&
	operator<<(FArchive& Ar, std::optional<TSplatStaticBuffer<T>>& Buffer)
	{
		if (Ar.IsSaving())
		{
			check(Buffer);
			check(Buffer->Data);

			Buffer->Data->Serialize(Ar);
		}
		else if (Ar.IsLoading())
		{
			check(!Buffer);
			// Note: If LocalData is called just Data, it triggers an erroneous
			// warning about the member variable Data being shadowed even though
			// this member function is static.
			TStaticMeshVertexData<T> LocalData;
			LocalData.Serialize(Ar);
			Buffer = TSplatStaticBuffer<T>(std::move(LocalData));
		}

		return Ar;
	}

private:
	std::unique_ptr<TStaticMeshVertexData<T>> Data;
};

} // namespace PICO::Splat