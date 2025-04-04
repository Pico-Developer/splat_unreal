/*
  Copyright (c) 2025 PICO Technology Co., Ltd. See LICENSE.md.
*/

#pragma once

#include "AssetDefinitionDefault.h"
#include "SplatAsset.h"
#include "SplatConstants.h"

#include "AssetDefinition_Splat.generated.h"

/**
 * Metadata about `USplatAsset` for Editor UI.
 */
UCLASS()
class UAssetDefinition_Splat final : public UAssetDefinitionDefault
{
	GENERATED_BODY()

public:
	//~ Begin UAssetDefinition Interface
	virtual FText GetAssetDisplayName() const override
	{
		return NSLOCTEXT(
			"AssetTypeActions", "AssetTypeActions_Splat", "Splat Asset");
	}
	virtual TSoftClassPtr<UObject> GetAssetClass() const override
	{
		return USplatAsset::StaticClass();
	}
	virtual FLinearColor GetAssetColor() const override
	{
		return PICO::Splat::EditorColor.ReinterpretAsLinear();
	}
	//~ End UAssetDefinition Interface
};

#undef LOCTEXT_NAMESPACE