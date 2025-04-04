/*
  Copyright (c) 2025 PICO Technology Co., Ltd. See LICENSE.md.
*/

#pragma once

#include "Components/PrimitiveComponent.h"
#include "PhysicsEngine/BodySetup.h"
#include "SplatAsset.h"
#include "SplatComponent.generated.h"

/**
 * Component holding a renderable 3DGS model or scene.
 *
 * @see https://dev.epicgames.com/documentation/en-us/unreal-engine/components-in-unreal-engine
 *
 * TODO(seth): I haven't figured out why the BodyInstance's Physics Actor is not
 * being created successfully on device. Until this is resolved, physics won't
 * work on device.
 */
UCLASS()
class USplatComponent final : public UPrimitiveComponent
{
	GENERATED_BODY()

public:
	//~ Begin UPrimitiveComponent Interface
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	virtual UBodySetup* GetBodySetup() override;
#if WITH_EDITOR
	// Materials are only used in Editor, for mouse selection and debug views.
	virtual void GetUsedMaterials(
		TArray<UMaterialInterface*>& OutMaterials,
		bool bGetDebugMaterials = false) const override;
#endif
	//~ End UPrimitiveComponent Interface

	//~ Begin USceneComponent Interface
	virtual FBoxSphereBounds
	CalcBounds(const FTransform& LocalToWorld) const override;
#if WITH_EDITOR
	virtual bool ShouldCollideWhenPlacing() const override { return true; }
#endif
	//~ End USceneComponent Interface

	/**
	 * Gets the asset this component is tied to, if any.
	 *
	 * @return The asset attached to this component, or nullptr.
	 */
	TObjectPtr<USplatAsset> GetAsset() const { return Asset; }

private:
	UPROPERTY(Category = Splat, EditAnywhere)
	TObjectPtr<USplatAsset> Asset;

	UPROPERTY()
	TObjectPtr<UBodySetup> BodySetup;

#if WITH_EDITOR
	friend class UActorFactorySplat;
#endif
};