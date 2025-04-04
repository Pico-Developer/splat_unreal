/*
  Copyright (c) 2025 PICO Technology Co., Ltd. See LICENSE.md.
*/

#include "SplatSceneProxy.h"

#include "MaterialDomain.h"
#include "Materials/MaterialRenderProxy.h"
#include "PackedTypes.h"
#include "SplatConstants.h"
#include "SplatSettings.h"
#include "SplatSubsystem.h"

#if WITH_EDITOR
#include "Materials/Material.h"
#include "SceneManagement.h"
#endif

namespace PICO::Splat
{

FSplatSceneProxy::FSplatSceneProxy(USplatComponent& Component)
	: FPrimitiveSceneProxy(&Component)
	, Asset(Component.GetAsset())
	, Transforms(Asset->GetNumSplats(), EPixelFormat::PF_FloatRGBA)
	, bIsSortingOnGPU(USplatSettings::IsSortingOnGPU())
#if WITH_EDITOR
	, VertexFactory(GetScene().GetFeatureLevel(), "FSplatSceneProxy")
	, BodySetup(Component.GetBodySetup())
#endif
{
	if (bIsSortingOnGPU)
	{
		Indices = FSplatGPUToGPUBuffer(
			Asset->GetNumSplats(), EPixelFormat::PF_R32_UINT);
	}
	else
	{
		CPUSorting = std::make_shared<FMultithreadedSortingBuffers>(
			Asset->GetNumSplats());
	}

#if WITH_EDITOR
	TConstArrayView<uint32> ConvexHullIndices = Asset->GetConvexHullIndices();
	TConstArrayView<FVector3f> ConvexHullVertices =
		Asset->GetConvexHullVertices();
	NumConvexHullTris = ConvexHullIndices.Num() / 3;

	TArray<FDynamicMeshVertex> OutVerts;
	for (int32 Index = 0; Index < ConvexHullVertices.Num(); ++Index)
	{
		OutVerts.Push(ConvexHullVertices[Index]);
	}
	// Enqueues RHI init for each buffer.
	VertexBuffers.InitFromDynamicVertex(&VertexFactory, OutVerts);

	IndexBuffer.Indices.SetNumUninitialized(ConvexHullIndices.Num());
	for (int32 Index = 0; Index < ConvexHullIndices.Num(); ++Index)
	{
		IndexBuffer.Indices[Index] = ConvexHullIndices[Index];
	}
	BeginInitResource(&IndexBuffer);

	Name = Component.GetOwner()->GetActorLabel();
#else
	Name = Component.GetOwner()->GetName();
#endif
}

#if WITH_EDITOR
FPrimitiveViewRelevance
FSplatSceneProxy::GetViewRelevance(const FSceneView* View) const
{
	FPrimitiveViewRelevance Result{};

	// Even with WITH_EDITOR guard, Unreal generally checks GIsEditor, so do the
	// same.
	if (GIsEditor)
	{
		// We always draw in Editor, as this is used to select the Splat Actor.
		Result.bDrawRelevance = IsShown(View);
		// Triggers a call to GetDynamicMeshElements().
		Result.bDynamicRelevance = true;
		// Enables Editor highlighting / selection outline.
		Result.bEditorStaticSelectionRelevance = (IsSelected() || IsHovered());
	}

	return Result;
}
#endif

void FSplatSceneProxy::CreateRenderThreadResources(
	FRHICommandListBase& RHICmdList)
{
	if (bIsSortingOnGPU)
	{
		Indices->InitRHI(RHICmdList);
	}
	else
	{
		CPUSorting->InitResources_RenderThread(RHICmdList);
	}
	Transforms.InitRHI(RHICmdList);

	check(GEngine);
	USplatSubsystem* Subsystem = GEngine->GetEngineSubsystem<USplatSubsystem>();
	check(Subsystem);
	Subsystem->RegisterSplat_RenderThread(this);
}

void FSplatSceneProxy::DestroyRenderThreadResources()
{
	check(GEngine);
	USplatSubsystem* Subsystem = GEngine->GetEngineSubsystem<USplatSubsystem>();
	check(Subsystem);
	Subsystem->UnregisterSplat_RenderThread(this);

	if (bIsSortingOnGPU)
	{
		Indices->ReleaseResource();
	}
	else
	{
		CPUSorting->ReleaseResources();
	}

	Transforms.ReleaseResource();

#if WITH_EDITOR
	VertexFactory.ReleaseResource();

	VertexBuffers.PositionVertexBuffer.ReleaseResource();
	VertexBuffers.StaticMeshVertexBuffer.ReleaseResource();
	VertexBuffers.ColorVertexBuffer.ReleaseResource();

	IndexBuffer.ReleaseResource();
#endif
}

#if WITH_EDITOR
void FSplatSceneProxy::GetDynamicMeshElements(
	const TArray<const FSceneView*>& Views,
	const FSceneViewFamily& ViewFamily,
	uint32 VisibilityMap,
	class FMeshElementCollector& Collector) const
{
	check(GEngine);
	check(BodySetup);

	if (GIsEditor)
	{
		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ++ViewIndex)
		{
			/**
			 * Collision Views.
			 *
			 * Collision: Show > Collision.
			 * CollisionPawn: View Mode > Player Collision.
			 * CollisionVisibility: View Mode > Visibility Collision.
			 */
			const bool bDrawPawnCollision =
				ViewFamily.EngineShowFlags.CollisionPawn;
			const bool bDrawVisCollision =
				ViewFamily.EngineShowFlags.CollisionVisibility;
			const bool bDrawCollisionOverlay =
				ViewFamily.EngineShowFlags.Collision;

			const bool bIsCollisionView = AllowDebugViewmodes() &&
			                              IsCollisionEnabled() &&
			                              bDrawPawnCollision;
			const bool bIsWireframeView =
				AllowDebugViewmodes() && ViewFamily.EngineShowFlags.Wireframe;

			if (bIsCollisionView)
			{
				FLinearColor SelectionColor =
					GetSelectionColor(EditorColor, IsSelected(), IsHovered());

				TObjectPtr<UMaterial> Material;

				// If overlay is active, collisions become wireframe.
				const bool bDrawSolid = !bDrawCollisionOverlay;
				if (bDrawSolid)
				{
					Material = GEngine->ShadedLevelColorationUnlitMaterial;
				}
				else
				{
					Material = GEngine->WireframeMaterial;
				}

				// Note: This will be registered for deletion within
				// RegisterOneFrameMaterialProxy().
				FColoredMaterialRenderProxy* CollisionMaterialInstance =
					new FColoredMaterialRenderProxy(
						Material->GetRenderProxy(), SelectionColor);
				Collector.RegisterOneFrameMaterialProxy(
					CollisionMaterialInstance);
				BodySetup->AggGeom.GetAggGeom(
					FTransform(GetLocalToWorld()),
					SelectionColor.ToFColor(false),
					CollisionMaterialInstance,
					false,
					bDrawSolid,
					AlwaysHasVelocity(),
					ViewIndex,
					Collector);
			}

			/**
			 * Wireframe: View Mode > Wireframe.
			 */
			else if (bIsWireframeView)
			{
				FLinearColor ViewWireframeColor =
					ViewFamily.EngineShowFlags.ActorColoration
						? GetPrimitiveColor()
						: GetWireframeColor();

				// Note: This will be registered for deletion within
				// RegisterOneFrameMaterialProxy().
				FColoredMaterialRenderProxy* WireframeMaterialInstance =
					new FColoredMaterialRenderProxy(
						GEngine->WireframeMaterial->GetRenderProxy(),
						GetSelectionColor(
							ViewWireframeColor,
							IsSelected(),
							IsHovered(),
							false));
				Collector.RegisterOneFrameMaterialProxy(
					WireframeMaterialInstance);

				FMeshBatch& Mesh = Collector.AllocateMesh();
				Mesh.bDisableBackfaceCulling = true; // In case we're inside.
				Mesh.LODIndex = 0;
				Mesh.MaterialRenderProxy = WireframeMaterialInstance;
				Mesh.bUseWireframeSelectionColoring = IsSelected();
				Mesh.VertexFactory = &VertexFactory;
				Mesh.bWireframe = true;

				FMeshBatchElement& BatchElement = Mesh.Elements[0];
				BatchElement.FirstIndex = 0;
				BatchElement.IndexBuffer = &IndexBuffer;
				BatchElement.NumPrimitives = NumConvexHullTris;

				Collector.AddMesh(ViewIndex, Mesh);
			}

			/**
			 * If no special display, render an invisible mesh to enable mouse
			 * selection.
			 */
			else
			{
				/**
				 * Note: I haven't confirmed this is deleted by Unreal, but
				 * other scene proxies do the same thing.
				 */
				FColoredMaterialRenderProxy* HullMaterialInstance =
					new FColoredMaterialRenderProxy(
						GEngine->GeomMaterial->GetRenderProxy(),
						FLinearColor(0, 0, 0, 0));
				FMeshBatch& Mesh = Collector.AllocateMesh();
				Mesh.bDisableBackfaceCulling = true; // In case we're inside.
				Mesh.LODIndex = 0;
				Mesh.MaterialRenderProxy = HullMaterialInstance;
				Mesh.VertexFactory = &VertexFactory;

				FMeshBatchElement& BatchElement = Mesh.Elements[0];
				BatchElement.FirstIndex = 0;
				BatchElement.IndexBuffer = &IndexBuffer;
				BatchElement.NumPrimitives = NumConvexHullTris;

				Collector.AddMesh(ViewIndex, Mesh);
			}
		}
	}
}
#endif

bool FSplatSceneProxy::IsVisible(const FSceneView& View) const
{
	check(Asset);

	bool bIsShown = IsShown(&View);
	bool bIsInScene = &GetScene() == View.Family->Scene;
	bool bIsVisible = bIsShown && bIsInScene;

#if WITH_EDITOR
	const FEngineShowFlags& Flags = View.Family->EngineShowFlags;
	bool bIsWireframe = Flags.Wireframe;
	bool bIsCollision =
		Flags.Collision || Flags.CollisionPawn || Flags.CollisionVisibility;

	return !bIsWireframe && !bIsCollision && bIsVisible;
#else
	return bIsVisible;
#endif
}

void FSplatSceneProxy::TryEnqueueSort(
	const FVector3f& OriginCM, const FVector3f& Forward)
{
	check(!bIsSortingOnGPU);
	check(Asset);
	check(CPUSorting);

	if (!CPUSorting->IsReadyForSorting())
	{
		return;
	}

	// This launches a new sorting task which will `delete` itself once finished.
	// This is necessary as we otherwise must wait on the task to be completed in
	// our destructor before it can be deleted.
	// See AsyncWork.h.
	(new FAutoDeleteAsyncTask<FCPUSortingTask>(
		 Asset->GetPositions(),
		 CPUSorting,
		 OriginCM,
		 Forward,
		 FMatrix44f(GetLocalToWorld())))
		->StartBackgroundTask();
}

} // namespace PICO::Splat