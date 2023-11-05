// Fill out your copyright notice in the Description page of Project Settings.


#include "GrassGPUFrustumCulling.h"

// --------------------------------------------------------------
// ---------------- Compute Shaders Declarations ----------------
// --------------------------------------------------------------

//DECLARE_STATS_GROUP(TEXT("VoteShader"), STATGROUP_VoteShader, STATCAT_Advanced);
//DECLARE_CYCLE_STAT(TEXT("VoteShader Execute"), STAT_VoteShader_Execute, STATGROUP_VoteShader);

//DECLARE_STATS_GROUP(TEXT("ScanShader"), STATGROUP_ScanShader, STATCAT_Advanced);
//DECLARE_CYCLE_STAT(TEXT("ScanShader Execute"), STAT_ScanShader_Execute, STATGROUP_ScanShader);

//DECLARE_STATS_GROUP(TEXT("ScanGroupSumsShader"), STATGROUP_ScanGroupSumsShader, STATCAT_Advanced);
//DECLARE_CYCLE_STAT(TEXT("ScanGroupSumsShader Execute"), STAT_ScanGroupSumsShader_Execute, STATGROUP_ScanGroupSumsShader);

//DECLARE_STATS_GROUP(TEXT("CompactShader"), STATGROUP_CompactShader, STATCAT_Advanced);
//DECLARE_CYCLE_STAT(TEXT("CompactShader Execute"), STAT_CompactShader_Execute, STATGROUP_CompactShader);

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

}

