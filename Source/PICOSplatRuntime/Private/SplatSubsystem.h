/*
  Copyright (c) 2025 PICO Technology Co., Ltd. See LICENSE.md.
*/

#pragma once

#include "Misc/AssertionMacros.h"
#include "Rendering/SplatSceneProxy.h"
#include "Rendering/SplatSceneViewExtension.h"
#include "Subsystems/EngineSubsystem.h"

#include "SplatSubsystem.generated.h"

/**
 * Enables 3DGS rendering in the Engine.
 */
UCLASS()
class USplatSubsystem final : public UEngineSubsystem
{
	GENERATED_BODY()

public:
	/**
	 * Enables `FSplatSceneViewExtension`.
	 */
	virtual void Initialize(FSubsystemCollectionBase& Collection) override
	{
		Extension = FSceneViewExtensions::NewExtension<
			PICO::Splat::FSplatSceneViewExtension>();
	}

	/**
	 * Forwards a splat to the rendering extension. The splat will continue to
	 * draw until a subsequent call to `UnregisterSplat_RenderThread`.
	 *
	 * @param Splat - The `FSplatSceneProxy` for the splat to render.
	 */
	void RegisterSplat_RenderThread(PICO::Splat::FSplatSceneProxy* Splat)
	{
		check(Extension);
		Extension->RegisterSplat_RenderThread(Splat);
	}

	/**
	 * Stops rendering of a splat previously registered with
	 * `RegisterSplat_RenderThread`.
	 *
	 * @param Splat - The splat to stop rendering.
	 */
	void UnregisterSplat_RenderThread(PICO::Splat::FSplatSceneProxy* Splat)
	{
		check(Extension);
		Extension->UnregisterSplat_RenderThread(Splat);
	}

private:
	TSharedPtr<PICO::Splat::FSplatSceneViewExtension, ESPMode::ThreadSafe>
		Extension;
};