// Copyright Epic Games, Inc. All Rights Reserved.
// Adapted from the VirtualHeightfieldMesh plugin

#pragma once

#include "CoreMinimal.h"

#include "GlobalShader.h"
#include "PrimitiveSceneProxy.h"
#include "DataDrivenShaderPlatformInfo.h"

#include "EngineModule.h"
#include "Engine/Engine.h"

#include "HAL/IConsoleManager.h"
#include "CommonRenderResources.h"

#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "RenderUtils.h"

#include "MaterialDomain.h"
#include "Materials/MaterialRenderProxy.h"
#include "Materials/Material.h"

#include "GrassVertexFactory.h"
#include "GrassData.h"
#include "GrassFieldComponent.h"
#include "GrassShaders.h"

class FGrassSceneProxy;
class FGrassRendererExtension;

namespace GrassMesh
{
	/* Keep indirect args offsets in sync with ISM.usf. */
	static const int32 IndirectArgsByteOffset_FinalCull = 0;
	static const int32 IndirectArgsByteSize = 4 * sizeof(uint32);

	/** Buffers filled by GPU culling. */
	struct COMPUTESHADERS_API FPersistentBuffers
	{
		FBufferRHIRef GrassDataBuffer;
		FShaderResourceViewRHIRef GrassDataBufferSRV;

#if USE_INSTANCING
		/* Culled instance buffer. */
		FBufferRHIRef InstanceBuffer;
		FUnorderedAccessViewRHIRef InstanceBufferUAV;
		FShaderResourceViewRHIRef InstanceBufferSRV;
#endif

		/* IndirectArgs buffer for final DrawInstancedIndirect. */
		FBufferRHIRef IndirectArgsBuffer;
		FUnorderedAccessViewRHIRef IndirectArgsBufferUAV;
	};

	/** View matrices that can be frozen in freezerendering mode. */
	struct COMPUTESHADERS_API FViewData
	{
		FVector ViewOrigin;
		FMatrix ProjectionMatrix;
		FMatrix ViewMatrix;
		FMatrix ViewProjectionMatrix;
		FConvexVolume ViewFrustum;
		bool bViewFrozen;
	};


	struct COMPUTESHADERS_API FProxyDesc
	{
		TResourceArray<GrassMesh::FPackedGrassData>* GrassData;
		FUintVector2 LodStepsRange;
		float Lambda;
		float CutOffDistance;
		FGrassVertexBuffer* VertexBuffer;
		FGrassIndexBuffer* IndexBuffer;
	};

	/** View description used for LOD calculation in the main view. */
	struct COMPUTESHADERS_API FMainViewDesc
	{
		FSceneView const* ViewDebug;
		FVector3f ViewOrigin;
		FMatrix44f ViewMatrix;
		FMatrix44f ViewProjectionMatrix;
		float LodBiasScale;
		FVector4 LodDistances;
	};

	/** View description used for culling in the child view. */
	struct COMPUTESHADERS_API FChildViewDesc
	{
		FSceneView const* ViewDebug;
		FVector3f ViewOrigin;
		FMatrix44f ViewMatrix;
		FMatrix44f ViewProjectionMatrix;
		bool bIsMainView;
	};

	/** Structure to carry RDG resources. */
	struct COMPUTESHADERS_API FVolatileResources
	{
		FRDGBufferRef Counter;
		FRDGBufferUAVRef CounterUAV;
		FRDGBufferSRVRef CounterSRV;

		FRDGBufferRef CulledGrassDataBuffer;
		FRDGBufferUAVRef CulledGrassDataBufferUAV;
		FRDGBufferSRVRef CulledGrassDataBufferSRV;

		FRDGBufferRef IndirectArgsBuffer;
		FRDGBufferUAVRef IndirectArgsBufferUAV;
		FRDGBufferSRVRef IndirectArgsBufferSRV;
	};
}

const static FName NAME_Grass(TEXT("Grass"));

class COMPUTESHADERS_API FGrassSceneProxy final : public FPrimitiveSceneProxy
{
public:
	FGrassSceneProxy(class UGrassFieldComponent* InComponent);

	FGrassSceneProxy(class UGrassFieldComponent* InComponent, 
		TResourceArray<GrassMesh::FPackedGrassData>* GrassData,
		FUintVector2 LodStepsRange, 
		float Lambda, float CutOffDistance);

	virtual ~FGrassSceneProxy() override
	{
		// TODO: adjust resource release
		if(VertexBuffer != nullptr)
			VertexBuffer->ReleaseResource();

		if (IndexBuffer != nullptr)
			IndexBuffer->ReleaseResource();

		if (VertexFactory != nullptr)
			VertexFactory->ReleaseResource();
	}

protected:
	//~ Begin FPrimitiveSceneProxy Interface
	virtual SIZE_T GetTypeHash() const override;
	virtual uint32 GetMemoryFootprint() const override;
	virtual void CreateRenderThreadResources() override;
	virtual void DestroyRenderThreadResources() override;
	virtual void OnTransformChanged() override;
	// virtual bool HasSubprimitiveOcclusionQueries() const override;
	// virtual const TArray<FBoxSphereBounds>* GetOcclusionQueries(const FSceneView* View) const override;
	// virtual void AcceptOcclusionResults(const FSceneView* View, TArray<bool>* Results, int32 ResultsStart, int32 NumResults) override;
	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView *View) const override;
	virtual void GetDynamicMeshElements(const TArray<const FSceneView *> &Views, const FSceneViewFamily &ViewFamily, uint32 VisibilityMap, FMeshElementCollector &Collector) const override;
	//~ End FPrimitiveSceneProxy Interface

private:
	void BuildOcclusionVolumes(TArrayView<FVector2D> const &InMinMaxData, FIntPoint const &InMinMaxSize, TArrayView<int32> const &InMinMaxMips, int32 InNumLods);

public:
	bool bHiddenInEditor;
	bool bCallbackRegistered;

	class FMaterialRenderProxy *Material;
	FMaterialRelevance MaterialRelevance;

	TResourceArray<GrassMesh::FPackedGrassData>* GrassData;
	FUintVector2 LodStepsRange = FUintVector2(1, 7);
	float Lambda = 1;
	float CutoffDistance = 1000;

	FGrassVertexFactory* VertexFactory;
	FGrassVertexBuffer* VertexBuffer = nullptr;
	FGrassIndexBuffer* IndexBuffer = nullptr;
};

//  Notes: Looks like GetMeshShaderMap is returning nullptr during the DepthPass

/** Renderer extension to manage the buffer pool and add hooks for GPU culling passes. */
class FGrassRendererExtension : public FRenderResource
{
public:
	FGrassRendererExtension()
		: bInFrame(false), DiscardId(0)
	{
	}

	virtual ~FGrassRendererExtension()
	{
	}

	bool IsInFrame() { return bInFrame; }

	/** Call once to register this extension. */
	void RegisterExtension();

	/** Call once per frame for each mesh/view that has relevance.
	 *  This allocates the buffers to use for the frame and adds
	 *  the work to fill the buffers to the queue.
	 */
	GrassMesh::FPersistentBuffers& AddWork(
		const FGrassSceneProxy* InProxy,
		const FSceneView* InMainView,
		const FSceneView* InCullView);

	/** Submit all the work added by AddWork(). The work fills all of the buffers ready for use by the referencing mesh batches. */
	void SubmitWork(FRDGBuilder& GraphBuilder);

protected:
	//~ Begin FRenderResource Interface
	virtual void ReleaseRHI() override;
	//~ End FRenderResource Interface

private:
	/** Called by renderer at start of render frame. */
	void BeginFrame(FRDGBuilder& GraphBuilder);

	/** Called by renderer at end of render frame. */
	void EndFrame(FRDGBuilder& GraphBuilder);
	void EndFrame();

	/** Flag for frame validation. */
	bool bInFrame;

	/** Buffers to fill. Resources can persist between frames to reduce allocation cost, but contents don't persist. */
	TArray<GrassMesh::FPersistentBuffers> Buffers;
	/** Per buffer frame time stamp of last usage. */
	TArray<uint32> DiscardIds;
	/** Current frame time stamp. */
	uint32 DiscardId;

	/** Arrary of unique scene proxies to render this frame. */
	TArray<const FGrassSceneProxy*> SceneProxies;
	/** Arrary of unique main views to render this frame. */
	TArray<const FSceneView*> MainViews;
	/** Arrary of unique culling views to render this frame. */
	TArray<const FSceneView*> CullViews;

	/** Key for each buffer we need to generate. */
	struct FWorkDesc
	{
		int8 ProxyIndex;
		int8 MainViewIndex;
		int8 CullViewIndex;
		int8 BufferIndex;
	};

	/** Keys specifying what to render. */
	TArray<FWorkDesc> WorkDescs;

	/** Sort predicate for FWorkDesc. When rendering we want to batch work by proxy, then by main view. */
	struct FWorkDescSort
	{
		uint32 SortKey(const FWorkDesc& WorkDesc) const
		{
			return (WorkDesc.ProxyIndex << 24) | (WorkDesc.MainViewIndex << 16) | (WorkDesc.CullViewIndex << 8) | WorkDesc.BufferIndex;
		}

		bool operator()(const FWorkDesc& A, const FWorkDesc& B) const
		{
			return SortKey(A) < SortKey(B);
		}
	};
};