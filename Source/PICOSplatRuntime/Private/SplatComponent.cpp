/*
  Copyright (c) 2025 PICO Technology Co., Ltd. See LICENSE.md.
*/

#include "SplatComponent.h"

#include "Misc/AssertionMacros.h"
#include "Rendering/SplatSceneProxy.h"

#if WITH_EDITOR
#include "Engine.h"
#endif

using PICO::Splat::FSplatSceneProxy;

// Defined here to avoid needing scene proxy to be module public.
FPrimitiveSceneProxy* USplatComponent::CreateSceneProxy()
{
	// Note: Unreal expects a new here, and will handle deletion itself.
	return Asset ? new FSplatSceneProxy{*this} : nullptr;
}

UBodySetup* USplatComponent::GetBodySetup()
{
	if (!Asset)
	{
		return nullptr;
	}
	else if (!BodySetup)
	{
		BodySetup = NewObject<UBodySetup>();

		TConstArrayView<FVector3f> ConvexHullVertices =
			Asset->GetConvexHullVertices();

		FKConvexElem Convex;
		Convex.VertexData.AddUninitialized(ConvexHullVertices.Num());
		for (int32 Index = 0; Index < ConvexHullVertices.Num(); ++Index)
		{
			Convex.VertexData[Index] = FVector(ConvexHullVertices[Index]);
		}
		Convex.UpdateElemBox();

		FKAggregateGeom AggGeom;
		AggGeom.ConvexElems.Add(Convex);
		BodySetup->AddCollisionFrom(AggGeom);
	}

	return BodySetup;
}

#if WITH_EDITOR
void USplatComponent::GetUsedMaterials(
	TArray<UMaterialInterface*>& OutMaterials, bool bGetDebugMaterials) const
{
	if (bGetDebugMaterials)
	{
		check(GEngine);
		OutMaterials.Add(GEngine->GeomMaterial);
		OutMaterials.Add(GEngine->ShadedLevelColorationUnlitMaterial);
		OutMaterials.Add(GEngine->WireframeMaterial);
	}
}
#endif

FBoxSphereBounds
USplatComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	if (BodySetup)
	{
		FBoxSphereBounds NewBounds;
		BodySetup->AggGeom.CalcBoxSphereBounds(NewBounds, LocalToWorld);
		return NewBounds;
	}
	else
	{
		return FBoxSphereBounds(
			LocalToWorld.GetLocation(), FVector::ZeroVector, 0.f);
	}
}
