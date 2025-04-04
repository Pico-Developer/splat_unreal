/*
  Copyright (c) 2025 PICO Technology Co., Ltd. See LICENSE.md.
*/

#pragma once

#include <optional>

#include "Containers/Array.h"
#include "PackedTypes.h"
#include "RenderCommandFence.h"
#include "Rendering/SplatBuffers.h"
#include "UObject/Object.h"

#include "SplatAsset.generated.h"

/**
 * Container for imported 3DGS scene/model data.
 * Owns CPU data, and handles loading and unloading of GPU data.
 *
 * @see https://dev.epicgames.com/documentation/en-us/unreal-engine/threaded-rendering-in-unreal-engine#staticresources
 */
UCLASS()
class PICOSPLATRUNTIME_API USplatAsset : public UObject
{
	GENERATED_BODY()

public:
	//~ Begin UObject Interface
	virtual void BeginDestroy() override;
	virtual bool IsReadyForFinishDestroy() override;
	virtual void PostLoad() override; // Loading from disk only.
	virtual void Serialize(FArchive& Ar) override;
	//~ End UObject Interface

	/**
	 * @return SRV for this asset's colors.
	 */
	FShaderResourceViewRHIRef GetColorsSRV() const
	{
		check(Colors);
		check(Colors->ShaderResourceViewRHI);
		return Colors->ShaderResourceViewRHI;
	}

	/**
	 * Gets the indices of this asset's convex hull.
	 *
	 * @return Constant view of hull indices.
	 */
	TConstArrayView<uint32> GetConvexHullIndices() const
	{
		return ConvexHullIndices;
	}

	/**
	 * Gets the vertices of this asset's convex hull.
	 *
	 * @return Constant view of hull vertices.
	 */
	TConstArrayView<FVector3f> GetConvexHullVertices() const
	{
		return ConvexHullVertices;
	}

	/**
	 * @return SRV for this asset's covariance matrices.
	 */
	FShaderResourceViewRHIRef GetCovariancesSRV() const
	{
		check(CovariancesCM);
		check(CovariancesCM->ShaderResourceViewRHI);
		return CovariancesCM->ShaderResourceViewRHI;
	}

	/**
	 * @return The number of splats in this asset.
	 */
	uint32 GetNumSplats() const { return NumSplats; }

	/**
	 * @return Constant view of this asset's positions.
	 */
	TConstArrayView<FVector3f> GetPositions() const
	{
		return PositionsFullPrecision;
	}

	/**
	 * Gets this assets positions, alongside element-wise minimum and scaling.
	 *
	 * @param OutPosMinCM - Element-wise minimum, in centimeters.
	 * @param OutPosScaleCM - Element-wise scale, in centimeters.
	 * @return SRV for this asset's packed positions.
	 */
	FShaderResourceViewRHIRef
	GetPositionsSRV(FVector3f& OutPosMinCM, FVector3f& OutPosScaleCM) const
	{
		check(Positions);
		check(Positions->ShaderResourceViewRHI);
		OutPosMinCM = PosMinCM;
		OutPosScaleCM = PosScaleCM;
		return Positions->ShaderResourceViewRHI;
	}

#if WITH_EDITOR
	/**
	 * Populates this asset with the given colors.
	 *
	 * @param ColorsLinear - Array of linear, 8-bit-per-channel colors.
	 */
	void SetColorsLinear(TArray<FColor>&& ColorsLinear)
	{
		check(ColorsLinear.Num() == NumSplats);

		TStaticMeshVertexData<FColor> Data;
		Data.Assign(ColorsLinear);
		Colors = PICO::Splat::TSplatStaticBuffer(std::move(Data));
	}

	/**
	 * Populates this asset with covariance matrices describing the given
	 * rotations and scales.
	 *
	 * @param Rotations - Array of rotations, one per splat.
	 * @param ScalesMeters - Array of scales, one per splat, in meters.
	 */
	void SetCovariancesQuatScaleMeters(
		const TArray<FQuat4f>& Rotations,
		const TArray<FVector3f>& ScalesMeters);

	/**
	 * Sets the number of splats in the asset.
	 *
	 * @param InNumSplats - Number of splats.
	 */
	void SetNumSplats(uint32 InNumSplats) { NumSplats = InNumSplats; }

	/**
	 * Populates this asset with the given positions. If sorting on CPU, this
	 * buffer will be kept around under the class is destroyed.
	 *
	 * @param PositionsMeters - An array of positions, one per splat, in meters.
	 */
	void SetPositionsMeters(TArray<FVector3f>&& PositionsMeters);
#endif

private:
	/**
	 * Enqueues RHI initialization for all resources.
	 */
	void BeginInit();

	/**
	 * Creates packed position data from an array of positions. Does not copy or
	 * destroy the given buffer.
	 *
	 * @param PositionsMeters - An array of positions, one per splat, in meters.
	 */
	void SetPositionsMetersInternal(const TArray<FVector3f>& PositionsMeters);

	uint32 NumSplats = 0;

	TArray<FVector3f> PositionsFullPrecision;
	FVector3f PosMinCM;
	FVector3f PosMaxCM;
	FVector3f PosScaleCM;

	/**
	 * Note: Using optionals as these are not populated until after the
	 * asset is constructed. This way, at least these buffers can always be
	 * valid post-construction.
	 */
	std::optional<PICO::Splat::TSplatStaticBuffer<PICO::Splat::FPackedPos>>
		Positions;
	std::optional<PICO::Splat::TSplatStaticBuffer<PICO::Splat::FPackedCovMat>>
		CovariancesCM;
	std::optional<PICO::Splat::TSplatStaticBuffer<FColor>> Colors;

	TArray<FVector3f> ConvexHullVertices;
	TArray<uint32> ConvexHullIndices;

	FRenderCommandFence ReleaseResourcesFence;

#if WITH_EDITOR
	friend class USplatAssetFactory;
#endif
};