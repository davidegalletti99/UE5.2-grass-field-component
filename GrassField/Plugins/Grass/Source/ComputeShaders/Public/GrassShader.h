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

#include "ComputeShaders/Public/ComputeShaders.h"
#include "ProceduralMeshComponent.h"

// deve valere che: X * Y * Z <= 1024
#define NUM_THREADS_GrassShader_X 1024
#define NUM_THREADS_GrassShader_Y 1
#define NUM_THREADS_GrassShader_Z 1

// --------------------------------------------------------------
// ------------ Compute Shader Struct & Definitions -------------
// --------------------------------------------------------------

struct COMPUTESHADERS_API FGrassShaderDispatchParams
{
	int X;
	int Y;
	int Z;

	float GlobalWorldTime;
	float MinHeight;
	float MaxHeight;
	TArray<FVector4f> Points;
	FVector Position;
	FVector CameraDirection;
	float Lambda;


	FGrassShaderDispatchParams(
		float globalWorlTime, 
		FVector position, 
		TArray<FVector> points, 
		float minHeight, 
		float maxHeight, 
		FVector cameraDirection, 
		float lambda)
		:	X((int)points.Num() / NUM_THREADS_GrassShader_X + (int)(points.Num() % NUM_THREADS_GrassShader_X > 0)), 
			Y(1), 
			Z(1),
		GlobalWorldTime(globalWorlTime),
		MinHeight(minHeight),
		MaxHeight(maxHeight),
		Lambda(lambda),
		Position(position),
		CameraDirection(cameraDirection)

	{
		for (auto point : points)
		{
			Points.Add(FVector4f(FVector3f(point), 0.0));
		}
	}
};


// --------------------------------------------------------------
// ----------------- Compute Shader Declaration -----------------
// --------------------------------------------------------------

class COMPUTESHADERS_API FGrassShader : public FGlobalShader
{
public:

	DECLARE_GLOBAL_SHADER(FGrassShader);
	SHADER_USE_PARAMETER_STRUCT(FGrassShader, FGlobalShader);


	class FGrassShader_Perm_TEST : SHADER_PERMUTATION_INT("TEST", 1);
	using FPermutationDomain = TShaderPermutationDomain<FGrassShader_Perm_TEST>;

	BEGIN_SHADER_PARAMETER_STRUCT(FGrassShaderDispatchParams, COMPUTESHADERS_API)
		SHADER_PARAMETER(FVector3f, Position)
		SHADER_PARAMETER(FVector3f, CameraDirection)
		SHADER_PARAMETER(int, Size)
		SHADER_PARAMETER(float, GlobalWorldTime)
		SHADER_PARAMETER(float, Lambda)
		SHADER_PARAMETER(float, MinHeight)
		SHADER_PARAMETER(float, MaxHeight)
		SHADER_PARAMETER_RDG_BUFFER_SRV(TArray<FVector4f>, Points)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<float>, GrassPoints)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<int>, GrassTriangles)
	END_SHADER_PARAMETER_STRUCT()

	using FParameters = FGrassShaderDispatchParams;

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
class COMPUTESHADERS_API FGrassShaderInterface {
public:
	// Executes this shader on the render thread
	static void DispatchRenderThread(
		FRHICommandListImmediate& RHICmdList,
		FGrassShaderDispatchParams& Params,
		TFunction<void(TArray<FVector>& GrassPoints, TArray<int32>& GrassTriangles)> AsyncCallback);

	// Executes this shader on the render thread from the game thread via EnqueueRenderThreadCommand
	static void DispatchGameThread(
		FGrassShaderDispatchParams& Params,
		TFunction<void(TArray<FVector>& GrassPoints, TArray<int32>& GrassTriangles)> AsyncCallback);

	// Dispatches this shader. Can be called from any thread
	static void Dispatch(
		FGrassShaderDispatchParams& Params,
		TFunction<void(TArray<FVector>& GrassPoints, TArray<int32>& GrassTriangles)> AsyncCallback);
};



// --------------------------------------------------------------
// ------------------- Compute Shader Library -------------------
// --------------------------------------------------------------

class COMPUTESHADERS_API GrassShaderExecutor : public FAsyncTaskBase
{
	void DoTaskWork();
	bool TryAbandonTask();

public:
	int sectionId;
	float globalWorldTime; 

	UProceduralMeshComponent* meshComponent;

	float lambda;
	float minHeight;
	float maxHeight;

	TArray<FVector> points;
	FVector cameraDirection;

	GrassShaderExecutor();
private:

};
