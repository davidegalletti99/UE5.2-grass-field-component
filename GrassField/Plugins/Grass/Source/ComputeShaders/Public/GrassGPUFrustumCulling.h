// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "GlobalShader.h"
#include "ShaderParameterUtils.h"
#include "Shader.h"
#include "ShaderParameterStruct.h"
#include "ShaderCompilerCore.h"


#define NUM_THREADS_VoteShader_X 1024
#define NUM_THREADS_VoteShader_Y 1
#define NUM_THREADS_VoteShader_Z 1

#define NUM_THREADS_ScanShader_X 1024
#define NUM_THREADS_ScanShader_Y 1
#define NUM_THREADS_ScanShader_Z 1

#define NUM_THREADS_ScanGroupSumsShader_X 1024
#define NUM_THREADS_ScanGroupSumsShader_Y 1
#define NUM_THREADS_ScanGroupSumsShader_Z 1

#define NUM_THREADS_CompactShader_X 1024
#define NUM_THREADS_CompactShader_Y 1
#define NUM_THREADS_CompactShader_Z 1

struct COMPUTESHADERS_API FBaseGPUFrustumCullingParams
{
	int X;
	int Y;
	int Z;
};

struct COMPUTESHADERS_API FCompactShaderDispatchParams : public FBaseGPUFrustumCullingParams
{
};

struct COMPUTESHADERS_API FVoteShaderDispatchParams : public FBaseGPUFrustumCullingParams
{
};

struct COMPUTESHADERS_API FScanShaderDispatchParams : public FBaseGPUFrustumCullingParams
{
};

struct COMPUTESHADERS_API FScanGroupSumsShaderDispatchParams : public FBaseGPUFrustumCullingParams
{
};

/**
 * Compute Shader that compute the vote phase for the GPU frustum culling algo.
 */
class COMPUTESHADERS_API FVoteShader : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FVoteShader);
	SHADER_USE_PARAMETER_STRUCT(FVoteShader, FGlobalShader);

public:
	using FPermutationDomain = TShaderPermutationDomain<>;

	BEGIN_SHADER_PARAMETER_STRUCT(FVoteShaderDispatchParams, COMPUTESHADERS_API)
		SHADER_PARAMETER(FMatrix44f, VP)
		SHADER_PARAMETER_RDG_BUFFER_SRV(TArray<FVector4f>, GrassDataIn)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<FVector4f>, GrassDataOut)
	END_SHADER_PARAMETER_STRUCT()

	using FParameters = FVoteShaderDispatchParams;

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

	BEGIN_SHADER_PARAMETER_STRUCT(FScanShaderDispatchParams, COMPUTESHADERS_API)
		SHADER_PARAMETER(FMatrix44f, VP)
		SHADER_PARAMETER_RDG_BUFFER_SRV(TArray<FVector4f>, GrassDataIn)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<FVector4f>, GrassDataOut)
		END_SHADER_PARAMETER_STRUCT()

		using FParameters = FScanShaderDispatchParams;

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

	BEGIN_SHADER_PARAMETER_STRUCT(FScanGroupSumsShaderDispatchParams, COMPUTESHADERS_API)
		SHADER_PARAMETER(FMatrix44f, VP)
		SHADER_PARAMETER_RDG_BUFFER_SRV(TArray<FVector4f>, GrassDataIn)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<FVector4f>, GrassDataOut)
	END_SHADER_PARAMETER_STRUCT()

		using FParameters = FScanGroupSumsShaderDispatchParams;

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

	BEGIN_SHADER_PARAMETER_STRUCT(FCompactShaderDispatchParams, COMPUTESHADERS_API)
		SHADER_PARAMETER(FMatrix44f, VP)
		SHADER_PARAMETER_RDG_BUFFER_SRV(TArray<FVector4f>, GrassDataIn)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<FVector4f>, GrassDataOut)
		END_SHADER_PARAMETER_STRUCT()

		using FParameters = FCompactShaderDispatchParams;

	static bool ShouldCompilePermutation(
		const FGlobalShaderPermutationParameters& Parameters);

	static void ModifyCompilationEnvironment(
		const FGlobalShaderPermutationParameters& Parameters,
		FShaderCompilerEnvironment& OutEnvironment);
};