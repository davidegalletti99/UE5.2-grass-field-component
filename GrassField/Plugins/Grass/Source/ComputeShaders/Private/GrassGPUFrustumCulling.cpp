#include "GrassGPUFrustumCulling.h"

// --------------------------------------------------------------
// ---------------- Compute Shaders Declarations ----------------
// --------------------------------------------------------------

IMPLEMENT_GLOBAL_SHADER(FVoteShader, "/Shaders/GrassGPUFrustumCulling.usf", "Vote", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FScanShader, "/Shaders/GrassGPUFrustumCulling.usf", "Scan", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FScanGroupSumsShader, "/Shaders/GrassGPUFrustumCulling.usf", "ScanGroupSums", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FCompactShader, "/Shaders/GrassGPUFrustumCulling.usf", "Compact", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FResetArgsShader, "/Shaders/GrassGPUFrustumCulling.usf", "ResetArgs", SF_Compute);

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
	OutEnvironment.CompilerFlags.Add(CFLAG_AllowTypedUAVLoads);
}

// Reset Args
bool FResetArgsShader::ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
{
	const FPermutationDomain PermutationVector(Parameters.PermutationId);
	return true;
}

void FResetArgsShader::ModifyCompilationEnvironment(
	const FGlobalShaderPermutationParameters& Parameters,
	FShaderCompilerEnvironment& OutEnvironment)
{
	FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	OutEnvironment.CompilerFlags.Add(CFLAG_AllowTypedUAVLoads);
}