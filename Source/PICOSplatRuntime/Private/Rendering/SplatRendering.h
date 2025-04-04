/*
  Copyright (c) 2025 PICO Technology Co., Ltd. See LICENSE.md.
*/

#pragma once

#include "RHICommandList.h"
#include "RenderGraphBuilder.h"
#include "SceneView.h"
#include "SplatSceneProxy.h"
#include "SplatShaders.h"

namespace PICO::Splat
{

// TODO(seth): CPU/GPU render handling should be merged.

BEGIN_SHADER_PARAMETER_STRUCT(FRenderSplatCPUSortDeps, )
SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint2>, Indices) // (Index, Distance)
SHADER_PARAMETER_STRUCT_INCLUDE(
	PICO::Splat::Shaders::FRenderSplatVS<
		PICO::Splat::Shaders::ESortingDevice::CPU>::FParameters,
	VS)
SHADER_PARAMETER_STRUCT_INCLUDE(
	PICO::Splat::Shaders::FRenderSplatPS::FParameters, PS)
END_SHADER_PARAMETER_STRUCT()

BEGIN_SHADER_PARAMETER_STRUCT(FRenderSplatGPUSortDeps, )
SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint>, Indices)
SHADER_PARAMETER_STRUCT_INCLUDE(
	PICO::Splat::Shaders::FRenderSplatVS<
		PICO::Splat::Shaders::ESortingDevice::GPU>::FParameters,
	VS)
SHADER_PARAMETER_STRUCT_INCLUDE(
	PICO::Splat::Shaders::FRenderSplatPS::FParameters, PS)
END_SHADER_PARAMETER_STRUCT()

/**
 * Adds distance calculation compute shader pass.
 *
 * @param GraphBuilder - Graph to add pass to.
 * @param View - View to measure distance from.
 * @param Proxy - Splat proxy to measure.
 * @param Indices - Output buffer, populated with the index of each splat.
 * @param Distances - Output buffer, populated with the distance to each splat.
 * @return A reference to the added pass.
 */
FRDGPassRef CalculateDistances(
	FRDGBuilder& GraphBuilder,
	const FSceneView& View,
	FSplatSceneProxy* Proxy,
	FRDGBufferRef Indices,
	FRDGBufferRef Distances);

/**
 * Adds transform calculation compute shader pass.
 *
 * @param GraphBuilder - Graph to add pass to.
 * @param View - View to transform relative to.
 * @param Proxy - Splat proxy to transform.
 * @return A reference to the added pass.
 */
FRDGPassRef ComputeTransforms(
	FRDGBuilder& GraphBuilder, const FSceneView& View, FSplatSceneProxy* Proxy);

/**
 * Draws a splat, sorted by CPU.
 *
 * @param RHICmdList - Command list to write to.
 * @param SplatParameters - Parameters for draw.
 * @param NumSplats - Number of splats to draw.
 * @param View - View to draw for.
 */
void RenderSplatCPUSort(
	FRHICommandList& RHICmdList,
	FRenderSplatCPUSortDeps* SplatParameters,
	uint32 NumSplats,
	const FSceneView& View);

/**
 * Draws a splat, sorted by GPU.
 *
 * @param RHICmdList - Command list to write to.
 * @param SplatParameters - Parameters for draw.
 * @param NumSplats - Number of splats to draw.
 * @param View - View to draw for.
 */
void RenderSplatGPUSort(
	FRHICommandList& RHICmdList,
	FRenderSplatGPUSortDeps* SplatParameters,
	uint32 NumSplats,
	const FSceneView& View);

/**
 * Adds GPU sorting pass.
 *
 * @param GraphBuilder - Graph to add pass to.
 * @param View - View sorting is relative to.
 * @param Proxy - Splat proxy to sort.
 * @param Indices - In/out buffer, sorted by matching distance.
 * @param Distances - In/out buffer, sorted.
 * @return A reference to the added pass.
 */
FRDGPassRef SortSplats(
	FRDGBuilder& GraphBuilder,
	const FSceneView& View,
	FSplatSceneProxy* Proxy,
	FRDGBufferRef Indices,
	FRDGBufferRef Distances);

} // namespace PICO::Splat