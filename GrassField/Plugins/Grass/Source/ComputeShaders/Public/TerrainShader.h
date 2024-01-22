#pragma once

#include "GenericPlatform/GenericPlatformMisc.h"

#include "CoreMinimal.h"
#include "EngineDefines.h"
#include "UniformBuffer.h"


#include "RHI.h"
#include "RHICommandList.h"
#include "RHIStaticStates.h"


#include "GlobalShader.h"
#include "ShaderParameterUtils.h"
#include "Shader.h"
#include "ShaderParameterStruct.h"
#include "ShaderCompilerCore.h"

#include "RendererInterface.h"
#include "RenderResource.h"
#include "RenderGraphResources.h"
#include "ProceduralMeshComponent.h"

#include "CanvasTypes.h"
#include "MaterialShader.h"
#include "StaticMeshResources.h"
#include "DynamicMeshBuilder.h"
#include "RenderGraphResources.h"
#include "RHIGPUReadback.h"
#include "RenderCore/Public/RenderGraphUtils.h"
#include "Containers/UnrealString.h"

#define NUM_THREADS_TerrainShader_X 32
#define NUM_THREADS_TerrainShader_Y 32
#define NUM_THREADS_TerrainShader_Z 1

// --------------------------------------------------------------
// ------------ Compute Shader Struct & Definitions -------------
// --------------------------------------------------------------

struct COMPUTESHADERS_API FTerrainShaderDispatchParams
{
	FUintVector Size;
	float Spacing = 10;
	FVector2D Scale = FVector2D(1, 1);


	FTerrainShaderDispatchParams(
		FUintVector Size, float Spacing, FVector2D Scale)
		: Size(Size), Spacing(Spacing), Scale(Scale)
	{}
};


// --------------------------------------------------------------
// ----------------- Compute Shader Declaration -----------------
// --------------------------------------------------------------

class COMPUTESHADERS_API FTerrainShader : public FGlobalShader
{
public:

	DECLARE_GLOBAL_SHADER(FTerrainShader);
	SHADER_USE_PARAMETER_STRUCT(FTerrainShader, FGlobalShader);
	
	using FPermutationDomain = TShaderPermutationDomain<>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, COMPUTESHADERS_API)

		SHADER_PARAMETER(float, Time)
		SHADER_PARAMETER(FUintVector, Size)
		SHADER_PARAMETER(float, Spacing)
		SHADER_PARAMETER(FVector2f, Scale)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<FVector3f>, Points)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<FVector3f>, Normals)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<FVector3f>, Tangents)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<int32>, Triangles)

	END_SHADER_PARAMETER_STRUCT();


	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		const FPermutationDomain PermutationVector(Parameters.PermutationId);

		return true;
	}

	static void ModifyCompilationEnvironment(
		const FGlobalShaderPermutationParameters& Parameters,
		FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);

		const FPermutationDomain PermutationVector(Parameters.PermutationId);

		// These defines are used in the thread count section of our shader
		OutEnvironment.SetDefine(TEXT("THREADS_X"), NUM_THREADS_TerrainShader_X);
		OutEnvironment.SetDefine(TEXT("THREADS_Y"), NUM_THREADS_TerrainShader_Y);
		OutEnvironment.SetDefine(TEXT("THREADS_Z"), NUM_THREADS_TerrainShader_Z);

		// This shader must support typed UAV load and we are testing if it is supported at runtime using RHIIsTypedUAVLoadSupported
		OutEnvironment.CompilerFlags.Add(CFLAG_AllowTypedUAVLoads);
	}
};


// --------------------------------------------------------------
// ------------------ Compute Shader Interface ------------------
// --------------------------------------------------------------

// This is a public interface that we define so outside code can invoke our compute shader.
class COMPUTESHADERS_API FTerrainShaderInterface {
public:
	// Executes this shader on the render thread
	static void DispatchRenderThread(
		FRHICommandListImmediate& RHICmdList,
		FTerrainShaderDispatchParams& Params,
		TFunction<void(TArray<FVector>& Points, TArray<FVector>& Normals, TArray<FVector>& Tangents, TArray<int32>& Triangles)> AsyncCallback);

	// Executes this shader on the render thread from the game thread via EnqueueRenderThreadCommand
	static void DispatchGameThread(
		FTerrainShaderDispatchParams& Params,
		TFunction<void(TArray<FVector>& Points, TArray<FVector>& Normals, TArray<FVector>& Tangents, TArray<int32>& Triangles)> AsyncCallback);

	// Dispatches this shader. Can be called from any thread
	static void Dispatch(
		FTerrainShaderDispatchParams& Params,
		TFunction<void(TArray<FVector>& Points, TArray<FVector>& Normals, TArray<FVector>& Tangents, TArray<int32>& Triangles)> AsyncCallback);
};



// --------------------------------------------------------------
// ------------------- Compute Shader Library -------------------
// --------------------------------------------------------------


class COMPUTESHADERS_API TerrainShaderExecutor
{
public:
	TerrainShaderExecutor();
	static void Execute(FUintVector Size, float Spacing, FVector2D Scale, UProceduralMeshComponent* MeshComponent);

private:

};