/*
  Copyright (c) 2025 PICO Technology Co., Ltd. See LICENSE.md.
*/

#pragma once

#include "Misc/AssertionMacros.h"
#include "SplatSceneProxy.h"
#include "SplatShaders.h"

namespace PICO::Splat
{

/**
 * TODO(seth): These functions are broken out, in part, to abstract special
 * handling for stereo rendering. For now, we grab all of our view data from the
 * first view, but we should actually be synthesizing data that covers both.
 */

/**
 * Get focal length for a view.
 * f = (w / 2) / tan(fov_x / 2)
 *
 * @param View - View to use.
 * @return Focal length, in pixels.
 */
inline float GetFocalLength(const FSceneView& View)
{
	return View.UnconstrainedViewRect.Width() /
	       (2.f * tanf(View.ViewMatrices.ComputeHalfFieldOfViewPerAxis().X));
}

/**
 * Get forward vector from view.
 *
 * @param View - View to use.
 * @return Forward vector.
 */
inline FVector3f GetForward(const FSceneView& View)
{
	return FVector3f(View.GetViewDirection()).GetSafeNormal();
}

/**
 * Get origin for view.
 *
 * @param View - View to use.
 * @return Origin point.
 */
inline FVector3f GetOrigin(const FSceneView& View)
{
	return FVector3f(View.ViewMatrices.GetViewOrigin());
}

/**
 * Gets the view matrix from a view.
 *
 * @param View - View to use.
 * @return View matrix.
 */
inline FMatrix GetView(const FSceneView& View)
{
	return View.ViewMatrices.GetViewMatrix();
}

/**
 * Get view projection matrix.
 *
 * @param View - View to use.
 * @return ViewProjection matrix.
 */
inline FMatrix GetViewProj(const FSceneView& View)
{
	return View.ViewMatrices.GetViewProjectionMatrix();
}

/**
 * Helper to make packed position parameters from a proxy. This is because
 * packed parameters are relative to a component-wise min and max, so they must
 * be sent to the GPU with an origin/offset and a scale, in order to be
 * reconstructed.
 *
 * @param Proxy - Proxy to get positions from.
 * @return Packed position paramters: Origin, scale and positions.
 */
inline Shaders::FPackedPositionParameters
MakePositionParams(FSplatSceneProxy* Proxy)
{
	check(Proxy);

	Shaders::FPackedPositionParameters Params;
	Params.positions =
		Proxy->GetPositionsSRV(Params.pos_min_cm, Params.pos_scale_cm);

	return Params;
}
} // namespace PICO::Splat