// Copyright Epic Games, Inc. All Rights Reserved.
// Adapted from the VirtualHeightfieldMesh plugin

#include "GrassSceneProxy.h"

/** Single global instance of the ISM renderer extension. */
TGlobalResource<FGrassRendererExtension> GrassRendererExtension;

namespace GrassMesh
{
	static void InitOrUpdateResource(FRenderResource* InResource)
	{
		check(IsInRenderingThread());
		
		if (!InResource->IsInitialized())
			InResource->InitResource();
		else
			InResource->UpdateRHI();
	}
	
	void ReleaseInstanceBuffers(FPersistentBuffers& InBuffers)
	{
		InBuffers.GrassDataBuffer.SafeRelease();
		InBuffers.GrassDataBufferSRV.SafeRelease();
		
		InBuffers.IndirectArgsBuffer.SafeRelease();
		InBuffers.IndirectArgsBufferUAV.SafeRelease();
	}

	void InitializeInstanceBuffers(FGrassSectionProxy* InSectionProxy, FPersistentBuffers& InBuffers)
	{
		InBuffers.SectionProxy = InSectionProxy;
		const int32 GrassDataNum = InSectionProxy->GrassData.Num();
		{
			FRHIResourceCreateInfo CreateInfo(TEXT("FGrass.GrassDataBuffer"), &InSectionProxy->GrassData);
			constexpr int32 GrassDataSize = sizeof(FPackedGrassData);
			const int32 GrassDataBufferSize = GrassDataNum * GrassDataSize;
			InBuffers.GrassDataBuffer = RHICreateStructuredBuffer(GrassDataSize, GrassDataBufferSize, BUF_ShaderResource, ERHIAccess::SRVMask, CreateInfo);
			InBuffers.GrassDataBufferSRV = RHICreateShaderResourceView(InBuffers.GrassDataBuffer);
		}
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
			OverlapUAVs.Add(IndirectArgsBufferUAV);
			
			const ERHIAccess IndirectArgsAccessFrom = bToWrite ? ERHIAccess::IndirectArgs : ERHIAccess::UAVMask;
			const ERHIAccess IndirectArgsAccessTo = bToWrite ? ERHIAccess::UAVMask : ERHIAccess::IndirectArgs;

			TransitionInfos.Add(
				FRHITransitionInfo(IndirectArgsBufferUAV, IndirectArgsAccessFrom, IndirectArgsAccessTo));

		}

		AddPass(GraphBuilder, RDG_EVENT_NAME("TransitionAllDrawBuffers"),
			[bToWrite, OverlapUAVs, TransitionInfos](FRHICommandList& InRHICmdList)
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

		PassParameters->CameraPosition = InViewDesc.ViewOrigin;
		PassParameters->ViewMatrix = InViewDesc.ViewMatrix;
		PassParameters->RWCounter = InVolatileResources.CounterUAV;
		PassParameters->CulledGrassDataBuffer = InVolatileResources.CulledGrassDataBufferSRV;
		PassParameters->RWVertexBuffer = InProxyDesc.VertexBuffer->VertexBufferUAV;
		PassParameters->RWIndexBuffer = InProxyDesc.IndexBuffer->IndexBufferUAV;
		PassParameters->RWIndirectArgsBuffer = InOutputResources.IndirectArgsBufferUAV;


		const int32 GrassDataNum = InProxyDesc.GrassData->Num();
		const FIntVector GroupCount = FIntVector(FMath::CeilToInt(GrassDataNum / 1024.0f), 1, 1);
		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("ComputeGrassMesh"),
			ComputeShader, PassParameters, GroupCount);
	}
}

FGrassSectionProxy::FGrassSectionProxy(const ERHIFeatureLevel::Type FeatureLevel)
: VertexFactory(FeatureLevel)
{
}

// Begin FGrassSceneProxy implementations
FGrassSceneProxy::FGrassSceneProxy(UGrassFieldComponent* InComponent)
	: FPrimitiveSceneProxy(InComponent, NAME_Grass)
{
	
	const uint16 NumSections = InComponent->GetMeshSections().Num();

	Sections.AddZeroed(NumSections);
	for (uint16 SectionIdx = 0; SectionIdx < NumSections; SectionIdx++)
	{
		UGrassMeshSection* SrcSection = InComponent->GetMeshSections()[SectionIdx];
		{
			FGrassSectionProxy* NewSection = new FGrassSectionProxy(GetScene().GetFeatureLevel());
			NewSection->GrassData = SrcSection->GetGrassData();
			NewSection->Bounds = SrcSection->GetBounds();
			NewSection->LodStepsRange = InComponent->GetLodStepsRange();
			NewSection->CutoffDistance = InComponent->GetCutoffDistance();
			
			// Save ref to new section
			Sections[SectionIdx] = NewSection;
		}
	}

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
	for (const auto& Section: Sections)
	{
		const uint32 GrassDataNum = Section->GrassData.Num();
		if (GrassDataNum <= 0)
			continue;

		FGrassVertexBuffer* VertexBuffer = &Section->VertexBuffer;
		VertexBuffer->GrassDataNum = GrassDataNum;
		VertexBuffer->MaxLodSteps = Section->LodStepsRange.Y;

		FGrassIndexBuffer* IndexBuffer = &Section->IndexBuffer;
		IndexBuffer->GrassDataNum = GrassDataNum;
		IndexBuffer->MaxLodSteps = Section->LodStepsRange.Y;

		FGrassVertexFactory* VertexFactory = &Section->VertexFactory;
		VertexFactory->InitData(VertexBuffer);

		GrassMesh::InitOrUpdateResource(VertexBuffer);
		GrassMesh::InitOrUpdateResource(IndexBuffer);
		GrassMesh::InitOrUpdateResource(VertexFactory);
	}
}

void FGrassSceneProxy::DestroyRenderThreadResources()
{
	check(IsInRenderingThread());
	
	for (int i = 0; i < Sections.Num(); i++)
	{
		const auto& Section = Sections[i];
		delete Section;
	}
	
	Sections.Empty();
}

FPrimitiveViewRelevance FGrassSceneProxy::GetViewRelevance(const FSceneView* View) const
{
	constexpr bool bValid = true; // TODO Allow users to modify
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
	const uint32 VisibilityMap,
	FMeshElementCollector& Collector) const
{
	check(IsInRenderingThread());

	if (GrassRendererExtension.IsInFrame())
	{
		// Can't add new work while bInFrame.
		// In UE5 we need to AddWork()/SubmitWork() in two phases: InitViews() and InitViewsAfterPrepass()
		// The main renderer hooks for that don't exist in UE5.0 and are only added in UE5.1
		// That means that for UE5.0 we always hit this for shadow drawing and shadows will not be rendered.
		// Not earlying out here can lead to crashes from buffers being released too soon.
		return;
	}

	const bool bWireframe = AllowDebugViewmodes() && ViewFamily.EngineShowFlags.Wireframe;
	
	const FSceneView* MainView = ViewFamily.Views[0];
	for (const auto& Section : Sections)
	{
		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
		{
			// Check if our mesh is visible from this view
			if (VisibilityMap & (1 << ViewIndex))
			{
				if (Section->GrassData.Num() <= 0)
					continue;
				
				const FSceneView* View = Views[ViewIndex];
				// TODO: Sistemare chunk distance e frustum culling
				{
					float Distance = FVector::DistXY(View->CullingOrigin, Section->Bounds.GetCenter());
					Distance -= FVector::DistXY(FVector::Zero(), Section->Bounds.GetExtent());
					
					if (!View->ViewFrustum.IntersectBox(Section->Bounds.GetCenter(), Section->Bounds.GetExtent())
						|| Distance > Section->CutoffDistance)
						continue;
				}
				
				const GrassMesh::FPersistentBuffers& Buffers =
					GrassRendererExtension.AddWork(Section, MainView, View);
				
				FMeshBatch& Mesh = Collector.AllocateMesh();
				Mesh.Elements.SetNumZeroed(1);
				FMeshBatchElement& BatchElement = Mesh.Elements[0];
				
				Mesh.ReverseCulling = IsLocalToWorldDeterminantNegative();
				Mesh.VertexFactory = &Section->VertexFactory;
				Mesh.MaterialRenderProxy = Material;
				Mesh.Type = EPrimitiveType::PT_TriangleList;
				Mesh.DepthPriorityGroup = ESceneDepthPriorityGroup::SDPG_World;
				
				Mesh.bWireframe = bWireframe;
				Mesh.bUseWireframeSelectionColoring = IsSelected();
				Mesh.bCanApplyViewModeOverrides = false;
				Mesh.CastRayTracedShadow = true;
				Mesh.CastShadow = true;
				Mesh.bUseForMaterial = true;
				Mesh.bUseForDepthPass = true;
				
				bool bHasPrecomputedVolumetricLightmap;
				FMatrix PreviousLocalToWorld;
				int32 SingleCaptureIndex;
				bool bOutputVelocity;
				GetScene().GetPrimitiveUniformShaderParameters_RenderThread(
					GetPrimitiveSceneInfo(),
					bHasPrecomputedVolumetricLightmap,
					PreviousLocalToWorld,
					SingleCaptureIndex,
					bOutputVelocity);
				
				// TODO allow for non indirect instanced rendering
				BatchElement.PrimitiveIdMode = EPrimitiveIdMode::PrimID_ForceZero;
				BatchElement.IndexBuffer = &Section->IndexBuffer;
				BatchElement.IndirectArgsBuffer = Buffers.IndirectArgsBuffer;
				BatchElement.IndirectArgsOffset = 0;
				BatchElement.FirstIndex = 0;
				BatchElement.NumPrimitives = 0;
				BatchElement.MinVertexIndex = 0;
				BatchElement.MaxVertexIndex = 0;
				BatchElement.PrimitiveUniformBuffer = GetUniformBuffer();
				Collector.AddMesh(ViewIndex, Mesh);
			}
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
	FGrassSectionProxy* InSection,
	const FSceneView* InMainView, 
	const FSceneView* InCullView)
{
	// If we hit this then BeginFrame()/EndFrame() logic needs fixing in the Scene Renderer.
	if (!ensure(!bInFrame))
	{
		EndFrame();
	}

	// Create workload
	FWorkDesc WorkDesc;
	WorkDesc.ProxyIndex = SceneProxies.AddUnique(InSection);
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
			if (SceneProxies[WorkDesc.ProxyIndex] != Buffers[BufferIndex].SectionProxy)
				continue;
			
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
		
		InitializeInstanceBuffers(InSection, Buffers[WorkDesc.BufferIndex]);
		
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
		// if expired (more then 4 frames passed from the creation)
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
	for (FWorkDesc WorkDesc : WorkDescs)
	{
		UsedBufferIndices.Add(WorkDesc.BufferIndex);
	}
	AddPass_TransitionAllDrawBuffers(GraphBuilder, Buffers, UsedBufferIndices, true);

	// Add passes to initialize the output buffers
	for (FWorkDesc WorkDesc : WorkDescs)
	{
		AddPass_InitIndirectArgs(GraphBuilder, GetGlobalShaderMap(GMaxRHIFeatureLevel), Buffers[WorkDesc.BufferIndex]);
	}

	TMap<FGrassSectionProxy*, GrassMesh::FProxyDesc> Proxy2Desc;
	TMap<const FSceneView*, GrassMesh::FMainViewDesc> MainViewsSet;
	TMap<const FSceneView*, GrassMesh::FChildViewDesc> CullViewsSet;
	
	// Iterate workloads and submit work
	const int32 NumWorkItems = WorkDescs.Num();
	int32 WorkIndex = 0;
	while (WorkIndex < NumWorkItems)
	{
		// Gather data per proxy
		FGrassSectionProxy* SectionProxy = SceneProxies[WorkDescs[WorkIndex].ProxyIndex];
		GrassMesh::FProxyDesc& ProxyDesc = Proxy2Desc.FindOrAdd(SectionProxy);
		if(!ProxyDesc.IsValid)
		{
			ProxyDesc.IsValid = true;
			ProxyDesc.GrassData = &SectionProxy->GrassData;
			ProxyDesc.LodStepsRange = SectionProxy->LodStepsRange;
			ProxyDesc.CutOffDistance = SectionProxy->CutoffDistance;
			ProxyDesc.VertexBuffer = &SectionProxy->VertexBuffer;
			ProxyDesc.IndexBuffer = &SectionProxy->IndexBuffer;
		}
		
		// Gather data per main view
		const FSceneView* MainView = MainViews[WorkDescs[WorkIndex].MainViewIndex];
		GrassMesh::FMainViewDesc& MainViewDesc = MainViewsSet.FindOrAdd(MainView);
		if (!MainViewDesc.IsValid)
		{
			GrassMesh::FViewData MainViewData;
			GrassMesh::GetViewData(MainView, MainViewData);

			MainViewDesc.IsValid = true;
			MainViewDesc.ViewDebug = MainView;
			MainViewDesc.ViewOrigin = FVector3f(MainViewData.ViewOrigin);
			MainViewDesc.ViewMatrix = FMatrix44f(MainViewData.ViewMatrix);
			MainViewDesc.ViewProjectionMatrix = FMatrix44f(MainViewData.ViewProjectionMatrix);
		}

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

		// Gather data per child view
		const FSceneView* CullView = CullViews[WorkDescs[WorkIndex].CullViewIndex];
		GrassMesh::FChildViewDesc ChildViewDesc = CullViewsSet.FindOrAdd(CullView);
		if (!ChildViewDesc.IsValid)
		{
			GrassMesh::FViewData CullViewData;
			GrassMesh::GetViewData(CullView, CullViewData);
		

			ChildViewDesc.IsValid = true;
			ChildViewDesc.bIsMainView = CullView == MainView;
			ChildViewDesc.ViewDebug = MainView;
			ChildViewDesc.ViewMatrix = FMatrix44f(CullViewData.ViewMatrix);
			ChildViewDesc.ViewProjectionMatrix = ChildViewDesc.bIsMainView ? MainViewDesc.ViewProjectionMatrix : FMatrix44f(CullViewData.ViewProjectionMatrix);
			ChildViewDesc.ViewOrigin = ChildViewDesc.bIsMainView ? MainViewDesc.ViewOrigin : FVector3f(CullViewData.ViewOrigin);
		}
		
		GrassMesh::AddPass_ComputeGrassMesh(
			GraphBuilder, GetGlobalShaderMap(GMaxRHIFeatureLevel),
			VolatileResources, Buffers[WorkDescs[WorkIndex].BufferIndex],
			ProxyDesc, ChildViewDesc);
		

		WorkIndex++;
	}

	// TODO: Capire a cosa serve
	// Add pass to transition all output buffers for reading
	AddPass_TransitionAllDrawBuffers(GraphBuilder, Buffers, UsedBufferIndices, false);
}
// End FGrassRendererExtension implementations


