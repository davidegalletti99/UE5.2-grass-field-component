// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GrassData.h"

#include "GlobalShader.h"
#include "ShaderParameterUtils.h"
#include "Shader.h"
#include "ShaderParameterStruct.h"
#include "ShaderCompilerCore.h"


#include "ComputeShaders.h"

/**
 * Compute Shader that compute the vote phase for the GPU frustum culling algo.
 */
class COMPUTESHADERS_API FVoteShader : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FVoteShader);
	SHADER_USE_PARAMETER_STRUCT(FVoteShader, FGlobalShader);

public:
	using FPermutationDomain = TShaderPermutationDomain<>;
	
	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, COMPUTESHADERS_API)
		SHADER_PARAMETER(FMatrix44f, MATRIX_VP)
		SHADER_PARAMETER(FVector4f, CameraPosition)
		SHADER_PARAMETER(float, Distance) // cutoff distance
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FGrassData>, GrassDataBuffer)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint32>, VoteBuffer)
	END_SHADER_PARAMETER_STRUCT()

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

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, COMPUTESHADERS_API)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint32>, VoteBuffer)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint32>, ScanBuffer)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint32>, GroupSumArray)
	END_SHADER_PARAMETER_STRUCT()

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

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, COMPUTESHADERS_API)
		SHADER_PARAMETER(int, NumOfGroups)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint32>, GroupSumArrayIn)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint32>, GroupSumArrayOut)
	END_SHADER_PARAMETER_STRUCT()

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
	
	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, COMPUTESHADERS_API)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint32>, ArgsBuffer)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FGrassData>, GrassDataBuffer)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint32>, VoteBuffer)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint32>, ScanBuffer)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint32>, GroupSumArray)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<FGrassData>, CulledGrassOutputBuffer)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(
		const FGlobalShaderPermutationParameters& Parameters);

	static void ModifyCompilationEnvironment(
		const FGlobalShaderPermutationParameters& Parameters,
		FShaderCompilerEnvironment& OutEnvironment);
};


/**
 * Compute Shader that resets the args buffer
 */
class COMPUTESHADERS_API FResetArgsShader : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FResetArgsShader);
	SHADER_USE_PARAMETER_STRUCT(FResetArgsShader, FGlobalShader);

public:
	using FPermutationDomain = TShaderPermutationDomain<>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, COMPUTESHADERS_API)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint32>, ArgsBuffer)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(
		const FGlobalShaderPermutationParameters& Parameters);

	static void ModifyCompilationEnvironment(
		const FGlobalShaderPermutationParameters& Parameters,
		FShaderCompilerEnvironment& OutEnvironment);
};