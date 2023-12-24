// Copyright Epic Games, Inc. All Rights Reserved.


#include "..\Public\GrassInstancingVertexFactory.h"

#include "Engine/Engine.h"
#include "EngineGlobals.h"
#include "Materials/Material.h"
#include "MeshMaterialShader.h"
#include "RHIStaticStates.h"
#include "ShaderParameters.h"
#include "DataDrivenShaderPlatformInfo.h"
#include "GrassData.h"
#include "MaterialDomain.h"
#include "MeshDrawShaderBindings.h"
#include "ShaderParameterUtils.h"

IMPLEMENT_GLOBAL_SHADER_PARAMETER_STRUCT(FGrassInstancingParameters, "GrassInstancingParams");


void FGrassInstancingVertexBuffer::InitRHI()
{
	if (Vertices.IsEmpty())
		return;
	
	FRHIResourceCreateInfo CreateInfo(TEXT("FGrass.VertexBuffer"), &Vertices);
	VertexBufferRHI = RHICreateVertexBuffer(Vertices.GetResourceDataSize(), BUF_UnorderedAccess | BUF_DrawIndirect, CreateInfo);
}

void FGrassInstancingVertexBuffer::ReleaseRHI()
{
	Vertices.Empty();
	FVertexBuffer::ReleaseRHI();
}

void FGrassInstancingIndexBuffer::InitRHI()
{
	if (Indices.IsEmpty())
		return;
	
	FRHIResourceCreateInfo CreateInfo(TEXT("FGrass.IndexBuffer"), &Indices);
	IndexBufferRHI = RHICreateVertexBuffer(Indices.GetResourceDataSize(), BUF_UnorderedAccess | BUF_DrawIndirect, CreateInfo);

}

void FGrassInstancingIndexBuffer::ReleaseRHI()
{
	Indices.Empty();
	FIndexBuffer::ReleaseRHI();
}

FGrassInstancingVertexFactory::FGrassInstancingVertexFactory(const ERHIFeatureLevel::Type InFeatureLevel)
		: FVertexFactory(InFeatureLevel)
{
	// TODO: Sistemare per poter usare i parametri
	FGrassInstancingParameters UniformParams;
	Params = UniformParams;
}

FGrassInstancingVertexFactory::~FGrassInstancingVertexFactory()
{
	
}

void FGrassInstancingVertexFactory::SetData(FGrassInstancingVertexDataType& InData)
{
	Data = InData;
	UpdateRHI();
}

void FGrassInstancingVertexFactory::SetData(const FVertexBuffer* VertexBuffer)
{
	ENQUEUE_RENDER_COMMAND(InitGrassVertexFactory)([VertexBuffer, this](FRHICommandListImmediate& RHICmdListImmediate)
		{
			// Initialize the vertex factory's stream components.
			FGrassInstancingVertexDataType NewData;
			NewData.PositionComponent = STRUCTMEMBER_VERTEXSTREAMCOMPONENT(VertexBuffer, GrassMesh::FGrassVertex, Position, VET_Float3);
			NewData.UVComponent = STRUCTMEMBER_VERTEXSTREAMCOMPONENT(VertexBuffer, GrassMesh::FGrassVertex, UV, VET_Float2);
			NewData.TangentBasisComponents[0] = STRUCTMEMBER_VERTEXSTREAMCOMPONENT(VertexBuffer, GrassMesh::FGrassVertex, TangentX, VET_Float3);
			NewData.TangentBasisComponents[1] = STRUCTMEMBER_VERTEXSTREAMCOMPONENT(VertexBuffer, GrassMesh::FGrassVertex, TangentZ, VET_Float4);
			this->SetData(NewData);
		});
}

void FGrassInstancingVertexFactory::InitRHI()
{
	check(Streams.Num() == 0);
	
	UniformBuffer = FGrassInstancingBufferRef::CreateUniformBufferImmediate(Params, UniformBuffer_MultiFrame);
	
	FVertexDeclarationElementList Elements;
	if (Data.PositionComponent.VertexBuffer != nullptr)
		Elements.Add(AccessStreamComponent(Data.PositionComponent, 0));

	if (Data.UVComponent.VertexBuffer != nullptr)
		Elements.Add(AccessStreamComponent(Data.UVComponent, 1));
	
	if (Data.TangentBasisComponents[0].VertexBuffer != nullptr)
		Elements.Add(AccessStreamComponent(Data.TangentBasisComponents[0], 2));
	
	if (Data.TangentBasisComponents[1].VertexBuffer != nullptr)
		Elements.Add(AccessStreamComponent(Data.TangentBasisComponents[1], 3));
	
	InitDeclaration(Elements, EVertexInputStreamType::Default);
	check(Streams.Num() > 0);
}

void FGrassInstancingVertexFactory::ReleaseRHI()
{
	UniformBuffer.SafeRelease();
	FVertexFactory::ReleaseRHI();
}

bool FGrassInstancingVertexFactory::ShouldCompilePermutation(const FVertexFactoryShaderPermutationParameters &Parameters)
{
	if (!IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM6))
	{
		return false;
	}
	const bool Result = Parameters.MaterialParameters.bIsSpecialEngineMaterial
			|| Parameters.MaterialParameters.MaterialDomain == MD_Surface;
	
	return Result;
}

void FGrassInstancingVertexFactory::ModifyCompilationEnvironment(const FVertexFactoryShaderPermutationParameters &Parameters, FShaderCompilerEnvironment &OutEnvironment)
{
	OutEnvironment.SetDefine(TEXT("USE_INSTANCING"), true);
}

void FGrassInstancingVertexFactory::ValidateCompiledResult(
	const FVertexFactoryType *Type, 
	EShaderPlatform Platform, 
	const FShaderParameterMap &ParameterMap, 
	TArray<FString> &OutErrors)
{
}

void FGrassInstancingShaderParameters::Bind(const FShaderParameterMap& ParameterMap)
{
	// TODO
	InstanceBufferParameter.Bind(ParameterMap, TEXT("InstanceBuffer"));
	LodViewOriginParameter.Bind(ParameterMap, TEXT("LodViewOrigin"));
	// LodDistancesParameter.Bind(ParameterMap, TEXT("LodDistances"));
}

void FGrassInstancingShaderParameters::GetElementShaderBindings(
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
	const FGrassInstancingVertexFactory* VertexFactory = (FGrassInstancingVertexFactory*)InVertexFactory;
	ShaderBindings.Add(Shader->GetUniformBufferParameter<FGrassInstancingParameters>(), VertexFactory->UniformBuffer);

	FGrassInstancingUserData* UserData = (FGrassInstancingUserData*)BatchElement.UserData;
	// TODO
	ShaderBindings.Add(InstanceBufferParameter, UserData->InstanceBufferSRV);
	ShaderBindings.Add(LodViewOriginParameter, UserData->LodViewOrigin);
	// ShaderBindings.Add(LodDistancesParameter, UserData->LodDistances);
}


#define GRASS_FLAGS	  EVertexFactoryFlags::UsedWithMaterials \
					| EVertexFactoryFlags::SupportsDynamicLighting \
					| EVertexFactoryFlags::SupportsPrimitiveIdStream

IMPLEMENT_VERTEX_FACTORY_TYPE(FGrassInstancingVertexFactory, "/Shaders/GrassVertexFactory.ush", GRASS_FLAGS);

IMPLEMENT_TYPE_LAYOUT(FGrassInstancingShaderParameters);

IMPLEMENT_VERTEX_FACTORY_PARAMETER_TYPE(FGrassInstancingVertexFactory, SF_Vertex, FGrassInstancingShaderParameters);
IMPLEMENT_VERTEX_FACTORY_PARAMETER_TYPE(FGrassInstancingVertexFactory, SF_Pixel, FGrassInstancingShaderParameters);