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
#include "FirstCShader.generated.h"

#define NUM_THREADS_FirstCShader_X 10
#define NUM_THREADS_FirstCShader_Y 1
#define NUM_THREADS_FirstCShader_Z 1

// --------------------------------------------------------------
// ------------ Compute Shader Struct & Definitions -------------
// --------------------------------------------------------------

struct COMPUTESHADERS_API FFirstCShaderDispatchParams
{
	int X;
	int Y;
	int Z;

	// offset + scale
	int OffScale[2];

	// input data
	float* Input;

	// Output data
	float Output[10];



	FFirstCShaderDispatchParams(int x, int y, int z)
		: X(x), Y(y), Z(z)
	{}
};


// --------------------------------------------------------------
// ----------------- Compute Shader Declaration -----------------
// --------------------------------------------------------------

class COMPUTESHADERS_API FFirstCShader : public FGlobalShader
{
public:

	DECLARE_GLOBAL_SHADER(FFirstCShader);
	SHADER_USE_PARAMETER_STRUCT(FFirstCShader, FGlobalShader);


	class FFirstCShader_Perm_TEST : SHADER_PERMUTATION_INT("TEST", 1);
	using FPermutationDomain = TShaderPermutationDomain<FFirstCShader_Perm_TEST>;

	BEGIN_SHADER_PARAMETER_STRUCT(FFirstCShaderDispatchParams, COMPUTESHADERS_API)
		/*
		* Here's where you define one or more of the input parameters for your shader.
		* Some examples:
		*/
		// SHADER_PARAMETER(uint32, MyUint32) // On the shader side: uint32 MyUint32;
		// SHADER_PARAMETER(FVector3f, MyVector) // On the shader side: float3 MyVector;

		// SHADER_PARAMETER_TEXTURE(Texture2D, MyTexture) // On the shader side: Texture2D<float4> MyTexture; (float4 should be whatever you expect each pixel in the texture to be, in this case float4(R,G,B,A) for 4 channels)
		// SHADER_PARAMETER_SAMPLER(SamplerState, MyTextureSampler) // On the shader side: SamplerState MySampler; // CPP side: TStaticSamplerState<ESamplerFilter::SF_Bilinear>::GetRHI();

		// SHADER_PARAMETER_ARRAY(float, MyFloatArray, [3]) // On the shader side: float MyFloatArray[3];

		// SHADER_PARAMETER_UAV(RWTexture2D<FVector4f>, MyTextureUAV) // On the shader side: RWTexture2D<float4> MyTextureUAV;
		// SHADER_PARAMETER_UAV(RWStructuredBuffer<FMyCustomStruct>, MyCustomStructs) // On the shader side: RWStructuredBuffer<FMyCustomStruct> MyCustomStructs;
		// SHADER_PARAMETER_UAV(RWBuffer<FMyCustomStruct>, MyCustomStructs) // On the shader side: RWBuffer<FMyCustomStruct> MyCustomStructs;

		// SHADER_PARAMETER_SRV(StructuredBuffer<FMyCustomStruct>, MyCustomStructs) // On the shader side: StructuredBuffer<FMyCustomStruct> MyCustomStructs;
		// SHADER_PARAMETER_SRV(Buffer<FMyCustomStruct>, MyCustomStructs) // On the shader side: Buffer<FMyCustomStruct> MyCustomStructs;
		// SHADER_PARAMETER_SRV(Texture2D<FVector4f>, MyReadOnlyTexture) // On the shader side: Texture2D<float4> MyReadOnlyTexture;

		// SHADER_PARAMETER_STRUCT_REF(FMyCustomStruct, MyCustomStruct)

		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<int>, OffScale)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<float>, Input)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<float>, Output)

	END_SHADER_PARAMETER_STRUCT()

	using FParameters = FFirstCShaderDispatchParams;

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
class COMPUTESHADERS_API FFirstCShaderInterface {
public:
	// Executes this shader on the render thread
	static void DispatchRenderThread(
		FRHICommandListImmediate& RHICmdList,
		FFirstCShaderDispatchParams& Params,
		TFunction<void(float OutputVal[10])> AsyncCallback);

	// Executes this shader on the render thread from the game thread via EnqueueRenderThreadCommand
	static void DispatchGameThread(
		FFirstCShaderDispatchParams& Params,
		TFunction<void(float OutputVal[10])> AsyncCallback);

	// Dispatches this shader. Can be called from any thread
	static void Dispatch(
		FFirstCShaderDispatchParams& Params,
		TFunction<void(float OutputVal[10])> AsyncCallback);
};



// --------------------------------------------------------------
// ------------------- Compute Shader Library -------------------
// --------------------------------------------------------------

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnFirstCShaderLibrary_AsyncExecutionCompleted, const TArray<float>, Value);

class COMPUTESHADERS_API FirstCShaderExecutor 
{
public:
	FirstCShaderExecutor();
	void Execute(int Offset, int Scale, float* Data);

private:

};


UCLASS()
class COMPUTESHADERS_API UFirstCShaderLibrary_AsyncExecution : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

public:

	int Offset;
	int Scale;
	float Data[10];

	// Execute the actual load
	virtual void Activate();


	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", Category = "ComputeShader", WorldContext = "WorldContextObject"))
		static UFirstCShaderLibrary_AsyncExecution* ExecuteBaseComputeShader(
			UObject* WorldContextObject,
			int Offset, int Scale, TArray<float> Data);

	UPROPERTY(BlueprintAssignable)
		FOnFirstCShaderLibrary_AsyncExecutionCompleted Completed;




};