#pragma once

#include "GenericPlatform/GenericPlatformMisc.h"

#include "CoreMinimal.h"
#include "EngineDefines.h"
#include "UniformBuffer.h"


#include "RHI.h"
#include "RHICommandList.h"
#include "RHIStaticStates.h"

#include "MeshPassProcessor.h"
#include "MeshMaterialShader.h"

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

#include "ComputeShaders.h"
#include "ProceduralMeshComponent.h"

#define NUM_THREADS_TerrainShader_X 32
#define NUM_THREADS_TerrainShader_Y 32
#define NUM_THREADS_TerrainShader_Z 1

// --------------------------------------------------------------
// ------------ Compute Shader Struct & Definitions -------------
// --------------------------------------------------------------

struct COMPUTESHADERS_API FTerrainShaderDispatchParams
{
	int X;
	int Y;
	int Z;

	float GlobalWorldTime = 0;
	int32 Width = 256;
	int32 Height = 256;
	int32 MaxAltitude = 100;
	int32 Spacing = 10;

	TArray<float> Points;
	TArray<int32> triangles;


	FTerrainShaderDispatchParams(float globalWorldTime, int32 width, int32 height, int32 maxAltitude, int32 spacing)
		: X((int)width / NUM_THREADS_TerrainShader_X), 
		  Y((int)height / NUM_THREADS_TerrainShader_Y), 
		  Z(1), GlobalWorldTime(globalWorldTime), 
		  Width(width), Height(height), MaxAltitude(maxAltitude), Spacing(spacing)
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


	class FTerrainShader_Perm_TEST : SHADER_PERMUTATION_INT("TEST", 1);
	using FPermutationDomain = TShaderPermutationDomain<FTerrainShader_Perm_TEST>;

	BEGIN_SHADER_PARAMETER_STRUCT(FTerrainShaderDispatchParams, COMPUTESHADERS_API)

		SHADER_PARAMETER(float, GlobalWorldTime)
		SHADER_PARAMETER(int32, Width)
		SHADER_PARAMETER(int32, Height)
		SHADER_PARAMETER(int32, MaxAltitude)
		SHADER_PARAMETER(int32, Spacing)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<float>, Points)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<int32>, Triangles)


		END_SHADER_PARAMETER_STRUCT()

		using FParameters = FTerrainShaderDispatchParams;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters);

	static void ModifyCompilationEnvironment(
		const FGlobalShaderPermutationParameters& Parameters,
		FShaderCompilerEnvironment& OutEnvironment);
private:
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
		TFunction<void(TArray<FVector>& Points, TArray<int32>& Triangles)> AsyncCallback);

	// Executes this shader on the render thread from the game thread via EnqueueRenderThreadCommand
	static void DispatchGameThread(
		FTerrainShaderDispatchParams& Params,
		TFunction<void(TArray<FVector>& Points, TArray<int32>& Triangles)> AsyncCallback);

	// Dispatches this shader. Can be called from any thread
	static void Dispatch(
		FTerrainShaderDispatchParams& Params,
		TFunction<void(TArray<FVector>& Points, TArray<int32>& Triangles)> AsyncCallback);
};



// --------------------------------------------------------------
// ------------------- Compute Shader Library -------------------
// --------------------------------------------------------------


class COMPUTESHADERS_API TerrainShaderExecutor
{
public:
	TerrainShaderExecutor();
	void Execute(float globalWorldTime, 
		int32 width, int32 height, 
		int32 maxAltitude, int32 spacing, 
		UProceduralMeshComponent* meshComponent);

private:

};