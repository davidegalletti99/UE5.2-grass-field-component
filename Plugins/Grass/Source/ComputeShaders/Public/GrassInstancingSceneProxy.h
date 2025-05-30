// Copyright Epic Games, Inc. All Rights Reserved.
// Adapted from the VirtualHeightfieldMesh plugin

#pragma once

#include "CoreMinimal.h"

#include "GlobalShader.h"
#include "PrimitiveSceneProxy.h"
#include "DataDrivenShaderPlatformInfo.h"

#include "ConvexVolume.h"

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

#include "GrassInstancingVertexFactory.h"
#include "GrassData.h"
#include "GrassFieldComponent.h"
#include "GrassShaders.h"

class FGrassInstancingSceneProxy;
class FGrassInstancingSectionProxy;
class FGrassInstancingRendererExtension;

namespace GrassUtils
{
	static constexpr int32 IndirectArgsPerElementSize = sizeof(uint32);
	static constexpr int32 IndirectArgsBytesSize = 5 * IndirectArgsPerElementSize;

	/** Buffers filled by GPU culling. */
	struct COMPUTESHADERS_API FPersistentBuffers
	{
		FGrassInstancingSectionProxy* SectionProxy = nullptr;

		/* GrassData buffer. */
		FBufferRHIRef GrassDataBuffer;
		FShaderResourceViewRHIRef GrassDataBufferSRV;

		/* ForceMap buffer. */
		FBufferRHIRef GrassForceMap;
		FUnorderedAccessViewRHIRef GrassForceMapUAV;
		FShaderResourceViewRHIRef GrassForceMapSRV;

		/* Culled instance buffer. */
		FBufferRHIRef InstanceBuffer;
		FUnorderedAccessViewRHIRef InstanceBufferUAV;
		FShaderResourceViewRHIRef InstanceBufferSRV;
		
		/* IndirectArgs buffer for final DrawInstancedIndirect. */
		FBufferRHIRef IndirectArgsBuffer;
		FUnorderedAccessViewRHIRef IndirectArgsBufferUAV;
		FShaderResourceViewRHIRef IndirectArgsBufferSRV;
	};

	/** View matrices that can be frozen in FreezeRendering mode. */
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
		bool IsValid = false;
		bool bIsCullingEnabled;
		TResourceArray<GrassUtils::FPackedGrassData>* GrassData;
		float CutoffDistance;
		int NumIndices;
	};

	/** View description used for LOD calculation in the main view. */
	struct COMPUTESHADERS_API FMainViewDesc
	{
		bool IsValid = false;
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
		bool IsValid = false;
		FSceneView const* ViewDebug;
		FVector3f ViewOrigin;
		FMatrix44f ViewMatrix;
		FMatrix44f ViewProjectionMatrix;
		bool bIsMainView;
	};

	/** Structure to carry RDG resources. */
	struct COMPUTESHADERS_API FVolatileResources
	{
		FRDGBufferRef CulledGrassDataBuffer;
		FRDGBufferUAVRef CulledGrassDataBufferUAV;
		FRDGBufferSRVRef CulledGrassDataBufferSRV;
	};
}

const static FName NAME_GrassInstancing(TEXT("GrassInstancing"));

class COMPUTESHADERS_API FGrassInstancingSectionProxy
{
public:
	explicit FGrassInstancingSectionProxy(const ERHIFeatureLevel::Type FeatureLevel);
	
	~FGrassInstancingSectionProxy()
	{
		if (VertexFactory.IsInitialized())
			VertexFactory.ReleaseResource();
	}

	static SIZE_T GetTypeHash()
	{
		static size_t UniquePointer;
		return reinterpret_cast<size_t>(&UniquePointer);
	}

	TResourceArray<GrassUtils::FPackedGrassData> GrassData;
	FBox Bounds = FBox(ForceInitToZero);
	uint32 NumIndices = 0;
	float CutoffDistance = 0.0f;
	bool bIsGPUCullingEnabled = true;
	
	FGrassInstancingVertexFactory VertexFactory;
};

struct COMPUTESHADERS_API FGrassMeshLodData
{
	uint8 Steps = 0;
	uint32 NumIndices = 1;
	uint32 NumVertices = 3;

	
	FGrassInstancingIndexBuffer* IndexBuffer;
	FGrassInstancingVertexBuffer* VertexBuffer;

	FGrassMeshLodData()
		: IndexBuffer(nullptr)
		, VertexBuffer(nullptr)
	{
		
	}

	explicit FGrassMeshLodData(const uint8 Steps)
		: Steps(Steps)
	{
		IndexBuffer = new FGrassInstancingIndexBuffer();
		VertexBuffer = new FGrassInstancingVertexBuffer();

		CreateGrassModels(VertexBuffer->Vertices, IndexBuffer->Indices, Steps);
		
		NumIndices = IndexBuffer->Indices.Num();
		NumVertices = VertexBuffer->Vertices.Num();

	}

	~FGrassMeshLodData()
	{
		if (IndexBuffer->IsInitialized())
			IndexBuffer->ReleaseResource();

		if (VertexBuffer->IsInitialized())
			VertexBuffer->ReleaseResource();
		
		delete VertexBuffer;
		delete IndexBuffer;
	}

	void InitResources() const
	{
		IndexBuffer->InitResource();
		VertexBuffer->InitResource();
	}
};

class COMPUTESHADERS_API FGrassInstancingSceneProxy final : public FPrimitiveSceneProxy
{
public:
	explicit FGrassInstancingSceneProxy(class UGrassFieldComponent* InComponent);


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

	void CreateBaseMeshBatch(
        FMeshElementCollector& Collector,
        const FSceneViewFamily& ViewFamily,
        int32 ViewIndex,
        const FGrassMeshLodData* Lod,
        FGrassInstancingSectionProxy* Section) const;
	
private:
	void BuildOcclusionVolumes(TArrayView<FVector2D> const &InMinMaxData, FIntPoint const &InMinMaxSize, TArrayView<int32> const &InMinMaxMips, int32 InNumLods);

public:
	bool bHiddenInEditor;
	bool bCallbackRegistered;
	
	class FMaterialRenderProxy *Material;
	FMaterialRelevance MaterialRelevance;

	float CutoffDistance;
	bool bIsCPUCullingEnabled;
	FUintVector2 MinMaxLodSteps;

	TMap<uint8, FGrassMeshLodData*> Lods;
	TArray<FGrassInstancingSectionProxy*> Sections;
};

//  Notes: Looks like GetMeshShaderMap is returning nullptr during the DepthPass

/** Renderer extension to manage the buffer pool and add hooks for GPU culling passes. */
class FGrassInstancingRendererExtension : public FRenderResource
{
public:
	FGrassInstancingRendererExtension()
		: bInFrame(false), DiscardId(0)
	{
	}

	virtual ~FGrassInstancingRendererExtension() override
	{
	}

	bool IsInFrame() const { return bInFrame; }

	/** Call once to register this extension. */
	void RegisterExtension();

	/** Call once per frame for each mesh/view that has relevance.
	 *  This allocates the buffers to use for the frame and adds
	 *  the work to fill the buffers to the queue.
	 */
	GrassUtils::FPersistentBuffers& AddWork(
		FGrassInstancingSectionProxy* InSection,
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
	TArray<GrassUtils::FPersistentBuffers> Buffers;
	/** Per buffer frame time stamp of last usage. */
	TArray<uint32> DiscardIds;
	/** Current frame time stamp. */
	uint32 DiscardId;

	/** Array of unique scene proxies to render this frame. */
	TArray<FGrassInstancingSectionProxy*> SceneProxies;
	/** Array of unique main views to render this frame. */
	TArray<const FSceneView*> MainViews;
	/** Array of unique culling views to render this frame. */
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
		static uint32 SortKey(const FWorkDesc& WorkDesc)
		{
			return (WorkDesc.ProxyIndex    << 24)
				|  (WorkDesc.MainViewIndex << 16)
				|  (WorkDesc.CullViewIndex << 8)
				|  (WorkDesc.BufferIndex);
		}

		bool operator()(const FWorkDesc& A, const FWorkDesc& B) const
		{
			return SortKey(A) < SortKey(B);
		}
	};
};