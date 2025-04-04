/*
  Copyright (c) 2025 PICO Technology Co., Ltd. See LICENSE.md.
*/

#include "SplatShaders.h"

#define IMPLEMENT_TEMPLATED_GLOBAL_SHADER(                                     \
	ShaderClass, SourceFilename, FunctionName, Frequency)                      \
	IMPLEMENT_SHADER_TYPE(                                                     \
		template <>,                                                           \
		ShaderClass,                                                           \
		TEXT(SourceFilename),                                                  \
		TEXT(FunctionName),                                                    \
		Frequency)

namespace PICO::Splat::Shaders
{

IMPLEMENT_GLOBAL_SHADER(
	FComputeDistanceCS,
	"/Plugin/PICOSplat/Private/ComputeDistanceCS.usf",
	"main",
	SF_Compute);
IMPLEMENT_GLOBAL_SHADER(
	FComputeTransformCS,
	"/Plugin/PICOSplat/Private/ComputeTransformCS.usf",
	"main",
	SF_Compute);
IMPLEMENT_TEMPLATED_GLOBAL_SHADER(
	FRenderSplatVS<ESortingDevice::CPU>,
	"/Plugin/PICOSplat/Private/RenderSplatVS.usf",
	"main",
	SF_Vertex);
IMPLEMENT_TEMPLATED_GLOBAL_SHADER(
	FRenderSplatVS<ESortingDevice::GPU>,
	"/Plugin/PICOSplat/Private/RenderSplatVS.usf",
	"main",
	SF_Vertex);
IMPLEMENT_GLOBAL_SHADER(
	FRenderSplatPS,
	"/Plugin/PICOSplat/Private/RenderSplatPS.usf",
	"main",
	SF_Pixel);

} // namespace PICO::Splat::Shaders