/*
  Copyright (c) 2025 PICO Technology Co., Ltd. See LICENSE.md.
*/

#include "SplatAssetFactory.h"

#include "CompGeom/ConvexHull3.h"
#include "Logging.h"
#include "Misc/AssertionMacros.h"
#include "SplatConstants.h"
#include "import/ply/splat_ply_conversion.h"
#include "import/ply/splat_ply_parsing.h"

using namespace import;
using import::GetPropertyFn;
using import::Metadata;
using import::ParseSplatFn;
using import::ply::SplatParserPly;
using PICO::Splat::MetersToCentimeters;

namespace
{
void MaybeAddIndex(TMap<uint32, uint32>& IndexMap, uint32 Index)
{
	if (!IndexMap.Contains(Index))
	{
		IndexMap.Add(Index, IndexMap.Num());
	}
}

TMap<uint32, uint32>
RemapIndices(TConstArrayView<UE::Geometry::FIndex3i> Indices)
{
	TMap<uint32, uint32> IndexMap{};

	for (const auto& Index3 : Indices)
	{
		MaybeAddIndex(IndexMap, Index3.A);
		MaybeAddIndex(IndexMap, Index3.B);
		MaybeAddIndex(IndexMap, Index3.C);
	}

	return IndexMap;
}

bool GenerateConvexHull(
	TConstArrayView<FVector3f> Positions,
	TArray<FVector3f>& OutVertices,
	TArray<uint32>& OutIndices)
{
	UE::Geometry::TConvexHull3<float> ConvexHull{};
	bool Success = ConvexHull.Solve<FVector3f>(Positions);
	if (!Success)
	{
		PICO_LOGE("Failed to solve for convex hull.");
		return false;
	}

	TArray<UE::Geometry::FIndex3i> HullIndices = ConvexHull.MoveTriangles();

	// Convert indices to only reference vertices in hull.

	TMap<uint32, uint32> IndexMap = RemapIndices(HullIndices);

	OutIndices.SetNumUninitialized(HullIndices.Num() * 3);
	for (int32 Index = 0; Index < HullIndices.Num(); ++Index)
	{
		OutIndices[Index * 3 + 0] = IndexMap[HullIndices[Index].A];
		OutIndices[Index * 3 + 1] = IndexMap[HullIndices[Index].B];
		OutIndices[Index * 3 + 2] = IndexMap[HullIndices[Index].C];
	}

	OutVertices.SetNumUninitialized(IndexMap.Num());
	for (const auto& Pair : IndexMap)
	{
		OutVertices[Pair.Value] = MetersToCentimeters * Positions[Pair.Key];
	}

	return true;
}

} // namespace

USplatAssetFactory::USplatAssetFactory()
{
	SupportedClass = USplatAsset::StaticClass();

	Formats.Emplace(TEXT("ply;Gaussian splat"));
	bEditorImport = true;
}

UObject* USplatAssetFactory::FactoryCreateBinary(
	UClass* InClass,
	UObject* InParent,
	FName InName,
	EObjectFlags Flags,
	UObject* Context,
	const TCHAR* Type,
	const uint8*& Buffer,
	const uint8* BufferEnd,
	FFeedbackContext* Warn)
{
	PICO_LOGL("Loading splats from %s.", *InName.ToString());

	SplatParserPly Parser;

	Metadata PLYMetadata;
	std::span<const uint8_t> BufferView(Buffer, BufferEnd);
	if (!Parser.parse_metadata(BufferView, PLYMetadata))
	{
		PICO_LOGE("Failed to parse metadata from %s.", *InName.ToString());
		return nullptr;
	}

	if (!ply::validate_metadata(PLYMetadata))
	{
		PICO_LOGE("Invalid metadata for %s.", *InName.ToString());
		return nullptr;
	}

	TArray<FVector3f> Positions;
	Positions.SetNumUninitialized(PLYMetadata.num_splats);
	TArray<FQuat4f> Rotations;
	Rotations.SetNumUninitialized(PLYMetadata.num_splats);
	TArray<FVector3f> Scales;
	Scales.SetNumUninitialized(PLYMetadata.num_splats);
	TArray<FColor> Colors;
	Colors.SetNumUninitialized(PLYMetadata.num_splats);

	ParseSplatFn ParseSplat =
		[P = std::span<FVector3f>(&Positions[0], Positions.Num()),
	     R = std::span<FQuat4f>(&Rotations[0], Rotations.Num()),
	     S = std::span<FVector3f>(&Scales[0], Scales.Num()),
	     C = std::span<FColor>(&Colors[0], Colors.Num())](
			uint32_t Index, GetPropertyFn Get)
	{ ply::convert_splat<FVector3f, FQuat4f, FColor>(Index, Get, P, R, S, C); };

	if (!Parser.parse_data(ParseSplat))
	{
		PICO_LOGE("Failed to parse splats from %s.", *InName.ToString());
		return nullptr;
	}

	USplatAsset* Asset = NewObject<USplatAsset>(InParent, InName, Flags);
	Asset->SetNumSplats(PLYMetadata.num_splats);
	Asset->SetPositionsMeters(std::move(Positions));
	Asset->SetCovariancesQuatScaleMeters(Rotations, Scales);
	Asset->SetColorsLinear(std::move(Colors));

	if (!GenerateConvexHull(
			Asset->PositionsFullPrecision,
			Asset->ConvexHullVertices,
			Asset->ConvexHullIndices))
	{
		PICO_LOGE("Failed to generate convex hull for %s.", *InName.ToString());
		return nullptr;
	}

	Asset->BeginInit();

	return Asset;
}