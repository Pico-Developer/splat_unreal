/*
  Copyright (c) 2025 PICO Technology Co., Ltd. See LICENSE.md.
*/

#include "ActorFactorySplat.h"

#include "Misc/AssertionMacros.h"

bool UActorFactorySplat::CanCreateActorFrom(
	const FAssetData& AssetData, FText& OutErrorMsg)
{
	if (!AssetData.IsValid() ||
	    !AssetData.IsInstanceOf(USplatAsset::StaticClass()))
	{
		OutErrorMsg = NSLOCTEXT(
			"CanCreateActor",
			"NoSplatAsset",
			"A valid splat asset must be specified.");
		return false;
	}

	return true;
}

void UActorFactorySplat::PostSpawnActor(UObject* Asset, AActor* NewActor)
{
	ASplatActor* SplatActor = CastChecked<ASplatActor>(NewActor);
	check(SplatActor->SplatComponent);
	SplatActor->SplatComponent->Asset = CastChecked<USplatAsset>(Asset);
}