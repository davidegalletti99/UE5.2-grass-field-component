// Copyright Epic Games, Inc. All Rights Reserved.

#include "GrassVertexFactory.h"

#include "Engine/Engine.h"
#include "EngineGlobals.h"
#include "Materials/Material.h"
#include "MeshMaterialShader.h"
#include "RHIStaticStates.h"
#include "ShaderParameters.h"
#include "DataDrivenShaderPlatformInfo.h"
#include "MaterialDomain.h"
#include "MeshDrawShaderBindings.h"
#include "ShaderParameterUtils.h"

IMPLEMENT_GLOBAL_SHADER_PARAMETER_STRUCT(FGrassParameters, "GrassParams");


void FGrassVertexBuffer::InitRHI()
{
	const int32 VerticesNum = GrassDataNum * (MaxLodSteps * 2 + 1);
	const int32 VertexBufferSize = VerticesNum * sizeof(GrassMesh::FGrassVertex);
	FRHIResourceCreateInfo CreateInfo(TEXT("FGrass.VertexBuffer"));
	VertexBufferRHI = RHICreateVertexBuffer(VertexBufferSize, BUF_UnorderedAccess | BUF_DrawIndirect, CreateInfo);
	VertexBufferUAV = RHICreateUnorderedAccessView(VertexBufferRHI, PF_R32_UINT);
}

void FGrassIndexBuffer::InitRHI()
{
	const int32 IndicesNum = GrassDataNum * (MaxLodSteps * 2 - 1) * 3;
	const int32 IndexBufferSize = IndicesNum * sizeof(int32);
	FRHIResourceCreateInfo CreateInfo(TEXT("FGrass.IndexBuffer"));
	IndexBufferRHI = RHICreateVertexBuffer(IndexBufferSize, BUF_UnorderedAccess | BUF_DrawIndirect, CreateInfo);
	IndexBufferUAV = RHICreateUnorderedAccessView(IndexBufferRHI, PF_R32_UINT);

}

FGrassVertexFactory::FGrassVertexFactory(const ERHIFeatureLevel::Type InFeatureLevel)
		: FVertexFactory(InFeatureLevel)
{
	// TODO: Sistemare per poter usare i parametri
	FGrassParameters UniformParams;
	Params = UniformParams;
}

FGrassVertexFactory::~FGrassVertexFactory()
{
}

void FGrassVertexFactory::SetData(FGrassVertexDataType& InData)
{
	Data = InData;

	if (IsInitialized())
		UpdateRHI();
	else
		InitResource();
}

void FGrassVertexFactory::InitData(FVertexBuffer* VertexBuffer)
{
	ENQUEUE_RENDER_COMMAND(InitGrassVertexFactory)([VertexBuffer, this](FRHICommandListImmediate& RHICmdListImmediate)
		{
			// Initialize the vertex factory's stream components.
			FGrassVertexDataType NewData;
			NewData.PositionComponent = STRUCTMEMBER_VERTEXSTREAMCOMPONENT(VertexBuffer, GrassMesh::FGrassVertex, Position, VET_Float3);
			NewData.UVComponent = STRUCTMEMBER_VERTEXSTREAMCOMPONENT(VertexBuffer, GrassMesh::FGrassVertex, UV, VET_Float2);
			NewData.TangentBasisComponents[0] = STRUCTMEMBER_VERTEXSTREAMCOMPONENT(VertexBuffer, GrassMesh::FGrassVertex, TangentX, VET_Float3);
			NewData.TangentBasisComponents[1] = STRUCTMEMBER_VERTEXSTREAMCOMPONENT(VertexBuffer, GrassMesh::FGrassVertex, TangentZ, VET_Float4);
			this->SetData(NewData);
		});
}

void FGrassVertexFactory::InitRHI()
{
	UniformBuffer = FGrassBufferRef::CreateUniformBufferImmediate(Params, UniformBuffer_MultiFrame);

	check(Streams.Num() == 0);
	FVertexDeclarationElementList Elements;
	if (Data.PositionComponent.VertexBuffer != nullptr)
		Elements.Add(AccessStreamComponent(Data.PositionComponent, 0));

	if (Data.UVComponent.VertexBuffer != nullptr)
		Elements.Add(AccessStreamComponent(Data.UVComponent, 1));
	
	if (Data.TangentBasisComponents[0].VertexBuffer != nullptr)
		Elements.Add(AccessStreamComponent(Data.TangentBasisComponents[0], 2));
	
	if (Data.TangentBasisComponents[1].VertexBuffer != nullptr)
		Elements.Add(AccessStreamComponent(Data.TangentBasisComponents[1], 3));

#if USE_INSTANCING
	AddPrimitiveIdStreamElement(EVertexInputStreamType::Default, Elements, 9, 4);
#endif
	
	InitDeclaration(Elements, EVertexInputStreamType::Default);
	check(Streams.Num() > 0);
}

void FGrassVertexFactory::ReleaseRHI()
{
	UniformBuffer.SafeRelease();
	FVertexFactory::ReleaseRHI();
}

bool FGrassVertexFactory::ShouldCompilePermutation(const FVertexFactoryShaderPermutationParameters &Parameters)
{
	if (!IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM6))
	{
		return false;
	}
	const bool Result = Parameters.MaterialParameters.bIsSpecialEngineMaterial
			|| Parameters.MaterialParameters.MaterialDomain == MD_Surface;
	
	return Result;
}

void FGrassVertexFactory::ModifyCompilationEnvironment(const FVertexFactoryShaderPermutationParameters &Parameters, FShaderCompilerEnvironment &OutEnvironment)
{
	// TODO
	OutEnvironment.SetDefine(TEXT("USE_INSTANCING"), USE_INSTANCING);
}

void FGrassVertexFactory::ValidateCompiledResult(
	const FVertexFactoryType *Type, 
	EShaderPlatform Platform, 
	const FShaderParameterMap &ParameterMap, 
	TArray<FString> &OutErrors)
{

}

void FGrassShaderParameters::Bind(const FShaderParameterMap& ParameterMap)
{
#if USE_INSTANCING
	// TODO
	InstanceBufferParameter.Bind(ParameterMap, TEXT("InstanceBuffer"));
	LodViewOriginParameter.Bind(ParameterMap, TEXT("LodViewOrigin"));
	// LodDistancesParameter.Bind(ParameterMap, TEXT("LodDistances"));
#endif
}

void FGrassShaderParameters::GetElementShaderBindings(
	const FSceneInterface* Scene,
	const FSceneView* View,
	const FMeshMaterialShader* Shader,
	const EVertexInputStreamType InputStreamType,
	ERHIFeatureLevel::Type FeatureLevel,
	const FVertexFactory* InVertexFactory,
	const FMeshBatchElement& BatchElement,
	FMeshDrawSingleShaderBindings& ShaderBindings,
	FVertexInputStreamArray& VertexStreams) const
{
	const FGrassVertexFactory* VertexFactory = (FGrassVertexFactory*)InVertexFactory;
	ShaderBindings.Add(Shader->GetUniformBufferParameter<FGrassParameters>(), VertexFactory->UniformBuffer);

#if USE_INSTANCING
	FGrassUserData* UserData = (FGrassUserData*)BatchElement.UserData;
	// TODO
	ShaderBindings.Add(InstanceBufferParameter, UserData->InstanceBufferSRV);
	ShaderBindings.Add(LodViewOriginParameter, UserData->LodViewOrigin);
	// ShaderBindings.Add(LodDistancesParameter, UserData->LodDistances);
#endif
}


#define GRASS_FLAGS	  EVertexFactoryFlags::UsedWithMaterials \
					| EVertexFactoryFlags::SupportsDynamicLighting \
					| EVertexFactoryFlags::SupportsPrimitiveIdStream \
					| EVertexFactoryFlags::SupportsNaniteRendering

IMPLEMENT_VERTEX_FACTORY_TYPE(FGrassVertexFactory, "/SHaders/GrassVertexFactory.ush", GRASS_FLAGS);

IMPLEMENT_TYPE_LAYOUT(FGrassShaderParameters);

IMPLEMENT_VERTEX_FACTORY_PARAMETER_TYPE(FGrassVertexFactory, SF_Vertex, FGrassShaderParameters);
IMPLEMENT_VERTEX_FACTORY_PARAMETER_TYPE(FGrassVertexFactory, SF_Pixel, FGrassShaderParameters);