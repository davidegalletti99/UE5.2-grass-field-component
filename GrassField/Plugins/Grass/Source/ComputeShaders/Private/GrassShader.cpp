#include "GrassShader.h"

#include "GlobalShader.h"
#include "UnifiedBuffer.h"
#include "CanvasTypes.h"
#include "MaterialShader.h"
#include "PixelShaderUtils.h"
#include "MeshPassProcessor.inl"
#include "StaticMeshResources.h"
#include "DynamicMeshBuilder.h"
#include "RenderGraphResources.h"
#include "RenderCore/Public/RenderGraphUtils.h"
#include "Containers/UnrealString.h"


// --------------------------------------------------------------
// ----------------- Compute Shader Declaration -----------------
// --------------------------------------------------------------

DECLARE_STATS_GROUP(TEXT("GrassShader"), STATGROUP_GrassShader, STATCAT_Advanced);
DECLARE_CYCLE_STAT(TEXT("GrassShader Execute"), STAT_GrassShader_Execute, STATGROUP_GrassShader);

IMPLEMENT_GLOBAL_SHADER(FGrassShader, "/Shaders/GrassShader.usf",       "Main"       , SF_Compute);


bool FGrassShader::ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
{
	const FPermutationDomain PermutationVector(Parameters.PermutationId);

	return true;
}

void FGrassShader::ModifyCompilationEnvironment(
	const FGlobalShaderPermutationParameters& Parameters,
	FShaderCompilerEnvironment& OutEnvironment)
{
	FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	OutEnvironment.CompilerFlags.Add(CFLAG_AllowTypedUAVLoads);
}