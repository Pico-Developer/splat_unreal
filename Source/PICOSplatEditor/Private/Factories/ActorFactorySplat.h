/*
  Copyright (c) 2025 PICO Technology Co., Ltd. See LICENSE.md.
*/

#pragma once

#include "ActorFactories/ActorFactory.h"
#include "SplatActor.h"

#include "ActorFactorySplat.generated.h"

/**
 * Creates `USplatActor`s from `USplatAsset`s in Editor.
 */
UCLASS()
class UActorFactorySplat final : public UActorFactory
{
	GENERATED_BODY()

public:
	UActorFactorySplat() { NewActorClass = ASplatActor::StaticClass(); }

	//~ Begin UActorFactory Interface
	virtual bool CanCreateActorFrom(
		const FAssetData& AssetData, FText& OutErrorMsg) override;
	virtual void PostSpawnActor(UObject* Asset, AActor* NewActor) override;
	//~ End UActorFactory Interface
};