// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GrassData.h"

/**
 * Per frame UserData to pass to the vertex shader.
 */
struct FGrassInstancingUserData : FOneFrameResource
{
	FRHIShaderResourceView *InstanceBufferSRV;
	FVector3f LodViewOrigin;
};

/**
 * 
 */
class FGrassInstancingVertexBuffer : public FVertexBuffer
{
public:
	FGrassInstancingVertexBuffer()
	{
		
	}

	virtual void InitRHI() override;
	virtual void ReleaseRHI() override;
	
	TResourceArray<GrassMesh::FGrassVertex> Vertices;
};

/*
 * Index buffer to provide incides for the mesh we're rending.
 */
class FGrassInstancingIndexBuffer : public FIndexBuffer
{
public:
	FGrassInstancingIndexBuffer()
	{

	}

	virtual void InitRHI() override;
	virtual void ReleaseRHI() override;
	
	TResourceArray<uint32> Indices;
};

struct FGrassInstancingVertexDataType
{
	FVertexStreamComponent PositionComponent;
	FVertexStreamComponent UVComponent;
	FVertexStreamComponent TangentBasisComponents[2];
	
	void operator=(const FGrassInstancingVertexDataType& InData)
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
BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT(FGrassInstancingParameters, )
// SHADER_PARAMETER_TEXTURE(Texture2D<uint4>, PageTableTexture)
END_GLOBAL_SHADER_PARAMETER_STRUCT()

typedef TUniformBufferRef<FGrassInstancingParameters> FGrassInstancingBufferRef;

class FGrassInstancingVertexFactory : public FVertexFactory
{
	DECLARE_VERTEX_FACTORY_TYPE(FGrassInstancingVertexFactory);
	
public:
	explicit FGrassInstancingVertexFactory(const ERHIFeatureLevel::Type InFeatureLevel);

	virtual ~FGrassInstancingVertexFactory() override;


	void SetData(const FVertexBuffer* VertexBuffer);

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

	void SetData(FGrassInstancingVertexDataType& InData);
	
	FGrassInstancingVertexDataType& GetData()
	{
		return Data;
	}


private:
	FGrassInstancingVertexDataType Data;

	FGrassInstancingParameters Params;
public:
	FGrassInstancingBufferRef UniformBuffer;
	
	// Shader parameters is the data passed to our vertex shader
	friend class FGrassShaderParameters;
};


/**
 * Shader parameters for vertex factory.
 */
class FGrassInstancingShaderParameters : public FVertexFactoryShaderParameters
{
	DECLARE_TYPE_LAYOUT(FGrassInstancingShaderParameters, NonVirtual);

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
	// TODO
	LAYOUT_FIELD(FShaderResourceParameter, InstanceBufferParameter);
	LAYOUT_FIELD(FShaderParameter, LodViewOriginParameter);
	// LAYOUT_FIELD(FShaderParameter, LodDistancesParameter);
};