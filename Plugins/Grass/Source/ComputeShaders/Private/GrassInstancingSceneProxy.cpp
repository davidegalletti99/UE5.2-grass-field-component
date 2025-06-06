// Copyright Epic Games, Inc. All Rights Reserved.
// Adapted from the VirtualHeightfieldMesh plugin

#include "GrassInstancingSceneProxy.h"

#include "Chaos/Plane.h"
#include "Kismet/GameplayStatics.h"
#include "Math/UnitConversion.h"

/** Single global instance of the ISM renderer extension. */
TGlobalResource<FGrassInstancingRendererExtension> GrassRendererExtension;

namespace GrassUtils
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

		InBuffers.InstanceBuffer.SafeRelease();
		InBuffers.InstanceBufferUAV.SafeRelease();
		InBuffers.InstanceBufferSRV.SafeRelease();
		
		InBuffers.IndirectArgsBuffer.SafeRelease();
		InBuffers.IndirectArgsBufferUAV.SafeRelease();
	}

	void InitializeInstanceBuffers(
		FGrassInstancingSectionProxy* InSectionProxy,
		GrassUtils::FPersistentBuffers& InBuffers)
	{
		InBuffers.SectionProxy = InSectionProxy;
		const int32 GrassDataNum = InSectionProxy->GrassData.Num();
		{
			FRHIResourceCreateInfo CreateInfo(TEXT("FGrass.GrassDataBuffer"), &InSectionProxy->GrassData);
			constexpr int32 GrassDataSize = sizeof(GrassUtils::FPackedGrassData);
			const int32 GrassDataBufferSize = GrassDataNum * GrassDataSize;
			InBuffers.GrassDataBuffer = RHICreateStructuredBuffer(GrassDataSize, GrassDataBufferSize, BUF_ShaderResource, ERHIAccess::SRVMask, CreateInfo);
			InBuffers.GrassDataBufferSRV = RHICreateShaderResourceView(InBuffers.GrassDataBuffer);
		}
		{
			FRHIResourceCreateInfo CreateInfo(TEXT("FGrass.InstanceBuffer"));
			constexpr int32 InstanceSize = sizeof(GrassUtils::FGrassInstance);
			const int32 InstanceBufferSize = GrassDataNum * InstanceSize;
			InBuffers.InstanceBuffer = RHICreateStructuredBuffer(
				InstanceSize, InstanceBufferSize,
				BUF_UnorderedAccess | BUF_ShaderResource, ERHIAccess::SRVMask, CreateInfo);
			InBuffers.InstanceBufferUAV = RHICreateUnorderedAccessView(InBuffers.InstanceBuffer, PF_R32_UINT);
			InBuffers.InstanceBufferSRV = RHICreateShaderResourceView(InBuffers.InstanceBuffer);
		}
		{
			FRHIResourceCreateInfo CreateInfo(TEXT("FGrass.IndirectArgsBuffer"));
			InBuffers.IndirectArgsBuffer = RHICreateStructuredBuffer(
				IndirectArgsPerElementSize, IndirectArgsBytesSize,
				BUF_UnorderedAccess | BUF_ShaderResource | BUF_DrawIndirect,
				ERHIAccess::IndirectArgs, CreateInfo);
			InBuffers.IndirectArgsBufferUAV = RHICreateUnorderedAccessView(
				InBuffers.IndirectArgsBuffer, PF_R32_UINT);
			InBuffers.IndirectArgsBufferSRV = RHICreateShaderResourceView(InBuffers.IndirectArgsBuffer);
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
		const FGrassInstancingSectionProxy* Proxy,
		const FProxyDesc& ProxyDesc,
		const FMainViewDesc& InMainViewDesc,
		FVolatileResources& OutResources)
	{
		{
			const int GrassDataNum = ProxyDesc.GrassData->Num();
			OutResources.CulledGrassDataBuffer =
				GraphBuilder.CreateBuffer(
					FRDGBufferDesc::CreateStructuredDesc(sizeof(GrassUtils::FPackedGrassData), GrassDataNum),
					TEXT("FGrass.CulledGrassDataBuffer"));
			OutResources.CulledGrassDataBufferSRV = GraphBuilder.CreateSRV(OutResources.CulledGrassDataBuffer);
			OutResources.CulledGrassDataBufferUAV = GraphBuilder.CreateUAV(OutResources.CulledGrassDataBuffer);
		}
	}

	/** Initialise the draw indirect buffer. */
	void AddPass_InitIndirectArgs(
		FRDGBuilder& GraphBuilder,
		const FGlobalShaderMap* InGlobalShaderMap,
		const FPersistentBuffers& InOutputResources)
	{
		TShaderMapRef<GrassUtils::FInitInstanceBuffer_CS> ComputeShader(InGlobalShaderMap);
		
		GrassUtils::FInitInstanceBuffer_CS::FParameters* PassParameters =
			GraphBuilder.AllocParameters<GrassUtils::FInitInstanceBuffer_CS::FParameters>();
		PassParameters->RWIndirectArgsBuffer = InOutputResources.IndirectArgsBufferUAV;
		PassParameters->NumIndices = InOutputResources.SectionProxy->NumIndices;

		const FIntVector GroupCount = FIntVector(1, 1, 1);
		
		FComputeShaderUtils::AddPass<GrassUtils::FInitInstanceBuffer_CS>(
			GraphBuilder,
			RDG_EVENT_NAME("InitInstancingIndirectArgs"),
			ComputeShader, PassParameters,
			GroupCount);
	}

	void AddPass_CullInstances(
		FRDGBuilder& GraphBuilder,
		const FGlobalShaderMap* InGlobalShaderMap,
		const FVolatileResources& InVolatileResources,
		const FPersistentBuffers& InOutputResources,
		const FProxyDesc& ProxyDesc,
		const FMainViewDesc& InViewDesc)
	{
		GrassUtils::FCullInstances_CS::FParameters* PassParameters =
			GraphBuilder.AllocParameters<GrassUtils::FCullInstances_CS::FParameters>();
		const GrassUtils::FCullInstances_CS::FPermutationDomain PermutationVector;
		const TShaderMapRef<GrassUtils::FCullInstances_CS> ComputeShader(InGlobalShaderMap, PermutationVector);

		PassParameters->bIsCullingEnabled = ProxyDesc.bIsCullingEnabled;
		PassParameters->VP_MATRIX = InViewDesc.ViewProjectionMatrix;
		PassParameters->CameraPosition = InViewDesc.ViewOrigin;
		PassParameters->CutoffDistance = ProxyDesc.CutoffDistance;
		PassParameters->GrassDataSize = ProxyDesc.GrassData->Num();
		
		PassParameters->GrassDataBuffer = InOutputResources.GrassDataBufferSRV;
		PassParameters->RWCulledGrassDataBuffer = InVolatileResources.CulledGrassDataBufferUAV;
		PassParameters->RWIndirectArgsBuffer = InOutputResources.IndirectArgsBufferUAV;
		
		const int32 GrassDataNum = ProxyDesc.GrassData->Num();
		const FIntVector GroupCount = FIntVector(FMath::CeilToInt(GrassDataNum / static_cast<float>(MAX_THREADS_PER_GROUP)), 1, 1);
		FComputeShaderUtils::AddPass<GrassUtils::FCullInstances_CS>(
			GraphBuilder,
			RDG_EVENT_NAME("CullGrassData"),
			ComputeShader, PassParameters, GroupCount);
	}

	/** Cull quads and write to the final output buffer. */
	void AddPass_ComputeInstanceData(
		FRDGBuilder& GraphBuilder,
		const FGlobalShaderMap* InGlobalShaderMap,
		const FVolatileResources& InVolatileResources,
		const FPersistentBuffers& InOutputResources,
		const FProxyDesc& ProxyDesc,
		const FMainViewDesc& InViewDesc)
	{
		GrassUtils::FComputeInstanceData_CS::FParameters* PassParameters =
			GraphBuilder.AllocParameters<GrassUtils::FComputeInstanceData_CS::FParameters>();
		const GrassUtils::FComputeInstanceData_CS::FPermutationDomain PermutationVector;
		const TShaderMapRef<GrassUtils::FComputeInstanceData_CS> ComputeShader(InGlobalShaderMap, PermutationVector);

		PassParameters->RWIndirectArgsBuffer = InOutputResources.IndirectArgsBufferUAV;
		PassParameters->CulledGrassDataBuffer = InVolatileResources.CulledGrassDataBufferSRV;
		PassParameters->RWInstanceBuffer = InOutputResources.InstanceBufferUAV;
		
		const int32 GrassDataNum = ProxyDesc.GrassData->Num();
		const FIntVector GroupCount = FIntVector(FMath::CeilToInt(GrassDataNum / static_cast<float>(MAX_THREADS_PER_GROUP)), 1, 1);
		FComputeShaderUtils::AddPass<GrassUtils::FComputeInstanceData_CS>(
			GraphBuilder,
			RDG_EVENT_NAME("ComputeInstanceData"),
			ComputeShader, PassParameters, GroupCount);
	}
}

FGrassInstancingSectionProxy::FGrassInstancingSectionProxy(const ERHIFeatureLevel::Type FeatureLevel)
	: VertexFactory(FeatureLevel)
{
}

// Begin FGrassInstancingSceneProxy implementations
FGrassInstancingSceneProxy::FGrassInstancingSceneProxy(UGrassFieldComponent* InComponent)
	: FPrimitiveSceneProxy(InComponent, NAME_GrassInstancing)
{
	
	const uint16 NumSections = InComponent->GetMeshSections().Num();
	MinMaxLodSteps = InComponent->GetLodStepsRange();
	CutoffDistance = InComponent->GetCutoffDistance();
	bIsCPUCullingEnabled = InComponent->IsCPUCullingEnabled();
	
	Sections.AddZeroed(NumSections);
	for (uint16 SectionIdx = 0; SectionIdx < NumSections; SectionIdx++)
	{
		UGrassMeshSection* SrcSection = InComponent->GetMeshSections()[SectionIdx];
		{
			FGrassInstancingSectionProxy* NewSection = new FGrassInstancingSectionProxy(GetScene().GetFeatureLevel());
			NewSection->GrassData = SrcSection->GetGrassData();
			NewSection->Bounds = SrcSection->GetBounds();
			NewSection->CutoffDistance = CutoffDistance;
			NewSection->bIsGPUCullingEnabled = InComponent->IsGPUCullingEnabled();
			
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
	Material = bValidMaterial ?
		ComponentMaterial->GetRenderProxy() : UMaterial::GetDefaultMaterial(MD_Surface)->GetRenderProxy();
	MaterialRelevance = Material->GetMaterialInterface()->GetRelevance_Concurrent(GetScene().GetFeatureLevel());
}


SIZE_T FGrassInstancingSceneProxy::GetTypeHash() const
{
	static size_t UniquePointer;
	return reinterpret_cast<size_t>(&UniquePointer);
}

uint32 FGrassInstancingSceneProxy::GetMemoryFootprint() const
{
	return (sizeof(*this) + FPrimitiveSceneProxy::GetAllocatedSize());
}

void FGrassInstancingSceneProxy::OnTransformChanged()
{
	// TODO
	// UVToLocal = UVToWorld * GetLocalToWorld().Inverse();

	// Setup a default occlusion volume array containing just the primitive bounds.
	// We use this if disabling the full set of occlusion volumes.
	// DefaultOcclusionVolumes.Reset();
	// DefaultOcclusionVolumes.Add(GetBounds());
}

void FGrassInstancingSceneProxy::CreateRenderThreadResources()
{
	check(IsInRenderingThread());
	bool bIsDataLoaded = true;
	for (const auto& Section: Sections)
	{
		bIsDataLoaded = bIsDataLoaded && Section->GrassData.Num() > 0;

		if (!bIsDataLoaded)
			return;
	}
	
	for (uint32 LodIndex = MinMaxLodSteps.X; LodIndex <= MinMaxLodSteps.Y; LodIndex++)
	{
		FGrassMeshLodData* Lod = new FGrassMeshLodData(LodIndex);
		Lod->InitResources();
		Lods.Add(LodIndex, Lod);
		
	}

	for (const auto& Section: Sections)
	{
		FGrassInstancingVertexFactory* VertexFactory = &Section->VertexFactory;
		VertexFactory->SetData(Lods[MinMaxLodSteps.X]->VertexBuffer);
		
		GrassUtils::InitOrUpdateResource(VertexFactory);
	}
}

void FGrassInstancingSceneProxy::DestroyRenderThreadResources()
{
	check(IsInRenderingThread());
	
	Lods.Empty();
	Sections.Empty();
}

FPrimitiveViewRelevance FGrassInstancingSceneProxy::GetViewRelevance(const FSceneView* View) const
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

void FGrassInstancingSceneProxy::CreateBaseMeshBatch(
	FMeshElementCollector& Collector,
	const FSceneViewFamily& ViewFamily,
	const int32 ViewIndex,
	const FGrassMeshLodData* Lod,
	FGrassInstancingSectionProxy* Section) const
{
	const FSceneView* MainView = ViewFamily.Views[0];
	const FSceneView* CullView = ViewFamily.Views[ViewIndex];

	const GrassUtils::FPersistentBuffers Buffers =
					GrassRendererExtension.AddWork(Section, MainView, CullView);
	
	FMeshBatch& Mesh = Collector.AllocateMesh();
	Mesh.LODIndex = Lod->Steps;
	Mesh.VisualizeLODIndex = Lod->Steps;
	
	Mesh.Elements.SetNumZeroed(1);
	FMeshBatchElement& BatchElement = Mesh.Elements[0];
	
	Mesh.ReverseCulling = IsLocalToWorldDeterminantNegative();
	Mesh.VertexFactory = &Section->VertexFactory;
	Mesh.MaterialRenderProxy = Material;
	Mesh.Type = EPrimitiveType::PT_TriangleList;
	Mesh.DepthPriorityGroup = ESceneDepthPriorityGroup::SDPG_World;
	
	Mesh.bWireframe = AllowDebugViewmodes() && ViewFamily.EngineShowFlags.Wireframe;
	Mesh.bUseWireframeSelectionColoring = IsSelected();
	Mesh.bCanApplyViewModeOverrides = false;
	Mesh.CastRayTracedShadow = false;
	Mesh.CastShadow = false;
	Mesh.bUseForMaterial = true;
	Mesh.bUseForDepthPass = true;
	
	
	// TODO allow for non indirect instanced rendering
	BatchElement.PrimitiveIdMode = EPrimitiveIdMode::PrimID_ForceZero;
	BatchElement.IndexBuffer = Lod->IndexBuffer;
	BatchElement.IndirectArgsBuffer = Buffers.IndirectArgsBuffer;
	BatchElement.IndirectArgsOffset = 0;
	BatchElement.FirstIndex = 0;
	BatchElement.NumPrimitives = 0;
	BatchElement.MinVertexIndex = 0;
	BatchElement.MaxVertexIndex = Lod->NumVertices - 1;
	
 	BatchElement.PrimitiveUniformBuffer = GetUniformBuffer();

	FGrassInstancingUserData* UserData = &Collector.AllocateOneFrameResource<FGrassInstancingUserData>();
	UserData->InstanceBufferSRV = Buffers.InstanceBufferSRV;
	UserData->NumVertices = Lod->NumVertices;

	// TODO: LWC Precision Loss
	UserData->LodViewOrigin = static_cast<FVector3f>(MainView->ViewMatrices.GetViewOrigin());
	
	BatchElement.UserData = static_cast<void*>(UserData);

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	// Support the freeze-rendering mode. Use any frozen view state for culling.
	const FViewMatrices* FrozenViewMatrices = MainView->State != nullptr ?
		MainView->State->GetFrozenViewMatrices() : nullptr;
	
	if (FrozenViewMatrices != nullptr)
	{
		UserData->LodViewOrigin = static_cast<FVector3f>(FrozenViewMatrices->GetViewOrigin());
	}
#endif
	Collector.AddMesh(ViewIndex, Mesh);
}

void FGrassInstancingSceneProxy::GetDynamicMeshElements(
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

	const FSceneView* MainView = ViewFamily.Views[0];
	for (FGrassInstancingSectionProxy* Section : Sections)
	{
		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
		{
			// Check if our mesh is visible from this view
			if (VisibilityMap & (1 << ViewIndex))
			{
				if (Section->GrassData.Num() <= 0)
					continue;
				
				const FSceneView* View = Views[ViewIndex];
				FConvexVolume ViewFrustum = View->ViewFrustum;
				FVector CullOrigin = MainView->ViewMatrices.GetViewOrigin();
				// Support the freeze-rendering mode. Use any frozen view state for culling.
				const FViewMatrices* FrozenViewMatrices = MainView->State != nullptr ?
					MainView->State->GetFrozenViewMatrices() : nullptr;
				if (FrozenViewMatrices != nullptr)
				{
					CullOrigin = FrozenViewMatrices->GetViewOrigin();

					FMatrix ViewMatrix = FrozenViewMatrices->GetViewProjectionMatrix();
					GetViewFrustumBounds(ViewFrustum, ViewMatrix, true, true);
				}
				
				// Chunk distance e frustum culling
				if (this->bIsCPUCullingEnabled)
				{
					if (!ViewFrustum.IntersectBox(
						Section->Bounds.GetCenter(),
						Section->Bounds.GetExtent()))
						continue;
					
					float Distance = FVector::Dist(CullOrigin, Section->Bounds.GetCenter());
					Distance -= Section->Bounds.GetExtent().Length();
				
					if (Distance > CutoffDistance)
						continue;
				}
				
				const uint32 LodIndex = GrassUtils::ComputeLodIndex(
					CullOrigin, Section->Bounds,
					CutoffDistance, MinMaxLodSteps);

				if (!Lods.Contains(LodIndex))
					continue;
				Section->NumIndices = Lods[LodIndex]->NumIndices;
				Section->VertexFactory.SetData(Lods[LodIndex]->VertexBuffer);

				CreateBaseMeshBatch(Collector, ViewFamily, ViewIndex, Lods[LodIndex], Section);
				
			}
		}
	}
}

// bool FGrassInstancingSceneProxy::HasSubprimitiveOcclusionQueries() const
// {
// 	return false;
// }

// const TArray<FBoxSphereBounds>* FGrassInstancingSceneProxy::GetOcclusionQueries(const FSceneView* View) const
// {
// 	// return &DefaultOcclusionVolumes;
// }

// void FGrassInstancingSceneProxy::BuildOcclusionVolumes(TArrayView<FVector2D> const& InMinMaxData, FIntPoint const& InMinMaxSize, TArrayView<int32> const& InMinMaxMips, int32 InNumLods)
// {
// 	// TODO
// }

// void FGrassInstancingSceneProxy::AcceptOcclusionResults(FSceneView const* View, TArray<bool>* Results, int32 ResultsStart, int32 NumResults)
// {
// 	check(IsInRenderingThread());

// 	// TODO
// }
// End FGrassInstancingSceneProxy implementations


void FGrassInstancingRendererExtension::RegisterExtension()
{
	static bool bInit = false;
	if (!bInit)
	{
		GEngine->GetPreRenderDelegateEx().AddRaw(this, &FGrassInstancingRendererExtension::BeginFrame);
		GEngine->GetPostRenderDelegateEx().AddRaw(this, &FGrassInstancingRendererExtension::EndFrame);
		bInit = true;
	}
}

void FGrassInstancingRendererExtension::ReleaseRHI()
{
	Buffers.Empty();
}

GrassUtils::FPersistentBuffers &FGrassInstancingRendererExtension::AddWork(
	FGrassInstancingSectionProxy* InSection,
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
		
 		GrassUtils::InitializeInstanceBuffers(InSection, Buffers[WorkDesc.BufferIndex]);
	}

	return Buffers[WorkDesc.BufferIndex];
}

void FGrassInstancingRendererExtension::BeginFrame(FRDGBuilder &GraphBuilder)
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

void FGrassInstancingRendererExtension::EndFrame()
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

void FGrassInstancingRendererExtension::EndFrame(FRDGBuilder &GraphBuilder)
{
	EndFrame();
}

void FGrassInstancingRendererExtension::SubmitWork(FRDGBuilder& GraphBuilder)
{
	// Sort work so that we can batch by proxy/view
	WorkDescs.Sort(FWorkDescSort());

	// // Add pass to transition all output buffers for writing
	// TArray<int32, TInlineAllocator<8>> UsedBufferIndices;
	// for (FWorkDesc WorkDesc : WorkDescs)
	// {
	// 	UsedBufferIndices.Add(WorkDesc.BufferIndex);
	// }

	// Add passes to initialize the output buffers
	for (FWorkDesc WorkDesc : WorkDescs)
	{
		AddPass_InitIndirectArgs(GraphBuilder, GetGlobalShaderMap(GMaxRHIFeatureLevel), Buffers[WorkDesc.BufferIndex]);
	}

	TMap<FGrassInstancingSectionProxy*, GrassUtils::FProxyDesc> Proxy2Desc;
	TMap<const FSceneView*, GrassUtils::FMainViewDesc> MainView2Desc;
	TMap<const FSceneView*, GrassUtils::FChildViewDesc> ChildViewView2Desc;
	
	// Iterate workloads and submit work
	const int32 NumWorkItems = WorkDescs.Num();
	int32 WorkIndex = 0;
	while (WorkIndex < NumWorkItems)
	{
		// Gather data per proxy
		FGrassInstancingSectionProxy* SectionProxy = SceneProxies[WorkDescs[WorkIndex].ProxyIndex];
		GrassUtils::FProxyDesc ProxyDesc = Proxy2Desc.FindOrAdd(SectionProxy);
		bool bIsNewProxy = !ProxyDesc.IsValid;
		if(bIsNewProxy)
		{
			ProxyDesc.IsValid = true;
			ProxyDesc.bIsCullingEnabled = SectionProxy->bIsGPUCullingEnabled;
			ProxyDesc.GrassData = &SectionProxy->GrassData;
			ProxyDesc.CutoffDistance = SectionProxy->CutoffDistance;
		}
		
		// Gather data per main view
		const FSceneView* MainView = MainViews[WorkDescs[WorkIndex].MainViewIndex];
		GrassUtils::FMainViewDesc MainViewDesc = MainView2Desc.FindOrAdd(MainView);
		if (!MainViewDesc.IsValid)
		{
			GrassUtils::FViewData MainViewData;
			GrassUtils::GetViewData(MainView, MainViewData);

			MainViewDesc.IsValid = true;
			MainViewDesc.ViewDebug = MainView;
			MainViewDesc.ViewOrigin = FVector3f(MainViewData.ViewOrigin);
			MainViewDesc.ViewMatrix = FMatrix44f(MainViewData.ViewMatrix);
			MainViewDesc.ViewProjectionMatrix = FMatrix44f(MainViewData.ViewProjectionMatrix);
		}

		// Build volatile graph resources
		GrassUtils::FVolatileResources VolatileResources;
		GrassUtils::InitializeResources(GraphBuilder,
			SectionProxy,
			ProxyDesc, MainViewDesc, VolatileResources);

		// Build graph
		GrassUtils::AddPass_CullInstances(
			GraphBuilder, GetGlobalShaderMap(GMaxRHIFeatureLevel),
			VolatileResources, Buffers[WorkDescs[WorkIndex].BufferIndex],
			ProxyDesc, MainViewDesc);
		
		GrassUtils::AddPass_ComputeInstanceData(
			GraphBuilder, GetGlobalShaderMap(GMaxRHIFeatureLevel),
			VolatileResources, Buffers[WorkDescs[WorkIndex].BufferIndex],
			ProxyDesc, MainViewDesc);

		// Gather data per child view
		// const FSceneView* CullView = CullViews[WorkDescs[WorkIndex].CullViewIndex];
		// GrassUtils::FChildViewDesc ChildViewDesc = ChildViewView2Desc.FindOrAdd(CullView);
		// if (!ChildViewDesc.IsValid)
		// {
		// 	GrassUtils::FViewData CullViewData;
		// 	GrassUtils::GetViewData(CullView, CullViewData);
		//
		//
		// 	ChildViewDesc.IsValid = true;
		// 	ChildViewDesc.bIsMainView = CullView == MainView;
		// 	ChildViewDesc.ViewDebug = MainView;
		// 	ChildViewDesc.ViewMatrix = FMatrix44f(CullViewData.ViewMatrix);
		// 	ChildViewDesc.ViewProjectionMatrix = ChildViewDesc.bIsMainView ? MainViewDesc.ViewProjectionMatrix : FMatrix44f(CullViewData.ViewProjectionMatrix);
		// 	ChildViewDesc.ViewOrigin = ChildViewDesc.bIsMainView ? MainViewDesc.ViewOrigin : FVector3f(CullViewData.ViewOrigin);
		// }
		

		WorkIndex++;
	}
}
// End FGrassInstancingRendererExtension implementations


