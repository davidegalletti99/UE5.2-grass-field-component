// Copyright Epic Games, Inc. All Rights Reserved.
// Adapted from the VirtualHeightfieldMesh plugin

#include "GrassSceneProxy.h"

namespace GrassMesh
{
	/** Initialize the FDrawInstanceBuffers objects. */
	void InitializeInstanceBuffers(const FGrassSceneProxy* Proxy, FDrawInstanceBuffers& InBuffers);

	/** Release the FDrawInstanceBuffers objects. */
	void ReleaseInstanceBuffers(FDrawInstanceBuffers & InBuffers)
	{
		/*InBuffers.VertexBuffer.SafeRelease();
		InBuffers.VertexBufferUAV.SafeRelease();
		InBuffers.IndexBuffer.SafeRelease();
		InBuffers.IndexBufferUAV.SafeRelease();*/
		InBuffers.InstanceBuffer.SafeRelease();
		InBuffers.InstanceBufferUAV.SafeRelease();
		InBuffers.InstanceBufferSRV.SafeRelease();
		InBuffers.IndirectArgsBuffer.SafeRelease();
		InBuffers.IndirectArgsBufferUAV.SafeRelease();
	}
}

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
	GrassMesh::FDrawInstanceBuffers& AddWork(
		const FGrassSceneProxy* InProxy,
		const FSceneView* InMainView,
		const FSceneView* InCullView);

	/** Submit all the work added by AddWork(). The work fills all of the buffers ready for use by the referencing mesh batches. */
	void SubmitWork(FRDGBuilder & GraphBuilder);

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
	TArray<GrassMesh::FDrawInstanceBuffers> Buffers;
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
		int32 ProxyIndex;
		int32 MainViewIndex;
		int32 CullViewIndex;
		int32 BufferIndex;
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

/** Single global instance of the ISM renderer extension. */
TGlobalResource<FGrassRendererExtension> GrassRendererExtension;

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

GrassMesh::FDrawInstanceBuffers &FGrassRendererExtension::AddWork(
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
	for (FWorkDesc &It : WorkDescs)
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

		GrassMesh::InitializeInstanceBuffers(InProxy, Buffers[WorkDesc.BufferIndex]);
		
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
		if (DiscardId - DiscardIds[Index] > 4u)
		{
			GrassMesh::ReleaseInstanceBuffers(Buffers[Index]);
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

const static FName NAME_Grass(TEXT("Grass"));

FGrassSceneProxy::FGrassSceneProxy(UGrassFieldComponent* InComponent, 
	TResourceArray<GrassMesh::FPackedGrassData>* GrassData, 
	FUintVector2 LodStepsRange, float Lambda, float CutoffDistance)
	: FPrimitiveSceneProxy(InComponent, NAME_Grass),
	VertexFactory(nullptr), GrassData(GrassData), LodStepsRange(LodStepsRange), Lambda(Lambda), CutoffDistance(CutoffDistance)
{
	GrassRendererExtension.RegisterExtension();

	// They have some LOD, but considered static as the LODs (are intended to) represent the same static surface.
	// TODO Check if this allows WPO
	bHasDeformableMesh = false;

	UMaterialInterface* ComponentMaterial = InComponent->GetMaterial();
	// TODO MATUSAGE
	const bool bValidMaterial = ComponentMaterial != nullptr;
	Material = bValidMaterial ? ComponentMaterial->GetRenderProxy() : UMaterial::GetDefaultMaterial(MD_Surface)->GetRenderProxy();
	MaterialRelevance = Material->GetMaterialInterface()->GetRelevance_Concurrent(GetScene().GetFeatureLevel());
}

FGrassSceneProxy::FGrassSceneProxy(UGrassFieldComponent *InComponent)
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

	int32 GrassDataNum = GrassData->Num() > 0 ? GrassData->Num() : 1;
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

	UE_LOG(LogTemp, Warning, TEXT("DestroyRenderThreadResources"));
	check(IsInRenderingThread());
	if (VertexFactory != nullptr)
	{
		VertexFactory->ReleaseResource();
		delete VertexFactory;
		VertexFactory = nullptr;
	}
}

FPrimitiveViewRelevance FGrassSceneProxy::GetViewRelevance(const FSceneView *View) const
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
	const TArray<const FSceneView *> &Views, 
	const FSceneViewFamily &ViewFamily, 
	uint32 VisibilityMap, 
	FMeshElementCollector &Collector) const
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
			GrassMesh::FDrawInstanceBuffers &Buffers = 
				GrassRendererExtension.AddWork(this, ViewFamily.Views[0], Views[ViewIndex]);
			
			FMeshBatch &Mesh = Collector.AllocateMesh();
			Mesh.bWireframe = AllowDebugViewmodes() & ViewFamily.EngineShowFlags.Wireframe;
			Mesh.bUseWireframeSelectionColoring = IsSelected();
			Mesh.VertexFactory = VertexFactory;
			Mesh.MaterialRenderProxy = Material;
			Mesh.CastRayTracedShadow = true;
			Mesh.ReverseCulling = IsLocalToWorldDeterminantNegative();
			Mesh.Type = EPrimitiveType::PT_TriangleList;
			Mesh.DepthPriorityGroup = ESceneDepthPriorityGroup::SDPG_World;
			Mesh.bCanApplyViewModeOverrides = true;
			Mesh.bUseForMaterial = true;
			Mesh.CastShadow = true;
			Mesh.bUseForDepthPass = true;

			Mesh.Elements.SetNumZeroed(1);
			{
				FMeshBatchElement &BatchElement = Mesh.Elements[0];
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

				FGrassUserData *UserData = &Collector.AllocateOneFrameResource<FGrassUserData>();
				BatchElement.UserData = (void *)UserData;

				UserData->InstanceBufferSRV = Buffers.InstanceBufferSRV;

				FSceneView const *MainView = ViewFamily.Views[0];
				UserData->LodViewOrigin = (FVector3f)MainView->ViewMatrices.GetViewOrigin(); // LWC_TODO: Precision Loss

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
				// Support the freezerendering mode. Use any frozen view state for culling.
				const FViewMatrices *FrozenViewMatrices = MainView->State != nullptr ? MainView->State->GetFrozenViewMatrices() : nullptr;
				if (FrozenViewMatrices != nullptr)
				{
					UserData->LodViewOrigin = (FVector3f)FrozenViewMatrices->GetViewOrigin();
				}
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

namespace GrassMesh
{
	/* Keep indirect args offsets in sync with ISM.usf. */
	static const int32 IndirectArgsByteOffset_FinalCull = 0;
	static const int32 IndirectArgsByteSize = 4 * sizeof(uint32);

	/** Compute shader to initialize all buffers, including adding the lowest mip page(s) to the QuadBuffer. */
	class COMPUTESHADERS_API FInitBuffers_CS : public FGlobalShader
	{
	public:
		DECLARE_GLOBAL_SHADER(FInitBuffers_CS);
		SHADER_USE_PARAMETER_STRUCT(FInitBuffers_CS, FGlobalShader);

		BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
			SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint32>, RWIndirectArgsBuffer)
			SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint32>, RWCounter)
		END_SHADER_PARAMETER_STRUCT()

		static bool ShouldCompilePermutation(FGlobalShaderPermutationParameters const &Parameters)
		{
			return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM6);
		}
	};

	IMPLEMENT_GLOBAL_SHADER(FInitBuffers_CS, "/Shaders/GrassCompute.usf", "InitBuffersCS", SF_Compute);

	class COMPUTESHADERS_API FCullGrassData_CS : public FGlobalShader
	{

	public:
		DECLARE_GLOBAL_SHADER(FCullGrassData_CS);
		SHADER_USE_PARAMETER_STRUCT(FCullGrassData_CS, FGlobalShader);

		BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
			SHADER_PARAMETER(FVector3f, CameraPosition)
			SHADER_PARAMETER(float, CutoffDistance)
			SHADER_PARAMETER(FMatrix44f, VP_MATRIX)
			SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint32>, RWCounter)
			SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FPackedGrassData>, GrassDataBuffer)
			SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<FPackedLodGrassData>, RWCulledGrassDataBuffer)
			SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint32>, RWIndirectArgsBuffer)
		END_SHADER_PARAMETER_STRUCT()

		static bool ShouldCompilePermutation(FGlobalShaderPermutationParameters const& Parameters)
		{
			return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM6);
		}
	};

	IMPLEMENT_GLOBAL_SHADER(FCullGrassData_CS, "/Shaders/GrassCompute.usf", "CullGrassDataCS", SF_Compute);


	class COMPUTESHADERS_API FComputeGrassMesh_CS : public FGlobalShader
	{

	public:
		DECLARE_GLOBAL_SHADER(FComputeGrassMesh_CS);
		SHADER_USE_PARAMETER_STRUCT(FComputeGrassMesh_CS, FGlobalShader);


		class FReuseCullDim : SHADER_PERMUTATION_BOOL("REUSE_CULL");
		using FPermutationDomain = TShaderPermutationDomain<FReuseCullDim>;

		BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
			SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint32>, RWCounter)
			SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FPackedLodGrassData>, CulledGrassDataBuffer)
			SHADER_PARAMETER_UAV(RWStructuredBuffer<FGrassVertex>, RWVertexBuffer)
			SHADER_PARAMETER_UAV(RWStructuredBuffer<uint32>, RWIndexBuffer)
			SHADER_PARAMETER_UAV(RWStructuredBuffer<FGrassInstance>, RWInstanceBuffer)
			SHADER_PARAMETER_UAV(RWStructuredBuffer<uint32>, RWIndirectArgsBuffer)
			SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint32>, IndirectArgsBufferSRV)
			RDG_BUFFER_ACCESS(IndirectArgsBuffer, ERHIAccess::IndirectArgs)
		END_SHADER_PARAMETER_STRUCT()

		static bool ShouldCompilePermutation(FGlobalShaderPermutationParameters const& Parameters)
		{
			return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM6);
		}
	};

	IMPLEMENT_GLOBAL_SHADER(FComputeGrassMesh_CS, "/Shaders/GrassCompute.usf", "ComputeGrassMeshCS", SF_Compute);

	/** InitInstanceBuffer compute shader. */
	class COMPUTESHADERS_API FInitInstanceBuffer_CS : public FGlobalShader
	{
	public:
		DECLARE_GLOBAL_SHADER(FInitInstanceBuffer_CS);
		SHADER_USE_PARAMETER_STRUCT(FInitInstanceBuffer_CS, FGlobalShader);

		BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_UAV(RWBuffer<uint>, RWIndirectArgsBuffer)
		END_SHADER_PARAMETER_STRUCT()

		static bool ShouldCompilePermutation(FGlobalShaderPermutationParameters const &Parameters)
		{
			return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM6);
		}
	};

	IMPLEMENT_GLOBAL_SHADER(FInitInstanceBuffer_CS, "/Shaders/GrassCompute.usf", "InitIndirectArgsCS", SF_Compute);

	/** View matrices that can be frozen in freezerendering mode. */
	struct COMPUTESHADERS_API FViewData
	{
		FVector ViewOrigin;
		FMatrix ProjectionMatrix;
		FMatrix ViewProjectionMatrix;
		FConvexVolume ViewFrustum;
		bool bViewFrozen;
	};

	/** Fill the FViewData from an FSceneView respecting the freezerendering mode. */
	void GetViewData(FSceneView const *InSceneView, FViewData &OutViewData)
	{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		const FViewMatrices *FrozenViewMatrices = InSceneView->State != nullptr ? InSceneView->State->GetFrozenViewMatrices() : nullptr;
		if (FrozenViewMatrices != nullptr)
		{
			OutViewData.ViewOrigin = FrozenViewMatrices->GetViewOrigin();
			OutViewData.ProjectionMatrix = FrozenViewMatrices->GetProjectionMatrix();
			OutViewData.ViewProjectionMatrix = FrozenViewMatrices->GetViewProjectionMatrix();
			GetViewFrustumBounds(OutViewData.ViewFrustum, FrozenViewMatrices->GetViewProjectionMatrix(), true);
			OutViewData.bViewFrozen = true;
		}
		else
#endif
		{
			OutViewData.ViewOrigin = InSceneView->ViewMatrices.GetViewOrigin();
			OutViewData.ProjectionMatrix = InSceneView->ViewMatrices.GetProjectionMatrix();
			OutViewData.ViewProjectionMatrix = InSceneView->ViewMatrices.GetViewProjectionMatrix();
			OutViewData.ViewFrustum = InSceneView->ViewFrustum;
			OutViewData.bViewFrozen = false;
		}
	}

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
		FSceneView const *ViewDebug;
		FVector3f ViewOrigin;
		FMatrix44f ViewProjectionMatrix;
		float LodBiasScale;
		FVector4 LodDistances;
		FVector4 Planes[5];
	};

	/** View description used for culling in the child view. */
	struct COMPUTESHADERS_API FChildViewDesc
	{
		FSceneView const *ViewDebug;
		FVector3f ViewOrigin;
		FMatrix44f ViewProjectionMatrix;
		bool bIsMainView;
		FVector4 Planes[5];
	};

	/** Structure to carry RDG resources. */
	struct COMPUTESHADERS_API FVolatileResources
	{
		FRDGBufferRef GrassDataBuffer;
		FRDGBufferUAVRef GrassDataBufferUAV;
		FRDGBufferSRVRef GrassDataBufferSRV;

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

	/** Initialize the FDrawInstanceBuffers objects. */
	void InitializeInstanceBuffers(const FGrassSceneProxy* InProxy, FDrawInstanceBuffers& InBuffers)
	{
		int32 GrassDataNum = InProxy->GrassData->Num();

		uint32 MaxLodSteps = InProxy->LodStepsRange.Y;
		int32 MaxVertexBufferSize = ((MaxLodSteps * 2) + 1) * GrassDataNum;
		{
			FRHIResourceCreateInfo CreateInfo(TEXT("FGrass.InstanceBuffer"));
			const int32 InstanceSize = sizeof(FGrassInstance);
			const int32 InstanceBufferSize = GrassDataNum * InstanceSize;
			InBuffers.InstanceBuffer = RHICreateStructuredBuffer(InstanceSize, InstanceBufferSize, BUF_UnorderedAccess | BUF_ShaderResource, ERHIAccess::SRVMask , CreateInfo);
			InBuffers.InstanceBufferUAV = RHICreateUnorderedAccessView(InBuffers.InstanceBuffer, PF_R32_UINT);
			InBuffers.InstanceBufferSRV = RHICreateShaderResourceView(InBuffers.InstanceBuffer);
		}
		{
			FRHIResourceCreateInfo CreateInfo(TEXT("FGrass.InstanceIndirectArgsBuffer"));
			const int32 IndirectArgsSize = sizeof(uint32);
			const int32 IndirectArgsBufferSize = 5 * IndirectArgsSize;
			InBuffers.IndirectArgsBuffer = RHICreateVertexBuffer(
				IndirectArgsBufferSize, BUF_UnorderedAccess | BUF_DrawIndirect, 
				ERHIAccess::IndirectArgs, CreateInfo);
			InBuffers.IndirectArgsBufferUAV = RHICreateUnorderedAccessView(
				InBuffers.IndirectArgsBuffer, PF_R32_UINT);
		}

	}

	/** Initialize the volatile resources used in the render graph. */
	void InitializeResources(
		FRDGBuilder& GraphBuilder, 
		const FProxyDesc& ProxyDesc, 
		const FMainViewDesc& InMainViewDesc, 
		FVolatileResources& OutResources)
	{
		int32 GrassDataNum = ProxyDesc.GrassData->Num();
		{
			const void* RawGrassData = (void*)ProxyDesc.GrassData->GetData();
			OutResources.GrassDataBuffer = CreateStructuredBuffer(
				GraphBuilder,
				TEXT("GrassMesh.GrassDataBuffer"),
				sizeof(FPackedGrassData), GrassDataNum,
				RawGrassData, sizeof(FPackedGrassData) * GrassDataNum);
			OutResources.GrassDataBufferUAV = GraphBuilder.CreateUAV(OutResources.GrassDataBuffer);
			OutResources.GrassDataBufferSRV = GraphBuilder.CreateSRV(OutResources.GrassDataBuffer);
		}
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
	void AddPass_TransitionAllDrawBuffers(FRDGBuilder & GraphBuilder, TArray<GrassMesh::FDrawInstanceBuffers> const &Buffers, TArrayView<int32> const &BufferIndices, bool bToWrite)
	{
		TArray<FRHIUnorderedAccessView *> OverlapUAVs;
		OverlapUAVs.Reserve(BufferIndices.Num());

		TArray<FRHITransitionInfo> TransitionInfos;
		TransitionInfos.Reserve(BufferIndices.Num() * 2);

		for (int32 BufferIndex : BufferIndices)
		{
			FRHIUnorderedAccessView *IndirectArgsBufferUAV = Buffers[BufferIndex].IndirectArgsBufferUAV;
			FRHIUnorderedAccessView *InstanceBufferUAV = Buffers[BufferIndex].InstanceBufferUAV;

			OverlapUAVs.Add(IndirectArgsBufferUAV);

			ERHIAccess DrawAccessFrom = bToWrite ? ERHIAccess::SRVMask : ERHIAccess::UAVMask;
			ERHIAccess DrawAccessTo = bToWrite ? ERHIAccess::UAVMask : ERHIAccess::SRVMask;

			ERHIAccess IndirectArgsAccessFrom = bToWrite ? ERHIAccess::IndirectArgs : ERHIAccess::UAVMask;
			ERHIAccess IndirectArgsAccessTo = bToWrite ? ERHIAccess::UAVMask : ERHIAccess::IndirectArgs;

			TransitionInfos.Add(
				FRHITransitionInfo(IndirectArgsBufferUAV, IndirectArgsAccessFrom, IndirectArgsAccessTo));
			TransitionInfos.Add(
				FRHITransitionInfo(InstanceBufferUAV, DrawAccessFrom, DrawAccessTo));
		}

		AddPass(GraphBuilder, RDG_EVENT_NAME("TransitionAllDrawBuffers"), [bToWrite, OverlapUAVs, TransitionInfos](FRHICommandList &InRHICmdList)
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
		FVolatileResources &InVolatileResources)
	{
		TShaderMapRef<FInitBuffers_CS> ComputeShader(InGlobalShaderMap);

		FInitBuffers_CS::FParameters *PassParameters =
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
		FDrawInstanceBuffers& InOutputResources)
	{
		TShaderMapRef<FInitInstanceBuffer_CS> ComputeShader(InGlobalShaderMap);

		FInitInstanceBuffer_CS::FParameters *PassParameters = 
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
		FDrawInstanceBuffers& InOutputResources,
		const FProxyDesc& ProxyDesc,
		const FMainViewDesc& InViewDesc)
	{
		FCullGrassData_CS::FParameters* PassParameters =
			GraphBuilder.AllocParameters<FCullGrassData_CS::FParameters>();
		
		PassParameters->VP_MATRIX = InViewDesc.ViewProjectionMatrix;
		PassParameters->CameraPosition = InViewDesc.ViewOrigin;
		PassParameters->CutoffDistance = ProxyDesc.CutOffDistance;
		PassParameters->GrassDataBuffer = InVolatileResources.GrassDataBufferSRV;
		PassParameters->RWCulledGrassDataBuffer = InVolatileResources.CulledGrassDataBufferUAV;
		PassParameters->RWIndirectArgsBuffer = InVolatileResources.IndirectArgsBufferUAV;
		PassParameters->RWCounter = InVolatileResources.CounterUAV;

		FCullGrassData_CS::FPermutationDomain PermutationVector;
		TShaderMapRef<FCullGrassData_CS> ComputeShader(InGlobalShaderMap, PermutationVector);

		int32 GrassDataNum = ProxyDesc.GrassData->Num();
		FIntVector GroupCount = FIntVector(FMath::CeilToInt(GrassDataNum / 1024.0f), 1, 1);
		FComputeShaderUtils::AddPass<FCullGrassData_CS>(
			GraphBuilder,
			RDG_EVENT_NAME("CullGrassData"), 
			ComputeShader, PassParameters, GroupCount);
	}

	void AddPass_ComputeGrassMesh(
		FRDGBuilder& GraphBuilder, 
		FGlobalShaderMap* InGlobalShaderMap,
		FVolatileResources& InVolatileResources, 
		FDrawInstanceBuffers& InOutputResources, 
		const FProxyDesc& InProxyDesc,
		const FChildViewDesc& InViewDesc)
	{
		FComputeGrassMesh_CS::FParameters *PassParameters =
			GraphBuilder.AllocParameters<FComputeGrassMesh_CS::FParameters>();
		PassParameters->RWCounter = InVolatileResources.CounterUAV;
		PassParameters->CulledGrassDataBuffer = InVolatileResources.CulledGrassDataBufferSRV;
		PassParameters->RWInstanceBuffer = InOutputResources.InstanceBufferUAV;
		PassParameters->RWVertexBuffer = InProxyDesc.VertexBuffer->VertexBufferUAV;
		PassParameters->RWIndexBuffer = InProxyDesc.IndexBuffer->IndexBufferUAV;
		PassParameters->RWIndirectArgsBuffer = InOutputResources.IndirectArgsBufferUAV;
		PassParameters->IndirectArgsBuffer = InVolatileResources.IndirectArgsBuffer;
		PassParameters->IndirectArgsBufferSRV = InVolatileResources.IndirectArgsBufferSRV;

		int32 IndirectArgOffset = GrassMesh::IndirectArgsByteOffset_FinalCull;

		FComputeGrassMesh_CS::FPermutationDomain PermutationVector;
		//PermutationVector.Set<FComputeGrassMesh_CS::FReuseCullDim>(InViewDesc.bIsMainView);
		TShaderMapRef<FComputeGrassMesh_CS> ComputeShader(InGlobalShaderMap, PermutationVector);

		int32 GrassDataNum = InProxyDesc.GrassData->Num();
		FIntVector GroupCount = FIntVector(FMath::CeilToInt(GrassDataNum / 1024.0f), 1, 1);
		FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("ComputeGrassMesh"),
				ComputeShader, PassParameters, GroupCount);
	}
}

void FGrassRendererExtension::SubmitWork(FRDGBuilder &GraphBuilder)
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
		FGrassSceneProxy const *Proxy = SceneProxies[WorkDescs[WorkIndex].ProxyIndex];
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
			FSceneView const *MainView = MainViews[WorkDescs[WorkIndex].MainViewIndex];

			GrassMesh::FViewData MainViewData;
			GrassMesh::GetViewData(MainView, MainViewData);

			GrassMesh::FMainViewDesc MainViewDesc;
			MainViewDesc.ViewDebug = MainView;

			MainViewDesc.ViewOrigin = FVector3f(MainViewData.ViewOrigin);
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
				FSceneView const* CullView = CullViews[WorkDescs[WorkIndex].CullViewIndex];
				GrassMesh::FViewData CullViewData;
				GrassMesh::GetViewData(CullView, CullViewData);

				// FConvexVolume const *ShadowFrustum = CullView->GetDynamicMeshElementsShadowCullFrustum();
				// FConvexVolume const &Frustum = ShadowFrustum != nullptr && ShadowFrustum->Planes.Num() > 0 ? *ShadowFrustum : CullView->ViewFrustum;
				// const FVector PreShadowTranslation = ShadowFrustum != nullptr ? CullView->GetPreShadowTranslation() : FVector::ZeroVector;

				GrassMesh::FChildViewDesc ChildViewDesc;
				ChildViewDesc.ViewDebug = MainView;
				ChildViewDesc.bIsMainView = CullView == MainView;
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
