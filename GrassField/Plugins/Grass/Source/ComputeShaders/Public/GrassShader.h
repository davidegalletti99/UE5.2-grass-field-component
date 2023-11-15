#pragma once

#include "GenericPlatform/GenericPlatformMisc.h"

#include "CoreMinimal.h"
#include "EngineDefines.h"
#include "UniformBuffer.h"
#include "GrassData.h"


#include "GlobalShader.h"
#include "ShaderParameterUtils.h"
#include "Shader.h"
#include "ShaderParameterStruct.h"
#include "ShaderCompilerCore.h"

#include "RendererInterface.h"
#include "RenderResource.h"
#include "RenderTargetPool.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphResources.h"
#include "RenderGraphUtils.h"
#include "Renderer/Private/ScenePrivate.h"

#include "Runtime/Engine/Classes/Engine/TextureRenderTarget2D.h"
#include "Kismet/BlueprintAsyncActionBase.h"

#include "ComputeShaders/Public/ComputeShaders.h"
#include "ProceduralMeshComponent.h"


// --------------------------------------------------------------
// ----------------- Compute Shader Declaration -----------------
// --------------------------------------------------------------

class COMPUTESHADERS_API FGrassShader : public FGlobalShader
{
public:

	DECLARE_GLOBAL_SHADER(FGrassShader);
	SHADER_USE_PARAMETER_STRUCT(FGrassShader, FGlobalShader);


	using FPermutationDomain = TShaderPermutationDomain<>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, COMPUTESHADERS_API)
		SHADER_PARAMETER(float, Lambda)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<int32>, ArgsBuffer)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<FGrassData>, GrassDataBuffer)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<FVector3f>, GrassPoints)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<int32>, GrassTriangles)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters);

	static void ModifyCompilationEnvironment(
		const FGlobalShaderPermutationParameters& Parameters,
		FShaderCompilerEnvironment& OutEnvironment);
private:
};