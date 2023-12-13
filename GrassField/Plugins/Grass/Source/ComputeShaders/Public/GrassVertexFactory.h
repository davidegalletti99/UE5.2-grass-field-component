// Copyright Epic Games, Inc. All Rights Reserved.
// Adapted from the VirtualHeightfieldMesh plugin

#pragma once

#include "CoreMinimal.h"
#include "Containers/DynamicRHIResourceArray.h"
#include "RenderResource.h"
#include "RHI.h"
#include "SceneManagement.h"
#include "UniformBuffer.h"
#include "VertexFactory.h"
#include "GrassData.h"

#if USE_INSTANCING
/**
 * Per frame UserData to pass to the vertex shader.
 */
struct FGrassUserData : public FOneFrameResource
{
	FRHIShaderResourceView *InstanceBufferSRV;
	FVector3f LodViewOrigin;
};
#endif

class FGrassVertexBuffer : public FVertexBuffer
{
public:
	FGrassVertexBuffer()
	{
		
	}

	void InitRHI() override;

	uint32 GrassDataNum;
	uint32 MaxLodSteps;

	FUnorderedAccessViewRHIRef VertexBufferUAV;
};

/*
 * Index buffer to provide incides for the mesh we're rending.
 */
class FGrassIndexBuffer : public FIndexBuffer
{
public:
	FGrassIndexBuffer()
	{

	}

	virtual void InitRHI() override;

	uint32 GrassDataNum;
	uint32 MaxLodSteps;

	FUnorderedAccessViewRHIRef IndexBufferUAV;
};

struct FGrassVertexDataType
{
	FVertexStreamComponent PositionComponent;
	FVertexStreamComponent UVComponent;
	FVertexStreamComponent TangentBasisComponents[2];
	
	void operator=(const FGrassVertexDataType& InData)
	{
		PositionComponent = InData.PositionComponent;
		UVComponent = InData.UVComponent;
		TangentBasisComponents[0] = InData.TangentBasisComponents[0];
		TangentBasisComponents[1] = InData.TangentBasisComponents[1];

	}

};

/**
 * Uniform buffer to hold parameters specific to this vertex factory. Only set up once.
 */
BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT(FGrassParameters, )
// SHADER_PARAMETER_TEXTURE(Texture2D<uint4>, PageTableTexture)
END_GLOBAL_SHADER_PARAMETER_STRUCT()

typedef TUniformBufferRef<FGrassParameters> FGrassBufferRef;

class FGrassVertexFactory : public FVertexFactory
{
	DECLARE_VERTEX_FACTORY_TYPE(FGrassVertexFactory);

public:
	explicit FGrassVertexFactory(const ERHIFeatureLevel::Type InFeatureLevel);

	virtual ~FGrassVertexFactory() override;


	void Init(FVertexBuffer* VertexBuffer);

	virtual void InitRHI() override;
	virtual void ReleaseRHI() override;
	
	static bool ShouldCompilePermutation(const FVertexFactoryShaderPermutationParameters &Parameters);

	static void ModifyCompilationEnvironment(
		const FVertexFactoryShaderPermutationParameters &Parameters, 
		FShaderCompilerEnvironment &OutEnvironment);

	static void ValidateCompiledResult(
		const FVertexFactoryType *Type, 
		EShaderPlatform Platform, 
		const FShaderParameterMap &ParameterMap, 
		TArray<FString> &OutErrors);

	void SetData(FGrassVertexDataType& InData);
	
	FGrassVertexDataType& GetData()
	{
		return Data;
	}


private:
	FGrassVertexDataType Data;

	FGrassParameters Params;
	FGrassBufferRef UniformBuffer;

	// Shader parameters is the data passed to our vertex shader
	friend class FGrassShaderParameters;
};


/**
 * Shader parameters for vertex factory.
 */
class FGrassShaderParameters : public FVertexFactoryShaderParameters
{
	DECLARE_TYPE_LAYOUT(FGrassShaderParameters, NonVirtual);

public:
	void Bind(const FShaderParameterMap& ParameterMap);

	void GetElementShaderBindings(
		const class FSceneInterface* Scene,
		const class FSceneView* View,
		const class FMeshMaterialShader* Shader,
		const EVertexInputStreamType InputStreamType,
		ERHIFeatureLevel::Type FeatureLevel,
		const class FVertexFactory* InVertexFactory,
		const struct FMeshBatchElement& BatchElement,
		class FMeshDrawSingleShaderBindings& ShaderBindings,
		FVertexInputStreamArray& VertexStreams) const;

protected:
#if USE_INSTANCING
	// TODO
	LAYOUT_FIELD(FShaderResourceParameter, InstanceBufferParameter);
	LAYOUT_FIELD(FShaderParameter, LodViewOriginParameter);
	// LAYOUT_FIELD(FShaderParameter, LodDistancesParameter);
#endif
};