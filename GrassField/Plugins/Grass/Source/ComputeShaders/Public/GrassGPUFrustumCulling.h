// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "GlobalShader.h"
#include "ShaderParameterUtils.h"
#include "Shader.h"
#include "ShaderParameterStruct.h"
#include "ShaderCompilerCore.h"


#include "ComputeShaders.h"

#define NUM_THREADS_VoteShader_X 128
#define NUM_THREADS_VoteShader_Y 1
#define NUM_THREADS_VoteShader_Z 1

#define NUM_THREADS_ScanShader_X 64
#define NUM_THREADS_ScanShader_Y 1
#define NUM_THREADS_ScanShader_Z 1

#define NUM_THREADS_ScanGroupSumsShader_X 1024
#define NUM_THREADS_ScanGroupSumsShader_Y 1
#define NUM_THREADS_ScanGroupSumsShader_Z 1

#define NUM_THREADS_CompactShader_X 128
#define NUM_THREADS_CompactShader_Y 1
#define NUM_THREADS_CompactShader_Z 1

struct COMPUTESHADERS_API FGrassData : FShaderParametersMetadata
{
	FVector4f position;
	FVector2f uv;
	float displacement;
};

struct COMPUTESHADERS_API FGPUFrustumCullingParams
{
	FMatrix44f VP;
	FVector4f CameraPosition;
	float Distance; //Cutoff distance
	TArray<FGrassData> GrassDataBuffer;

};

#define GRASS_DATA_STRUCT() \
	BEGIN_UNIFORM_BUFFER_STRUCT(FGrassData, ) \
		SHADER_PARAMETER(FVector4f, position) \
		SHADER_PARAMETER(FVector2f, uv) \
		SHADER_PARAMETER(float, displacement) \
	END_UNIFORM_BUFFER_STRUCT()

/**
 * Compute Shader that compute the vote phase for the GPU frustum culling algo.
 */
class COMPUTESHADERS_API FVoteShader : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FVoteShader);
	SHADER_USE_PARAMETER_STRUCT(FVoteShader, FGlobalShader);

public:
	using FPermutationDomain = TShaderPermutationDomain<>;
	GRASS_DATA_STRUCT()
	BEGIN_SHADER_PARAMETER_STRUCT(FGPUFrustumCullingParams, COMPUTESHADERS_API)
		SHADER_PARAMETER(FMatrix44f, MATRIX_VP)
		SHADER_PARAMETER(FVector4f, CameraPosition)
		SHADER_PARAMETER(float, Distance) // cutoff distance
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FGrassData>, GrassDataBuffer)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint32>, VoteBuffer)
	END_SHADER_PARAMETER_STRUCT()

	using FParameters = FGPUFrustumCullingParams;

	static bool ShouldCompilePermutation(
		const FGlobalShaderPermutationParameters& Parameters);

	static void ModifyCompilationEnvironment(
		const FGlobalShaderPermutationParameters& Parameters,
		FShaderCompilerEnvironment& OutEnvironment);
};

/**
 * Compute Shader that compute the scan phase for the GPU frustum culling algo.
 */
class COMPUTESHADERS_API FScanShader : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FScanShader);
	SHADER_USE_PARAMETER_STRUCT(FScanShader, FGlobalShader);

public:
	using FPermutationDomain = TShaderPermutationDomain<>;

	BEGIN_SHADER_PARAMETER_STRUCT(FGPUFrustumCullingParams, COMPUTESHADERS_API)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint32>, VoteBuffer)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint32>, ScanBuffer)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint32>, GroupSumArray)
	END_SHADER_PARAMETER_STRUCT()

	using FParameters = FGPUFrustumCullingParams;

	static bool ShouldCompilePermutation(
		const FGlobalShaderPermutationParameters& Parameters);

	static void ModifyCompilationEnvironment(
		const FGlobalShaderPermutationParameters& Parameters,
		FShaderCompilerEnvironment& OutEnvironment);
};


/**
 * Compute Shader that compute the scan group sums phase for the GPU frustum culling algo.
 */
class COMPUTESHADERS_API FScanGroupSumsShader : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FScanGroupSumsShader);
	SHADER_USE_PARAMETER_STRUCT(FScanGroupSumsShader, FGlobalShader);

public:
	using FPermutationDomain = TShaderPermutationDomain<>;

	BEGIN_SHADER_PARAMETER_STRUCT(FGPUFrustumCullingParams, COMPUTESHADERS_API)
		SHADER_PARAMETER(int, NumOfGroups)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint32>, GroupSumArrayIn)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint32>, GroupSumArrayOut)
	END_SHADER_PARAMETER_STRUCT()

	using FParameters = FGPUFrustumCullingParams;

	static bool ShouldCompilePermutation(
		const FGlobalShaderPermutationParameters& Parameters);

	static void ModifyCompilationEnvironment(
		const FGlobalShaderPermutationParameters& Parameters,
		FShaderCompilerEnvironment& OutEnvironment);
};


/**
 * Compute Shader that compute the compact phase for the GPU frustum culling algo.
 */
class COMPUTESHADERS_API FCompactShader : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FCompactShader);
	SHADER_USE_PARAMETER_STRUCT(FCompactShader, FGlobalShader);

public:
	using FPermutationDomain = TShaderPermutationDomain<>;
	GRASS_DATA_STRUCT()
	BEGIN_SHADER_PARAMETER_STRUCT(FGPUFrustumCullingParams, COMPUTESHADERS_API)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint32>, ArgsBuffer)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FGrassData>, GrassDataBuffer)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint32>, VoteBuffer)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint32>, ScanBuffer)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint32>, GroupSumArray)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<FGrassData>, CulledGrassOutputBuffer)
		SHADER_PARAMETER_STRUCT_REF(FGrassData, GrassData)
	END_SHADER_PARAMETER_STRUCT()

	using FParameters = FGPUFrustumCullingParams;

	static bool ShouldCompilePermutation(
		const FGlobalShaderPermutationParameters& Parameters);

	static void ModifyCompilationEnvironment(
		const FGlobalShaderPermutationParameters& Parameters,
		FShaderCompilerEnvironment& OutEnvironment);
};