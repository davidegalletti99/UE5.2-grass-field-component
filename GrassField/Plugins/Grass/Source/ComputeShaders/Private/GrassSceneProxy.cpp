// Copyright Epic Games, Inc. All Rights Reserved.
// Adapted from the VirtualHeightfieldMesh plugin

#include "GrassSceneProxy.h"

#include "PixelShaderUtils.h"

/** Single global instance of the ISM renderer extension. */
TGlobalResource<FGrassRendererExtension> GrassRendererExtension;

namespace GrassMesh
{
	void ReleaseInstanceBuffers(FPersistentBuffers& InBuffers)
	{
		InBuffers.GrassDataBuffer.SafeRelease();
		InBuffers.GrassDataBufferSRV.SafeRelease();

#if USE_INSTANCING
		InBuffers.InstanceBuffer.SafeRelease();
		InBuffers.InstanceBufferUAV.SafeRelease();
		InBuffers.InstanceBufferSRV.SafeRelease();
#endif

		InBuffers.IndirectArgsBuffer.SafeRelease();
		InBuffers.IndirectArgsBufferUAV.SafeRelease();
	}

	void InitializeInstanceBuffers(const FGrassSceneProxy* InProxy, FPersistentBuffers& InBuffers)
	{
		const int32 GrassDataNum = InProxy->GrassData->Num();
		{
			FRHIResourceCreateInfo CreateInfo(TEXT("FGrass.GrassDataBuffer"), InProxy->GrassData);
			constexpr int32 GrassDataSize = sizeof(FPackedGrassData);
			const int32 GrassDataBufferSize = GrassDataNum * GrassDataSize;
			InBuffers.GrassDataBuffer = RHICreateStructuredBuffer(GrassDataSize, GrassDataBufferSize, BUF_ShaderResource, ERHIAccess::SRVMask, CreateInfo);
			InBuffers.GrassDataBufferSRV = RHICreateShaderResourceView(InBuffers.GrassDataBuffer);
		}
#if USE_INSTANCING
		{
			FRHIResourceCreateInfo CreateInfo(TEXT("FGrass.InstanceBuffer"));
			constexpr int32 InstanceSize = sizeof(FGrassInstance);
			const int32 InstanceBufferSize = GrassDataNum * InstanceSize;
			InBuffers.InstanceBuffer = RHICreateStructuredBuffer(InstanceSize, InstanceBufferSize, BUF_UnorderedAccess | BUF_ShaderResource, ERHIAccess::SRVMask, CreateInfo);
			InBuffers.InstanceBufferUAV = RHICreateUnorderedAccessView(InBuffers.InstanceBuffer, PF_R32_UINT);
			InBuffers.InstanceBufferSRV = RHICreateShaderResourceView(InBuffers.InstanceBuffer);
		}
#endif
		{
			FRHIResourceCreateInfo CreateInfo(TEXT("FGrass.InstanceIndirectArgsBuffer"));
			constexpr int32 IndirectArgsSize = sizeof(uint32);
			constexpr int32 IndirectArgsBufferSize = 5 * IndirectArgsSize;
			InBuffers.IndirectArgsBuffer = RHICreateVertexBuffer(
				IndirectArgsBufferSize, BUF_UnorderedAccess | BUF_DrawIndirect,
				ERHIAccess::IndirectArgs, CreateInfo);
			InBuffers.IndirectArgsBufferUAV = RHICreateUnorderedAccessView(
				InBuffers.IndirectArgsBuffer, PF_R32_UINT);
		}

	}

	/** Fill the FViewData from an FSceneView respecting the freezerendering mode. */
	inline void GetViewData(FSceneView const* InSceneView, FViewData& OutViewData)
	{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		const FViewMatrices* FrozenViewMatrices = InSceneView->State != nullptr ? InSceneView->State->GetFrozenViewMatrices() : nullptr;
		if (FrozenViewMatrices != nullptr)
		{
			OutViewData.ViewOrigin = FrozenViewMatrices->GetViewOrigin();
			OutViewData.ProjectionMatrix = FrozenViewMatrices->GetProjectionMatrix();
			OutViewData.ViewMatrix = FrozenViewMatrices->GetViewMatrix();
			OutViewData.ViewProjectionMatrix = FrozenViewMatrices->GetViewProjectionMatrix();
			OutViewData.bViewFrozen = true;
		}
		else
#endif
		{
			OutViewData.ViewOrigin = InSceneView->ViewMatrices.GetViewOrigin();
			OutViewData.ProjectionMatrix = InSceneView->ViewMatrices.GetProjectionMatrix();
			OutViewData.ViewMatrix = InSceneView->ViewMatrices.GetViewMatrix();
			OutViewData.ViewProjectionMatrix = InSceneView->ViewMatrices.GetViewProjectionMatrix();
			OutViewData.bViewFrozen = false;
		}
	}

	/** Initialize the volatile resources used in the render graph. */
	void InitializeResources(
		FRDGBuilder& GraphBuilder,
		const FProxyDesc& ProxyDesc,
		const FMainViewDesc& InMainViewDesc,
		FVolatileResources& OutResources)
	{
		const int32 GrassDataNum = ProxyDesc.GrassData->Num();
		{
			OutResources.CulledGrassDataBuffer = GraphBuilder.CreateBuffer(
				FRDGBufferDesc::CreateStructuredDesc(sizeof(FPackedLodGrassData), GrassDataNum),
				TEXT("GrassMesh.CulledGrassDataBuffer"));

			OutResources.CulledGrassDataBufferUAV = GraphBuilder.CreateUAV(OutResources.CulledGrassDataBuffer);
			OutResources.CulledGrassDataBufferSRV = GraphBuilder.CreateSRV(OutResources.CulledGrassDataBuffer);
		}
		{
			OutResources.Counter = GraphBuilder.CreateBuffer(
				FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), 2),
				TEXT("GrassMesh.Counter"));
			OutResources.CounterUAV = GraphBuilder.CreateUAV(OutResources.Counter);
			OutResources.CounterSRV = GraphBuilder.CreateSRV(OutResources.Counter);
		}
		{
			OutResources.IndirectArgsBuffer = GraphBuilder.CreateBuffer(
				FRDGBufferDesc::CreateIndirectDesc<FRHIDrawIndirectParameters /*FRHIDrawIndexedIndirectParameters*/>(IndirectArgsByteSize),
				TEXT("GrassMesh.IndirectArgsBuffer"));
			OutResources.IndirectArgsBufferUAV = GraphBuilder.CreateUAV(OutResources.IndirectArgsBuffer);
			OutResources.IndirectArgsBufferSRV = GraphBuilder.CreateSRV(OutResources.IndirectArgsBuffer);
		}
	}

	/** Transition our output draw buffers for use. Read or write access is set according to the bToWrite parameter. */
	void AddPass_TransitionAllDrawBuffers(
		FRDGBuilder& GraphBuilder, 
		TArray<FPersistentBuffers> const& Buffers, 
		TArrayView<int32> const& BufferIndices, 
		bool bToWrite)
	{
		TArray<FRHIUnorderedAccessView*> OverlapUAVs;
		OverlapUAVs.Reserve(BufferIndices.Num());

		TArray<FRHITransitionInfo> TransitionInfos;
		TransitionInfos.Reserve(BufferIndices.Num() * 2);

		for (int32 BufferIndex : BufferIndices)
		{
			FRHIUnorderedAccessView* IndirectArgsBufferUAV = Buffers[BufferIndex].IndirectArgsBufferUAV;
#if USE_INSTANCING
			FRHIUnorderedAccessView* InstanceBufferUAV = Buffers[BufferIndex].InstanceBufferUAV;
#endif

			OverlapUAVs.Add(IndirectArgsBufferUAV);

			ERHIAccess DrawAccessFrom = bToWrite ? ERHIAccess::SRVMask : ERHIAccess::UAVMask;
			ERHIAccess DrawAccessTo = bToWrite ? ERHIAccess::UAVMask : ERHIAccess::SRVMask;

			const ERHIAccess IndirectArgsAccessFrom = bToWrite ? ERHIAccess::IndirectArgs : ERHIAccess::UAVMask;
			const ERHIAccess IndirectArgsAccessTo = bToWrite ? ERHIAccess::UAVMask : ERHIAccess::IndirectArgs;

			TransitionInfos.Add(
				FRHITransitionInfo(IndirectArgsBufferUAV, IndirectArgsAccessFrom, IndirectArgsAccessTo));
#if USE_INSTANCING
			TransitionInfos.Add(
				FRHITransitionInfo(InstanceBufferUAV, DrawAccessFrom, DrawAccessTo));
#endif
		}

		AddPass(GraphBuilder, RDG_EVENT_NAME("TransitionAllDrawBuffers"), [bToWrite, OverlapUAVs, TransitionInfos](FRHICommandList& InRHICmdList)
			{
				if (!bToWrite)
				{
					InRHICmdList.EndUAVOverlap(OverlapUAVs);
				}

				InRHICmdList.Transition(TransitionInfos);

				if (bToWrite)
				{
					InRHICmdList.BeginUAVOverlap(OverlapUAVs);
				}
			});
	}

	/** Initialize the buffers before collecting visible grass points. */
	void AddPass_InitBuffers(
		FRDGBuilder& GraphBuilder,
		FGlobalShaderMap* InGlobalShaderMap,
		FVolatileResources& InVolatileResources)
	{
		TShaderMapRef<FInitBuffers_CS> ComputeShader(InGlobalShaderMap);

		FInitBuffers_CS::FParameters* PassParameters =
			GraphBuilder.AllocParameters<FInitBuffers_CS::FParameters>();

		PassParameters->RWIndirectArgsBuffer = InVolatileResources.IndirectArgsBufferUAV;
		PassParameters->RWCounter = InVolatileResources.CounterUAV;

		FComputeShaderUtils::AddPass<FInitBuffers_CS>(
			GraphBuilder,
			RDG_EVENT_NAME("InitBuffers"),
			ComputeShader,
			PassParameters,
			FIntVector(1, 1, 1));
	}

	/** Initialise the draw indirect buffer. */
	void AddPass_InitIndirectArgs(
		FRDGBuilder& GraphBuilder,
		FGlobalShaderMap* InGlobalShaderMap,
		FPersistentBuffers& InOutputResources)
	{
		TShaderMapRef<FInitInstanceBuffer_CS> ComputeShader(InGlobalShaderMap);

		FInitInstanceBuffer_CS::FParameters* PassParameters =
			GraphBuilder.AllocParameters<FInitInstanceBuffer_CS::FParameters>();
		PassParameters->RWIndirectArgsBuffer = InOutputResources.IndirectArgsBufferUAV;

		FComputeShaderUtils::AddPass<FInitInstanceBuffer_CS>(
			GraphBuilder,
			RDG_EVENT_NAME("InitIndirectArgs"),
			ComputeShader, PassParameters,
			FIntVector(1, 1, 1));
	}

	/** Cull quads and write to the final output buffer. */
	void AddPass_CullGrassData(
		FRDGBuilder& GraphBuilder,
		FGlobalShaderMap* InGlobalShaderMap,
		FVolatileResources& InVolatileResources,
		FPersistentBuffers& InOutputResources,
		const FProxyDesc& ProxyDesc,
		const FMainViewDesc& InViewDesc)
	{
		FCullGrassData_CS::FParameters* PassParameters =
			GraphBuilder.AllocParameters<FCullGrassData_CS::FParameters>();
		const FCullGrassData_CS::FPermutationDomain PermutationVector;
		const TShaderMapRef<FCullGrassData_CS> ComputeShader(InGlobalShaderMap, PermutationVector);

		PassParameters->VP_MATRIX = InViewDesc.ViewProjectionMatrix;
		PassParameters->CameraPosition = InViewDesc.ViewOrigin;
		PassParameters->CutoffDistance = ProxyDesc.CutOffDistance;
		PassParameters->GrassDataSize = ProxyDesc.GrassData->Num();
		PassParameters->GrassDataBuffer = InOutputResources.GrassDataBufferSRV;
		PassParameters->RWCulledGrassDataBuffer = InVolatileResources.CulledGrassDataBufferUAV;
		PassParameters->RWIndirectArgsBuffer = InVolatileResources.IndirectArgsBufferUAV;
		PassParameters->RWCounter = InVolatileResources.CounterUAV;


		const int32 GrassDataNum = ProxyDesc.GrassData->Num();
		const FIntVector GroupCount = FIntVector(FMath::CeilToInt(GrassDataNum / 1024.0f), 1, 1);
		FComputeShaderUtils::AddPass<FCullGrassData_CS>(
			GraphBuilder,
			RDG_EVENT_NAME("CullGrassData"),
			ComputeShader, PassParameters, GroupCount);
	}

	void AddPass_ComputeGrassMesh(
		FRDGBuilder& GraphBuilder,
		FGlobalShaderMap* InGlobalShaderMap,
		FVolatileResources& InVolatileResources,
		FPersistentBuffers& InOutputResources,
		const FProxyDesc& InProxyDesc,
		const FChildViewDesc& InViewDesc)
	{
		FComputeGrassMesh_CS::FParameters* PassParameters =
			GraphBuilder.AllocParameters<FComputeGrassMesh_CS::FParameters>();
		const FComputeGrassMesh_CS::FPermutationDomain PermutationVector;
		const TShaderMapRef<FComputeGrassMesh_CS> ComputeShader(InGlobalShaderMap, PermutationVector);
		

#if USE_INSTANCING
		PassParameters->RWInstanceBuffer = InOutputResources.InstanceBufferUAV;
#endif
		PassParameters->Lambda = InProxyDesc.Lambda;
		PassParameters->CameraPosition = InViewDesc.ViewOrigin;
		PassParameters->ViewMatrix = InViewDesc.ViewMatrix;
		PassParameters->RWCounter = InVolatileResources.CounterUAV;
		PassParameters->CulledGrassDataBuffer = InVolatileResources.CulledGrassDataBufferSRV;
		PassParameters->RWVertexBuffer = InProxyDesc.VertexBuffer->VertexBufferUAV;
		PassParameters->RWIndexBuffer = InProxyDesc.IndexBuffer->IndexBufferUAV;
		PassParameters->RWIndirectArgsBuffer = InOutputResources.IndirectArgsBufferUAV;
		PassParameters->IndirectArgsBuffer = InVolatileResources.IndirectArgsBuffer;
		PassParameters->IndirectArgsBufferSRV = InVolatileResources.IndirectArgsBufferSRV;


		const int32 GrassDataNum = InProxyDesc.GrassData->Num();
		const FIntVector GroupCount = FIntVector(FMath::CeilToInt(GrassDataNum / 1024.0f), 1, 1);
		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("ComputeGrassMesh"),
			ComputeShader, PassParameters, GroupCount);
	}
}


// Begin FGrassSceneProxy implementations
FGrassSceneProxy::FGrassSceneProxy(UGrassFieldComponent* InComponent,
	TResourceArray<GrassMesh::FPackedGrassData>* GrassData,
	FUintVector2 LodStepsRange, float Lambda, float CutoffDistance)
	: FPrimitiveSceneProxy(InComponent, NAME_Grass), VertexFactory(nullptr)
{
	this->GrassData = GrassData;
	this->LodStepsRange = LodStepsRange;
	this->Lambda = Lambda;
	this->CutoffDistance = CutoffDistance;

	GrassRendererExtension.RegisterExtension();

	// They have some LOD, but considered static as the LODs (are intended to) represent the same static surface.
	// TODO Check if this allows WPO
	bHasDeformableMesh = false;

	const UMaterialInterface* ComponentMaterial = InComponent->GetMaterial();
	// TODO MATUSAGE
	const bool bValidMaterial = ComponentMaterial != nullptr;
	Material = bValidMaterial ? ComponentMaterial->GetRenderProxy() : UMaterial::GetDefaultMaterial(MD_Surface)->GetRenderProxy();
	MaterialRelevance = Material->GetMaterialInterface()->GetRelevance_Concurrent(GetScene().GetFeatureLevel());
}

FGrassSceneProxy::FGrassSceneProxy(UGrassFieldComponent* InComponent)
	: FPrimitiveSceneProxy(InComponent, NAME_Grass),
	VertexFactory(nullptr)
{

}

SIZE_T FGrassSceneProxy::GetTypeHash() const
{
	static size_t UniquePointer;
	return reinterpret_cast<size_t>(&UniquePointer);
}

uint32 FGrassSceneProxy::GetMemoryFootprint() const
{
	return (sizeof(*this) + FPrimitiveSceneProxy::GetAllocatedSize());
}

void FGrassSceneProxy::OnTransformChanged()
{
	// TODO
	// UVToLocal = UVToWorld * GetLocalToWorld().Inverse();

	// Setup a default occlusion volume array containing just the primitive bounds.
	// We use this if disabling the full set of occlusion volumes.
	// DefaultOcclusionVolumes.Reset();
	// DefaultOcclusionVolumes.Add(GetBounds());
}

void FGrassSceneProxy::CreateRenderThreadResources()
{
	check(IsInRenderingThread());
	// Gather vertex factory uniform parameters.
	FGrassParameters UniformParams;
	// TODO UNIFORM INIT

	// Create vertex factory.
	VertexFactory = new FGrassVertexFactory(GetScene().GetFeatureLevel(), UniformParams);

	const int32 GrassDataNum = GrassData->Num() > 0 ? GrassData->Num() : 1;
	VertexBuffer = new FGrassVertexBuffer();
	VertexBuffer->GrassDataNum = GrassDataNum;
	VertexBuffer->MaxLodSteps = LodStepsRange.Y;

	IndexBuffer = new FGrassIndexBuffer();
	IndexBuffer->GrassDataNum = GrassDataNum;
	IndexBuffer->MaxLodSteps = LodStepsRange.Y;

	VertexFactory->Init(VertexBuffer);
	IndexBuffer->InitResource();
	VertexBuffer->InitResource();

	VertexFactory->InitResource();
}

void FGrassSceneProxy::DestroyRenderThreadResources()
{
	check(IsInRenderingThread());
	if (VertexFactory != nullptr)
	{
		VertexFactory->ReleaseResource();
		delete VertexFactory;
		VertexFactory = nullptr;
	}
}

FPrimitiveViewRelevance FGrassSceneProxy::GetViewRelevance(const FSceneView* View) const
{
	const bool bValid = true; // TODO Allow users to modify
	const bool bIsHiddenInEditor = bHiddenInEditor && View->Family->EngineShowFlags.Editor;

	FPrimitiveViewRelevance Result;
	Result.bDrawRelevance = bValid && IsShown(View) && !bIsHiddenInEditor;
	Result.bShadowRelevance = bValid && IsShadowCast(View) && ShouldRenderInMainPass() && !bIsHiddenInEditor;
	Result.bDynamicRelevance = true;
	Result.bStaticRelevance = false;
	Result.bRenderInMainPass = ShouldRenderInMainPass();
	Result.bUsesLightingChannels = GetLightingChannelMask() != GetDefaultLightingChannelMask();
	Result.bRenderCustomDepth = ShouldRenderCustomDepth();
	Result.bTranslucentSelfShadow = false;
	Result.bVelocityRelevance = false;
	MaterialRelevance.SetPrimitiveViewRelevance(Result);
	return Result;
}

void FGrassSceneProxy::GetDynamicMeshElements(
	const TArray<const FSceneView*>& Views,
	const FSceneViewFamily& ViewFamily,
	uint32 VisibilityMap,
	FMeshElementCollector& Collector) const
{
	check(IsInRenderingThread());

	if (GrassData->Num() <= 0)
		return;

	if (GrassRendererExtension.IsInFrame())
	{
		// Can't add new work while bInFrame.
		// In UE5 we need to AddWork()/SubmitWork() in two phases: InitViews() and InitViewsAfterPrepass()
		// The main renderer hooks for that don't exist in UE5.0 and are only added in UE5.1
		// That means that for UE5.0 we always hit this for shadow drawing and shadows will not be rendered.
		// Not earlying out here can lead to crashes from buffers being released too soon.
		return;
	}

	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		if (VisibilityMap & (1 << ViewIndex))
		{
			// TODO INIT instance buffer
			GrassMesh::FPersistentBuffers& Buffers =
				GrassRendererExtension.AddWork(this, ViewFamily.Views[0], Views[ViewIndex]);
			
			FMeshBatch& Mesh = Collector.AllocateMesh();
			Mesh.bWireframe = AllowDebugViewmodes() & ViewFamily.EngineShowFlags.Wireframe;
			Mesh.bUseWireframeSelectionColoring = IsSelected();
			Mesh.VertexFactory = VertexFactory;
			Mesh.MaterialRenderProxy = Material;
			Mesh.ReverseCulling = IsLocalToWorldDeterminantNegative();
			Mesh.Type = EPrimitiveType::PT_TriangleList;
			Mesh.DepthPriorityGroup = ESceneDepthPriorityGroup::SDPG_World;
			Mesh.bCanApplyViewModeOverrides = true;
			Mesh.CastRayTracedShadow = true;
			Mesh.CastShadow = true;
			Mesh.bUseForMaterial = true;
			Mesh.bUseForDepthPass = true;

			Mesh.Elements.SetNumZeroed(1);
			{
				FMeshBatchElement& BatchElement = Mesh.Elements[0];
				// TODO allow for non indirect instanced rendering
				BatchElement.IndexBuffer = IndexBuffer;
				BatchElement.IndirectArgsBuffer = Buffers.IndirectArgsBuffer;
				BatchElement.IndirectArgsOffset = 0;
				BatchElement.FirstIndex = 0;
				BatchElement.NumPrimitives = 0;
				BatchElement.MinVertexIndex = 0;
				BatchElement.MaxVertexIndex = 0;

				BatchElement.PrimitiveIdMode = EPrimitiveIdMode::PrimID_ForceZero;
				BatchElement.PrimitiveUniformBuffer = GetUniformBuffer();

				FSceneView const* MainView = ViewFamily.Views[0];
#if USE_INSTANCING
				FGrassUserData* UserData = &Collector.AllocateOneFrameResource<FGrassUserData>();
				BatchElement.UserData = (void*)UserData;
				UserData->InstanceBufferSRV = Buffers.InstanceBufferSRV;
				UserData->LodViewOrigin = (FVector3f)MainView->ViewMatrices.GetViewOrigin(); // LWC_TODO: Precision Loss

	#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
				// Support the freezerendering mode. Use any frozen view state for culling.
				const FViewMatrices* FrozenViewMatrices = MainView->State != nullptr ? MainView->State->GetFrozenViewMatrices() : nullptr;
				if (FrozenViewMatrices != nullptr)
				{
					UserData->LodViewOrigin = (FVector3f)FrozenViewMatrices->GetViewOrigin();
				}
	#endif
#endif

			}

			Collector.AddMesh(ViewIndex, Mesh);
		}
	}
}

// bool FGrassSceneProxy::HasSubprimitiveOcclusionQueries() const
// {
// 	return false;
// }

// const TArray<FBoxSphereBounds>* FGrassSceneProxy::GetOcclusionQueries(const FSceneView* View) const
// {
// 	// return &DefaultOcclusionVolumes;
// }

// void FGrassSceneProxy::BuildOcclusionVolumes(TArrayView<FVector2D> const& InMinMaxData, FIntPoint const& InMinMaxSize, TArrayView<int32> const& InMinMaxMips, int32 InNumLods)
// {
// 	// TODO
// }

// void FGrassSceneProxy::AcceptOcclusionResults(FSceneView const* View, TArray<bool>* Results, int32 ResultsStart, int32 NumResults)
// {
// 	check(IsInRenderingThread());

// 	// TODO
// }
// End FGrassSceneProxy implementations


void FGrassRendererExtension::RegisterExtension()
{
	static bool bInit = false;
	if (!bInit)
	{
		GEngine->GetPreRenderDelegateEx().AddRaw(this, &FGrassRendererExtension::BeginFrame);
		GEngine->GetPostRenderDelegateEx().AddRaw(this, &FGrassRendererExtension::EndFrame);
		bInit = true;
	}
}

void FGrassRendererExtension::ReleaseRHI()
{
	Buffers.Empty();
}

GrassMesh::FPersistentBuffers &FGrassRendererExtension::AddWork(
	const FGrassSceneProxy* InProxy,
	const FSceneView* InMainView, 
	const FSceneView* InCullView)
{
	// If we hit this then BeginFrame()/EndFrame() logic needs fixing in the Scene Renderer.
	if (!ensure(!bInFrame))
	{
		EndFrame();
	}

	// Create workload
	FWorkDesc WorkDesc{};
	WorkDesc.ProxyIndex = SceneProxies.AddUnique(InProxy);
	WorkDesc.MainViewIndex = MainViews.AddUnique(InMainView);
	WorkDesc.CullViewIndex = CullViews.AddUnique(InCullView);
	WorkDesc.BufferIndex = -1;

	// Check for an existing duplicate
	for (const FWorkDesc &It : WorkDescs)
	{
		if (   It.ProxyIndex == WorkDesc.ProxyIndex 
			&& It.MainViewIndex == WorkDesc.MainViewIndex 
			&& It.CullViewIndex == WorkDesc.CullViewIndex 
			&& It.BufferIndex != -1)
		{
			WorkDesc.BufferIndex = It.BufferIndex;
			break;
		}
	}


	// Try to recycle a buffer
	if (WorkDesc.BufferIndex == -1)
	{
		for (int32 BufferIndex = 0; BufferIndex < Buffers.Num(); BufferIndex++)
		{
			if (DiscardIds[BufferIndex] < DiscardId)
			{
				DiscardIds[BufferIndex] = DiscardId;

				WorkDesc.BufferIndex = BufferIndex;
				WorkDescs.Add(WorkDesc);
				break;
			}
		}
	}

	// Allocate new buffer if necessary
	if (WorkDesc.BufferIndex == -1)
	{
		DiscardIds.Add(DiscardId);
		WorkDesc.BufferIndex = Buffers.AddDefaulted();
		WorkDescs.Add(WorkDesc);

		InitializeInstanceBuffers(InProxy, Buffers[WorkDesc.BufferIndex]);
		
	}

	return Buffers[WorkDesc.BufferIndex];
}

void FGrassRendererExtension::BeginFrame(FRDGBuilder &GraphBuilder)
{
	// If we hit this then BeginFrame()/EndFrame() logic needs fixing in the Scene Renderer.
	if (!ensure(!bInFrame))
	{
		EndFrame();
	}
	bInFrame = true;

	if (WorkDescs.Num() > 0)
	{
		SubmitWork(GraphBuilder);
	}
}

void FGrassRendererExtension::EndFrame()
{
	ensure(bInFrame);
	bInFrame = false;

	SceneProxies.Reset();
	MainViews.Reset();
	CullViews.Reset();
	WorkDescs.Reset();

	// Clean the buffer pool
	DiscardId++;

	for (int32 Index = 0; Index < DiscardIds.Num();)
	{
		// if expired (more then 4 millisecons pased from the crteation)
		if (DiscardId - DiscardIds[Index] > 4u)
		{
			ReleaseInstanceBuffers(Buffers[Index]);
			
			Buffers.RemoveAtSwap(Index);
			DiscardIds.RemoveAtSwap(Index);
		}
		else
		{
			++Index;
		}
	}
}

void FGrassRendererExtension::EndFrame(FRDGBuilder &GraphBuilder)
{
	EndFrame();
}

void FGrassRendererExtension::SubmitWork(FRDGBuilder& GraphBuilder)
{
	// Sort work so that we can batch by proxy/view
	WorkDescs.Sort(FWorkDescSort());

	// Add pass to transition all output buffers for writing
	TArray<int32, TInlineAllocator<8>> UsedBufferIndices;
	for (FWorkDesc WorkdDesc : WorkDescs)
	{
		UsedBufferIndices.Add(WorkdDesc.BufferIndex);
	}
	AddPass_TransitionAllDrawBuffers(GraphBuilder, Buffers, UsedBufferIndices, true);

	// Add passes to initialize the output buffers
	for (FWorkDesc WorkDesc : WorkDescs)
	{
		AddPass_InitIndirectArgs(GraphBuilder, GetGlobalShaderMap(GMaxRHIFeatureLevel), Buffers[WorkDesc.BufferIndex]);
	}

	// Iterate workloads and submit work
	const int32 NumWorkItems = WorkDescs.Num();
	int32 WorkIndex = 0;
	while (WorkIndex < NumWorkItems)
	{
		// Gather data per proxy
		FGrassSceneProxy const* Proxy = SceneProxies[WorkDescs[WorkIndex].ProxyIndex];
		GrassMesh::FProxyDesc ProxyDesc
		{
			Proxy->GrassData,
			Proxy->LodStepsRange,
			Proxy->Lambda,
			Proxy->CutoffDistance,

			Proxy->VertexBuffer,
			Proxy->IndexBuffer
		};

		while (WorkIndex < NumWorkItems && SceneProxies[WorkDescs[WorkIndex].ProxyIndex] == Proxy)
		{
			// Gather data per main view
			FSceneView const* MainView = MainViews[WorkDescs[WorkIndex].MainViewIndex];

			GrassMesh::FViewData MainViewData;
			GrassMesh::GetViewData(MainView, MainViewData);

			GrassMesh::FMainViewDesc MainViewDesc;
			MainViewDesc.ViewDebug = MainView;

			MainViewDesc.ViewOrigin = FVector3f(MainViewData.ViewOrigin);
			MainViewDesc.ViewMatrix = FMatrix44f(MainViewData.ViewMatrix);
			MainViewDesc.ViewProjectionMatrix = FMatrix44f(MainViewData.ViewProjectionMatrix);

			// Build volatile graph resources
			GrassMesh::FVolatileResources VolatileResources;
			GrassMesh::InitializeResources(GraphBuilder,
				ProxyDesc, MainViewDesc, VolatileResources);

			// Build graph
			GrassMesh::AddPass_InitBuffers(GraphBuilder, GetGlobalShaderMap(GMaxRHIFeatureLevel), VolatileResources);
			// Build graph
			GrassMesh::AddPass_CullGrassData(
				GraphBuilder, GetGlobalShaderMap(GMaxRHIFeatureLevel),
				VolatileResources, Buffers[WorkDescs[WorkIndex].BufferIndex],
				ProxyDesc, MainViewDesc);


			while (WorkIndex < NumWorkItems && MainViews[WorkDescs[WorkIndex].MainViewIndex] == MainView)
			{
				// Gather data per child view
				const FSceneView* CullView = CullViews[WorkDescs[WorkIndex].CullViewIndex];
				GrassMesh::FViewData CullViewData;
				GrassMesh::GetViewData(CullView, CullViewData);

				// FConvexVolume const *ShadowFrustum = CullView->GetDynamicMeshElementsShadowCullFrustum();
				// FConvexVolume const &Frustum = ShadowFrustum != nullptr && ShadowFrustum->Planes.Num() > 0 ? *ShadowFrustum : CullView->ViewFrustum;
				// const FVector PreShadowTranslation = ShadowFrustum != nullptr ? CullView->GetPreShadowTranslation() : FVector::ZeroVector;

				GrassMesh::FChildViewDesc ChildViewDesc;
				ChildViewDesc.bIsMainView = CullView == MainView;
				ChildViewDesc.ViewDebug = MainView;
				
				ChildViewDesc.ViewMatrix = FMatrix44f(CullViewData.ViewMatrix);
				ChildViewDesc.ViewProjectionMatrix = ChildViewDesc.bIsMainView ? MainViewDesc.ViewProjectionMatrix : FMatrix44f(CullViewData.ViewProjectionMatrix);
				ChildViewDesc.ViewOrigin = ChildViewDesc.bIsMainView ? MainViewDesc.ViewOrigin : FVector3f(CullViewData.ViewOrigin);

				GrassMesh::AddPass_ComputeGrassMesh(
					GraphBuilder, GetGlobalShaderMap(GMaxRHIFeatureLevel),
					VolatileResources, Buffers[WorkDescs[WorkIndex].BufferIndex],
					ProxyDesc, ChildViewDesc);
				

				WorkIndex++;
			}
		}
	}

	// Add pass to transition all output buffers for reading
	AddPass_TransitionAllDrawBuffers(GraphBuilder, Buffers, UsedBufferIndices, false);
}
// End FGrassRendererExtension implementations


