/*
  Copyright (c) 2025 PICO Technology Co., Ltd. See LICENSE.md.
*/

#include "SplatSceneViewExtension.h"

#include "Logging.h"
#include "PostProcess/PostProcessing.h"
#include "SplatRendering.h"
#include "SplatRenderingUtilities.h"
#include "SplatSettings.h"
#include "StereoRendering.h"

namespace PICO::Splat
{
namespace
{
/**
 * See comment in SplatRendering.cpp.
 */
BEGIN_SHADER_PARAMETER_STRUCT(FCPUSortRenderProducerParameters, )
SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint2>, IndicesUAV)
END_SHADER_PARAMETER_STRUCT()

Shaders::FRenderSplatSharedParameters
SetSharedParameters(const FSceneView& View, FSplatSceneProxy* Proxy)
{
	check(Proxy);

	Shaders::FRenderSplatSharedParameters Params;
	Params.View = View.ViewUniformBuffer;
	Params.InstancedView = View.GetInstancedViewUniformBuffer();
	Params.local_to_world = FMatrix44f(Proxy->GetLocalToWorld());
	Params.Positions = MakePositionParams(Proxy);
	Params.transforms = Proxy->GetTransformsSRV();
	Params.colors = Proxy->GetColorsSRV();

	return Params;
}
} // namespace

FSplatSceneViewExtension::FSplatSceneViewExtension(
	const FAutoRegister& AutoRegister)
	: FSceneViewExtensionBase(AutoRegister)
	, bIsSortingOnGPU(USplatSettings::IsSortingOnGPU())
	, Proxies()
{
	FSceneViewExtensionIsActiveFunctor IsActiveFunctor;
	IsActiveFunctor.IsActiveFunction =
		[](const ISceneViewExtension* SceneViewExtension,
	       const FSceneViewExtensionContext& Context)
	{
		check(SceneViewExtension);
		const FSplatSceneViewExtension* SplatSceneViewExtension =
			static_cast<const FSplatSceneViewExtension*>(SceneViewExtension);

		return TOptional<bool>(SplatSceneViewExtension->Proxies.Num() > 0);
	};

	IsActiveThisFrameFunctions.Add(IsActiveFunctor);
}

void FSplatSceneViewExtension::PreRenderView_RenderThread(
	FRDGBuilder& GraphBuilder, FSceneView& View)
{
	/**
	 * Full & primary passes do actual splat calculations, which are shared with
	 * secondary passes (if applicable).
	 *
	 * Full pass: Non-stereo.
	 * Primary: First eye, or both (e.g. instanced stereo or multiview).
	 * Secondary: Second eye.
	 */
	if (IStereoRendering::IsASecondaryView(View))
	{
		return;
	}

	for (auto& Proxy : Proxies)
	{
		check(Proxy);

		if (!Proxy->IsVisible(View))
		{
			continue;
		}

		uint32 NumSplats = Proxy->GetNumSplats();

		FRDGPassRef ProjPass = ComputeTransforms(GraphBuilder, View, Proxy);

		if (bIsSortingOnGPU)
		{
			FRDGBufferDesc IndexDesc =
				FRDGBufferDesc::CreateBufferDesc(sizeof(uint32), NumSplats);
			Proxy->GetIndicesFake() =
				GraphBuilder.CreateBuffer(IndexDesc, TEXT("Indices"));

			FRDGBufferDesc DistanceDesc =
				FRDGBufferDesc::CreateBufferDesc(sizeof(uint16), NumSplats);
			Proxy->GetDistancesFake() =
				GraphBuilder.CreateBuffer(DistanceDesc, TEXT("Distances"));

			FRDGPassRef DistPass = CalculateDistances(
				GraphBuilder,
				View,
				Proxy,
				Proxy->GetIndicesFake(),
				Proxy->GetDistancesFake());

			FRDGPassRef SortPass = SortSplats(
				GraphBuilder,
				View,
				Proxy,
				Proxy->GetIndicesFake(),
				Proxy->GetDistancesFake());
		}
		else
		{
			FRDGBufferDesc IndexDesc =
				FRDGBufferDesc::CreateBufferDesc(sizeof(uint32), NumSplats);
			Proxy->GetIndicesFake() = GraphBuilder.CreateBuffer(
				IndexDesc, TEXT("IndicesWithDistances"));

			Proxy->TryEnqueueSort(GetOrigin(View), GetForward(View));
		}
	}
}

void FSplatSceneViewExtension::PrePostProcessPass_RenderThread(
	FRDGBuilder& GraphBuilder,
	const FSceneView& View,
	const FPostProcessingInputs& Inputs)
{
	for (auto& Proxy : Proxies)
	{
		check(Proxy);

		if (!Proxy->IsVisible(View))
		{
			continue;
		}
		if (Proxy->NeedsSort())
		{
			continue;
		}

		if (!bIsSortingOnGPU)
		{
			FCPUSortRenderProducerParameters* SetupParameters =
				GraphBuilder
					.AllocParameters<FCPUSortRenderProducerParameters>();
			SetupParameters->IndicesUAV =
				GraphBuilder.CreateUAV(Proxy->GetIndicesFake(), PF_R32G32_UINT);

			GraphBuilder.AddPass(
				RDG_EVENT_NAME("Splat: RDG Producer"),
				SetupParameters,
				ERDGPassFlags::Compute,
				[](FRHIComputeCommandList& RHICmdList) {});
		}

		Shaders::FRenderSplatSharedParameters Shared =
			SetSharedParameters(View, Proxy);
		Shaders::FRenderSplatPS::FParameters ParamsPS;
		check(Inputs.SceneTextures);
		ParamsPS.RenderTargets[0] = FRenderTargetBinding(
			(*Inputs.SceneTextures)->SceneColorTexture,
			ERenderTargetLoadAction::ELoad);
		ParamsPS.RenderTargets.DepthStencil = FDepthStencilBinding(
			(*Inputs.SceneTextures)->SceneDepthTexture,
			ERenderTargetLoadAction::ELoad,
			FExclusiveDepthStencil::DepthWrite_StencilNop);

		if (bIsSortingOnGPU)
		{
			FRenderSplatGPUSortDeps* PassParameters =
				GraphBuilder.AllocParameters<FRenderSplatGPUSortDeps>();

			PassParameters->Indices =
				GraphBuilder.CreateSRV(Proxy->GetIndicesFake(), PF_R32_UINT);
			PassParameters->VS.Shared = Shared;
			PassParameters->VS.Indices = Proxy->GetIndicesSRV();
			PassParameters->PS = ParamsPS;

			GraphBuilder.AddPass(
				RDG_EVENT_NAME("Splat: Render %s", *Proxy->GetName()),
				PassParameters,
				ERDGPassFlags::Raster,
				[this, PassParameters, Proxy, &View](
					FRHICommandList& RHICmdList)
				{
					RenderSplatGPUSort(
						RHICmdList,
						PassParameters,
						Proxy->GetNumSplats(),
						View);
				});
		}
		else
		{
			FRenderSplatCPUSortDeps* PassParameters =
				GraphBuilder.AllocParameters<FRenderSplatCPUSortDeps>();

			PassParameters->Indices =
				GraphBuilder.CreateSRV(Proxy->GetIndicesFake(), PF_R32G32_UINT);
			PassParameters->VS.Shared = Shared;
			PassParameters->VS.Indices =
				Proxy->GetIndicesSRV(); // (Index, Distance).
			PassParameters->PS = ParamsPS;

			GraphBuilder.AddPass(
				RDG_EVENT_NAME("Splat: Render %s", *Proxy->GetName()),
				PassParameters,
				ERDGPassFlags::Raster,
				[this, PassParameters, Proxy, &View](
					FRHICommandList& RHICmdList)
				{
					RenderSplatCPUSort(
						RHICmdList,
						PassParameters,
						Proxy->GetNumSplats(),
						View);
				});
		}
	}
}

void FSplatSceneViewExtension::PostRenderBasePassMobile_RenderThread(
	FRHICommandList& RHICmdList, FSceneView& InView)
{
	for (auto& Proxy : Proxies)
	{
		check(Proxy);

		if (!Proxy->IsVisible(InView))
		{
			continue;
		}
		if (Proxy->NeedsSort())
		{
			continue;
		}

		Shaders::FRenderSplatSharedParameters Shared =
			SetSharedParameters(InView, Proxy);

		SCOPED_DRAW_EVENTF(
			RHICmdList,
			RenderSplat,
			TEXT("Splat: Render %s"),
			*Proxy->GetName());
		if (bIsSortingOnGPU)
		{
			FRenderSplatGPUSortDeps Parameters{};
			Parameters.VS.Shared = Shared;
			Parameters.VS.Indices = Proxy->GetIndicesSRV();
			RenderSplatGPUSort(
				RHICmdList, &Parameters, Proxy->GetNumSplats(), InView);
		}
		else
		{
			FRenderSplatCPUSortDeps Parameters{};
			Parameters.VS.Shared = Shared;
			Parameters.VS.Indices = Proxy->GetIndicesSRV();
			RenderSplatCPUSort(
				RHICmdList, &Parameters, Proxy->GetNumSplats(), InView);
		}
	}
}

} // namespace PICO::Splat