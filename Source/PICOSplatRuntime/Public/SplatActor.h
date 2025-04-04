/*
  Copyright (c) 2025 PICO Technology Co., Ltd. See LICENSE.md.
*/

#pragma once

#include "GameFramework/Actor.h"
#include "SplatAsset.h"
#include "SplatComponent.h"

#include "SplatActor.generated.h"

/**
 * Placeable object representing a 3DGS model or scene.
 * Holds a renderable `USplatComponent`.
 *
 * @see https://dev.epicgames.com/documentation/en-us/unreal-engine/actors-in-unreal-engine
 */
UCLASS(ComponentWrapperClass)
class PICOSPLATRUNTIME_API ASplatActor : public AActor
{
	GENERATED_BODY()

public:
	/**
	 * Creates a Splat Actor with a default Splat Component, holding no asset.
	 */
	ASplatActor()
	{
		SplatComponent =
			CreateDefaultSubobject<USplatComponent>(TEXT("SplatComponent"));
		RootComponent = SplatComponent;
	}

private:
	UPROPERTY(
		BlueprintReadOnly,
		Category = Splat,
		VisibleAnywhere,
		meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USplatComponent> SplatComponent;

#if WITH_EDITOR
	friend class UActorFactorySplat;
#endif
};