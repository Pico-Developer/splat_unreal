/*
  Copyright (c) 2025 PICO Technology Co., Ltd. See LICENSE.md.
*/

#pragma once

#include "Containers/Set.h"
#include "Misc/AssertionMacros.h"
#include "SceneViewExtension.h"
#include "SplatSceneProxy.h"

namespace PICO::Splat
{

/**
 * Extends the Engine's rendering system to support 3DGS.
 *
 * Splits splat rendering into two phases:
 * 1. Distancing, sorting and projection, which kicks off before rendering the
 * 		current view.
 * 2. Actual rendering, which happens after the base pass or before
 * 	  post-processing, depending on whether the renderer is desktop or mobile.
 */
class FSplatSceneViewExtension final : public FSceneViewExtensionBase
{
public:
	FSplatSceneViewExtension(const FAutoRegister& AutoRegister);

	//~ Begin ISceneViewExtension Interface
	virtual void SetupViewFamily(FSceneViewFamily& InViewFamily) override {};
	virtual void
	SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView) override {};
	virtual void
	BeginRenderViewFamily(FSceneViewFamily& InViewFamily) override {};

	/**
	 * First stage: Enqueue async compute work, to be done before actual
	 * rendering.
	 *
	 * 1. Measure distance to each splat (if GPU sort enabled).
	 * 2. Sort splats by distance (if GPU sort enabled).
	 * 3. Project splats (calculate 2x2 transform).
	 */
	virtual void PreRenderView_RenderThread(
		FRDGBuilder& GraphBuilder, FSceneView& InView) override;

	/**
	 * Second stage, on desktop renderer. Transforms and renders splats based on
	 * output from first stage.
	 *
	 * Must occur *after* lighting, as when using deferred rendering,
	 * transparent edges will pull in black from the unlit SceneColor.
	 */
	virtual void PrePostProcessPass_RenderThread(
		FRDGBuilder& GraphBuilder,
		const FSceneView& View,
		const FPostProcessingInputs& Inputs) override;

	/**
	 * Second stage, on mobile renderer.
	 */
	virtual void PostRenderBasePassMobile_RenderThread(
		FRHICommandList& RHICmdList, FSceneView& InView) override;
	//~ End ISceneViewExtension Interface

	/**
	 * Registers a splat for rendering. Continues until a subsequent call to
	 * `UnregisterSplat_RenderThread`.
	 *
	 * @param Proxy - The splat to begin rendering.
	 */
	void RegisterSplat_RenderThread(FSplatSceneProxy* Proxy)
	{
		check(IsInRenderingThread());
		Proxies.Add(Proxy);
	}

	/**
	 * Stop rendering a splat.
	 *
	 * @param Proxy - The splat to stop rendering.
	 */
	void UnregisterSplat_RenderThread(FSplatSceneProxy* Proxy)
	{
		check(IsInRenderingThread());
		Proxies.Remove(Proxy);
	}

private:
	bool bIsSortingOnGPU;
	TSet<FSplatSceneProxy*> Proxies;
};

} // namespace PICO::Splat