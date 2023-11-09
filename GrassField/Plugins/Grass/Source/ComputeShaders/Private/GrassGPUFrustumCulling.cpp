// Fill out your copyright notice in the Description page of Project Settings.


#include "GrassGPUFrustumCulling.h"

// --------------------------------------------------------------
// ---------------- Compute Shaders Declarations ----------------
// --------------------------------------------------------------

IMPLEMENT_GLOBAL_SHADER(FVoteShader, "/Shaders/GrassGPUFrustumCulling.usf", "Vote", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FScanShader, "/Shaders/GrassGPUFrustumCulling.usf", "Scan", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FScanGroupSumsShader, "/Shaders/GrassGPUFrustumCulling.usf", "ScanGroupSums", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FCompactShader, "/Shaders/GrassGPUFrustumCulling.usf", "Compact", SF_Compute);

// Vote
bool FVoteShader::ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
{
	const FPermutationDomain PermutationVector(Parameters.PermutationId);
	return true;
}

void FVoteShader::ModifyCompilationEnvironment(
	const FGlobalShaderPermutationParameters& Parameters,
	FShaderCompilerEnvironment& OutEnvironment)
{
	FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);

	// These defines are used in the thread count section of our shader
	OutEnvironment.SetDefine(TEXT("THREADS_X"), NUM_THREADS_VoteShader_X);
	OutEnvironment.SetDefine(TEXT("THREADS_Y"), NUM_THREADS_VoteShader_Y);
	OutEnvironment.SetDefine(TEXT("THREADS_Z"), NUM_THREADS_VoteShader_Z);
	OutEnvironment.CompilerFlags.Add(CFLAG_AllowTypedUAVLoads);
}

// Scan
bool FScanShader::ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
{
	const FPermutationDomain PermutationVector(Parameters.PermutationId);
	return true;
}

void FScanShader::ModifyCompilationEnvironment(
	const FGlobalShaderPermutationParameters& Parameters,
	FShaderCompilerEnvironment& OutEnvironment)
{
	FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);

	// These defines are used in the thread count section of our shader
	OutEnvironment.SetDefine(TEXT("THREADS_X"), NUM_THREADS_ScanShader_X);
	OutEnvironment.SetDefine(TEXT("THREADS_Y"), NUM_THREADS_ScanShader_Y);
	OutEnvironment.SetDefine(TEXT("THREADS_Z"), NUM_THREADS_ScanShader_Z);
	OutEnvironment.CompilerFlags.Add(CFLAG_AllowTypedUAVLoads);
}

// ScanGroupSums
bool FScanGroupSumsShader::ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
{
	const FPermutationDomain PermutationVector(Parameters.PermutationId);
	return true;
}

void FScanGroupSumsShader::ModifyCompilationEnvironment(
	const FGlobalShaderPermutationParameters& Parameters,
	FShaderCompilerEnvironment& OutEnvironment)
{
	FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);

	// These defines are used in the thread count section of our shader
	OutEnvironment.SetDefine(TEXT("THREADS_X"), NUM_THREADS_ScanGroupSumsShader_X);
	OutEnvironment.SetDefine(TEXT("THREADS_Y"), NUM_THREADS_ScanGroupSumsShader_Y);
	OutEnvironment.SetDefine(TEXT("THREADS_Z"), NUM_THREADS_ScanGroupSumsShader_Z);
	OutEnvironment.CompilerFlags.Add(CFLAG_AllowTypedUAVLoads);
}

// Compact
bool FCompactShader::ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
{
	const FPermutationDomain PermutationVector(Parameters.PermutationId);
	return true;
}

void FCompactShader::ModifyCompilationEnvironment(
	const FGlobalShaderPermutationParameters& Parameters,
	FShaderCompilerEnvironment& OutEnvironment)
{
	FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);

	// These defines are used in the thread count section of our shader
	OutEnvironment.SetDefine(TEXT("THREADS_X"), NUM_THREADS_CompactShader_X);
	OutEnvironment.SetDefine(TEXT("THREADS_Y"), NUM_THREADS_CompactShader_Y);
	OutEnvironment.SetDefine(TEXT("THREADS_Z"), NUM_THREADS_CompactShader_Z);
	OutEnvironment.CompilerFlags.Add(CFLAG_AllowTypedUAVLoads);
}