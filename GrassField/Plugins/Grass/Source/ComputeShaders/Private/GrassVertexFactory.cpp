// Copyright Epic Games, Inc. All Rights Reserved.
// Adapted from the VirtualHeightfieldMesh plugin

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


void FGrassIndexBuffer::InitRHI()
{

	int32 IndicesNum = GrassDataNum * (MaxLodSteps * 2 + 1) * 3 * 2;
	FRHIResourceCreateInfo CreateInfo(TEXT("FGrass.IndexBuffer"));
	IndexBufferRHI = RHICreateVertexBuffer(IndicesNum * sizeof(int32), BUF_UnorderedAccess | BUF_DrawIndirect, CreateInfo);
	IndexBufferUAV = RHICreateUnorderedAccessView(IndexBufferRHI, PF_R32_UINT);

}

void FGrassVertexBuffer::InitRHI()
{
	int32 VerticesNum = GrassDataNum * (MaxLodSteps * 2 + 1);
	FRHIResourceCreateInfo CreateInfo(TEXT("FGrass.VertexBuffer"));
	VertexBufferRHI = RHICreateVertexBuffer(VerticesNum * sizeof(GrassMesh::FGrassVertex), BUF_UnorderedAccess | BUF_DrawIndirect, CreateInfo);
	VertexBufferUAV = RHICreateUnorderedAccessView(VertexBufferRHI, PF_R32_UINT);
}

FGrassVertexFactory::FGrassVertexFactory(ERHIFeatureLevel::Type InFeatureLevel, const FGrassParameters &InParams)
		: FVertexFactory(InFeatureLevel), Params(InParams)
{
}

FGrassVertexFactory::~FGrassVertexFactory()
{
}


void FGrassVertexFactory::SetData(FGrassVertexDataType& InData)
{
	Data = InData;
	UpdateRHI();
}

void FGrassVertexFactory::Init(FVertexBuffer* VertexBuffer)
{

	ENQUEUE_RENDER_COMMAND(InitGrassVertexFactory)([VertexBuffer, this](FRHICommandListImmediate& RHICmdListImmediate)
		{
			// Initialize the vertex factory's stream components.
			FGrassVertexDataType NewData;
			NewData.PositionComponent = STRUCTMEMBER_VERTEXSTREAMCOMPONENT(VertexBuffer, GrassMesh::FGrassVertex, Position, VET_Float3);
			NewData.UVComponent = STRUCTMEMBER_VERTEXSTREAMCOMPONENT(VertexBuffer, GrassMesh::FGrassVertex, UV, VET_Float2);
			NewData.NormalComponent = STRUCTMEMBER_VERTEXSTREAMCOMPONENT(VertexBuffer, GrassMesh::FGrassVertex, Normal, VET_Float3);
			this->SetData(NewData);
		});
}

void FGrassVertexFactory::InitRHI()
{
	UE_LOG(LogTemp, Warning, TEXT("VertexFactory Init"));
	UniformBuffer = FGrassBufferRef::CreateUniformBufferImmediate(Params, UniformBuffer_MultiFrame);

	check(Streams.Num() == 0);
	FVertexDeclarationElementList Elements;
	if (Data.PositionComponent.VertexBuffer != nullptr)
		Elements.Add(AccessStreamComponent(Data.PositionComponent, 0));

	if (Data.UVComponent.VertexBuffer != nullptr)
		Elements.Add(AccessStreamComponent(Data.UVComponent, 1));

	if (Data.NormalComponent.VertexBuffer != nullptr)
		Elements.Add(AccessStreamComponent(Data.NormalComponent, 2));
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
	bool result =	Parameters.MaterialParameters.bIsSpecialEngineMaterial
			|| Parameters.MaterialParameters.MaterialDomain == MD_Surface;

	// TODO
	return result;
}

void FGrassVertexFactory::ModifyCompilationEnvironment(const FVertexFactoryShaderPermutationParameters &Parameters, FShaderCompilerEnvironment &OutEnvironment)
{
	// TODO
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
	// TODO
	InstanceBufferParameter.Bind(ParameterMap, TEXT("InstanceBuffer"));
	LodViewOriginParameter.Bind(ParameterMap, TEXT("LodViewOrigin"));
	// LodDistancesParameter.Bind(ParameterMap, TEXT("LodDistances"));
}

void FGrassShaderParameters::GetElementShaderBindings(
	const FSceneInterface* Scene,
	const FSceneView* View,
	const FMeshMaterialShader* Shader,
	const EVertexInputStreamType InputStreamType,
	ERHIFeatureLevel::Type FeatureLevel,
	const FVertexFactory* InVertexFactory,
	const FMeshBatchElement& BatchElement,
	class FMeshDrawSingleShaderBindings& ShaderBindings,
	FVertexInputStreamArray& VertexStreams) const
{
	FGrassVertexFactory* VertexFactory = (FGrassVertexFactory*)InVertexFactory;
	ShaderBindings.Add(Shader->GetUniformBufferParameter<FGrassParameters>(), VertexFactory->UniformBuffer);

	FGrassUserData* UserData = (FGrassUserData*)BatchElement.UserData;
	// TODO
	ShaderBindings.Add(InstanceBufferParameter, UserData->InstanceBufferSRV);
	ShaderBindings.Add(LodViewOriginParameter, UserData->LodViewOrigin);
	// ShaderBindings.Add(LodDistancesParameter, UserData->LodDistances);
}


#define GRASS_FLAGS	  EVertexFactoryFlags::UsedWithMaterials \
					| EVertexFactoryFlags::SupportsDynamicLighting \
					| EVertexFactoryFlags::SupportsPrimitiveIdStream

IMPLEMENT_VERTEX_FACTORY_TYPE(FGrassVertexFactory, "/SHaders/GrassVertexFactory.ush", GRASS_FLAGS);

IMPLEMENT_VERTEX_FACTORY_PARAMETER_TYPE(FGrassVertexFactory, SF_Vertex, FGrassShaderParameters);
IMPLEMENT_TYPE_LAYOUT(FGrassShaderParameters);

IMPLEMENT_VERTEX_FACTORY_PARAMETER_TYPE(FGrassVertexFactory, SF_Pixel, FGrassShaderParameters);