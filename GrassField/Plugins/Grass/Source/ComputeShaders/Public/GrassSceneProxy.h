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

namespace GrassMesh
{
	/** Buffers filled by GPU culling. */
	struct COMPUTESHADERS_API FDrawInstanceBuffers
	{
		/* Culled instance buffer. */
		FBufferRHIRef InstanceBuffer;
		FUnorderedAccessViewRHIRef InstanceBufferUAV;
		FShaderResourceViewRHIRef InstanceBufferSRV;

		/* IndirectArgs buffer for final DrawInstancedIndirect. */
		FBufferRHIRef IndirectArgsBuffer;
		FUnorderedAccessViewRHIRef IndirectArgsBufferUAV;
	};

	struct COMPUTESHADERS_API FIndirectDrawBuffers
	{

	};
}

class COMPUTESHADERS_API FGrassSceneProxy final : public FPrimitiveSceneProxy
{
public:
	FGrassSceneProxy(class UGrassFieldComponent* InComponent);

	FGrassSceneProxy(class UGrassFieldComponent* InComponent, 
		TResourceArray<GrassMesh::FPackedGrassData>* GrassData,
		FUintVector2 LodStepsRange, 
		float Lambda, float CutOffDistance);

	//virtual ~FGrassSceneProxy()
	//{
	//	VertexBuffer->ReleaseResource();
	//	IndexBuffer->ReleaseResource();
	//	VertexFactory->ReleaseResource();
	//}

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

	class FMaterialRenderProxy *Material;
	FMaterialRelevance MaterialRelevance;

	bool bCallbackRegistered;

	class FGrassVertexFactory *VertexFactory;

	TResourceArray<GrassMesh::FPackedGrassData>* GrassData;
	FUintVector2 LodStepsRange = FUintVector2(1, 7);
	float Lambda = 1;
	float CutoffDistance = 1000;
	FGrassVertexBuffer* VertexBuffer = nullptr;
	FGrassIndexBuffer* IndexBuffer = nullptr;
};

//  Notes: Looks like GetMeshShaderMap is returning nullptr during the DepthPass

