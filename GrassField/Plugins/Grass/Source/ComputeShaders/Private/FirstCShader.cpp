#include "FirstCShader.h"

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

DECLARE_STATS_GROUP(TEXT("FirstCShader"), STATGROUP_FirstCShader, STATCAT_Advanced);
DECLARE_CYCLE_STAT(TEXT("FirstCShader Execute"), STAT_FirstCShader_Execute, STATGROUP_FirstCShader);

// This will tell the engine to create the shader and where the shader entry point is.
//																	 (Entry Point)
//																		   |
//																		   v
//                        ShaderType             ShaderPath      Shader function name   Type
IMPLEMENT_GLOBAL_SHADER(FFirstCShader, "/Shaders/FirstCShader.usf", "FirstCShader", SF_Compute);


bool FFirstCShader::ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
{
	const FPermutationDomain PermutationVector(Parameters.PermutationId);

	return true;
}

void FFirstCShader::ModifyCompilationEnvironment(
	const FGlobalShaderPermutationParameters& Parameters,
	FShaderCompilerEnvironment& OutEnvironment)
{
	FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);

	const FPermutationDomain PermutationVector(Parameters.PermutationId);

	/*
	* Here you define constants that can be used statically in the shader code.
	* Example:
	*/
	// OutEnvironment.SetDefine(TEXT("MY_CUSTOM_CONST"), TEXT("1"));

	// These defines are used in the thread count section of our shader
	OutEnvironment.SetDefine(TEXT("THREADS_X"), NUM_THREADS_FirstCShader_X);
	OutEnvironment.SetDefine(TEXT("THREADS_Y"), NUM_THREADS_FirstCShader_Y);
	OutEnvironment.SetDefine(TEXT("THREADS_Z"), NUM_THREADS_FirstCShader_Z);

	// This shader must support typed UAV load and we are testing if it is supported at runtime using RHIIsTypedUAVLoadSupported
	OutEnvironment.CompilerFlags.Add(CFLAG_AllowTypedUAVLoads);

}


// --------------------------------------------------------------
// ------------------ Compute Shader Interface ------------------
// --------------------------------------------------------------

void FFirstCShaderInterface::DispatchRenderThread(
	FRHICommandListImmediate& RHICmdList,
	FFirstCShaderDispatchParams& Params,
	TFunction<void(float OutputVal[10])> AsyncCallback)
{
	
	// TODO: Capire perche' dopo questo @Params si corrompe
	FRDGBuilder GraphBuilder(RHICmdList);

	// Blocco di codice necessario per qualche motivo (problemi con nullptr al GraphBuilder)
	// TODO: Capire il perché
	{
		SCOPE_CYCLE_COUNTER(STAT_FirstCShader_Execute);
		DECLARE_GPU_STAT(FirstCShader)
			RDG_EVENT_SCOPE(GraphBuilder, "FirstCShader");
		RDG_GPU_STAT_SCOPE(GraphBuilder, FirstCShader);

		typename FFirstCShader::FPermutationDomain PermutationVector;

		TShaderMapRef<FFirstCShader> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel), PermutationVector);


		bool bIsShaderValid = ComputeShader.IsValid();

		if (bIsShaderValid) {
			FFirstCShader::FParameters* PassParameters = GraphBuilder.AllocParameters<FFirstCShader::FParameters>();


			const void* RawOffScaleData = (void*)Params.OffScale;
			int NumOffScale = 2;
			int OffScaleSize = sizeof(int);
			FRDGBufferRef OffScaleBuffer = CreateUploadBuffer(GraphBuilder, TEXT("OffScaleBuffer"), OffScaleSize, NumOffScale, RawOffScaleData, OffScaleSize * NumOffScale);


			PassParameters->OffScale = GraphBuilder.CreateSRV(FRDGBufferSRVDesc(OffScaleBuffer, PF_R32_SINT));


			const void* RawData = (void*)Params.Input;
			int NumInputs = 10; 
			int InputSize = sizeof(float) ;
			FRDGBufferRef InputBuffer = CreateUploadBuffer(GraphBuilder, TEXT("InputBuffer"), InputSize, NumInputs, RawData, InputSize * NumInputs);


			PassParameters->Input = GraphBuilder.CreateSRV(FRDGBufferSRVDesc(InputBuffer, PF_R32_FLOAT));

			FRDGBufferRef OutputBuffer = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateBufferDesc(sizeof(float), 10), TEXT("OutputBuffer"));

			PassParameters->Output = GraphBuilder.CreateUAV(FRDGBufferUAVDesc(OutputBuffer, PF_R32_FLOAT));


			auto GroupCount = FComputeShaderUtils::GetGroupCount(FIntVector(Params.X, Params.Y, Params.Z), FComputeShaderUtils::kGolden2DGroupSize);
			GraphBuilder.AddPass(
				RDG_EVENT_NAME("ExecuteFirstCShader"),
				PassParameters,
				ERDGPassFlags::AsyncCompute,
				[&PassParameters, ComputeShader, GroupCount](FRHIComputeCommandList& RHICmdList)
				{
					FComputeShaderUtils::Dispatch(RHICmdList, ComputeShader, *PassParameters, GroupCount);
				});


			FRHIGPUBufferReadback* GPUBufferReadback = new FRHIGPUBufferReadback(TEXT("ExecuteFirstCShaderOutput"));
			AddEnqueueCopyPass(GraphBuilder, GPUBufferReadback, OutputBuffer, 0u);

			auto RunnerFunc = [GPUBufferReadback, AsyncCallback](auto&& RunnerFunc) -> void 
			{
				if (GPUBufferReadback->IsReady()) 
				{

					float* OutVal = (float*)GPUBufferReadback->Lock(1);

					GPUBufferReadback->Unlock();

					AsyncTask(ENamedThreads::GameThread, [AsyncCallback, OutVal]() 
					{
						AsyncCallback(OutVal);
					});

					delete GPUBufferReadback;
				}
				else 
				{
					AsyncTask(ENamedThreads::ActualRenderingThread, [RunnerFunc]() 
					{
						RunnerFunc(RunnerFunc);
					});
				}
			};

			AsyncTask(ENamedThreads::ActualRenderingThread, [RunnerFunc]() 
			{
				RunnerFunc(RunnerFunc);
			});

		}
		else 
		{
			// We silently exit here as we don't want to crash the game if the shader is not found or has an error.

		}
	}

	GraphBuilder.Execute();
}

void FFirstCShaderInterface::DispatchGameThread(
	FFirstCShaderDispatchParams& Params,
	TFunction<void(float OutputVal[10])> AsyncCallback)
{
	ENQUEUE_RENDER_COMMAND(SceneDrawCompletion)(
		[&Params, AsyncCallback](FRHICommandListImmediate& RHICmdList)
		{
			DispatchRenderThread(RHICmdList, Params, AsyncCallback);
		});
}

void FFirstCShaderInterface::Dispatch(
	FFirstCShaderDispatchParams& Params,
	TFunction<void(float OutputVal[10])> AsyncCallback)
{
	if (IsInRenderingThread()) {
		DispatchRenderThread(GetImmediateCommandList_ForRenderCommand(), Params, AsyncCallback);
	}
	else {
		DispatchGameThread(Params, AsyncCallback);
	}
}



// --------------------------------------------------------------
// ------------------- Compute Shader Library -------------------
// --------------------------------------------------------------

FirstCShaderExecutor::FirstCShaderExecutor()
{}

void FirstCShaderExecutor::Execute(int Offset, int Scale, float* Data)
{
	// Create a dispatch parameters struct and fill it the input array with our args
	FFirstCShaderDispatchParams* Params = new FFirstCShaderDispatchParams(1, 1, 1);

	Params->OffScale[0] = Offset;
	Params->OffScale[1] = Scale;
	Params->Input = Data;

	// Dispatch the compute shader and wait until it completes
	// Executes the compute shader and calls the TFunction when complete.
	// Called when the results are back from the GPU.
	FFirstCShaderInterface::Dispatch(*Params,
		[this](float OutputVal[10])
		{
			FString text;


			for (int i = 0; i < 10; i++)
			{
				auto format = TEXT("");
				if (i == 0)
					text = FString::Printf(TEXT("%f"), OutputVal[i]);
				else
					text = FString::Printf(TEXT("%s, %f"), *text, OutputVal[i]);
			}

			UE_LOG(LogTemp, Warning, TEXT("Output: { %s }"), *text);
			
		});
}

// BluePrints functions
void UFirstCShaderLibrary_AsyncExecution::Activate()
{
	// Create a dispatch parameters struct and fill it the input array with our args
	FFirstCShaderDispatchParams* Params = new FFirstCShaderDispatchParams(1, 1, 1);

	Params->OffScale[0] = 1;
	Params->OffScale[1] = 10;

	float* array = new float[10] { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 };
	Params->Input = array;

	// Dispatch the compute shader and wait until it completes
	// Executes the compute shader and calls the TFunction when complete.
	FFirstCShaderInterface::Dispatch(*Params,
		[this](float OutputVal[10])
		{
			this->Completed.Broadcast(TArray<float>(OutputVal, 10));
		});
}

UFirstCShaderLibrary_AsyncExecution* UFirstCShaderLibrary_AsyncExecution::ExecuteBaseComputeShader(
	UObject* WorldContextObject,
	int Offset, int Scale, TArray<float> Data)
{
	UFirstCShaderLibrary_AsyncExecution* Action = 
		NewObject<UFirstCShaderLibrary_AsyncExecution>();

	Action->Offset = Offset;
	Action->Scale = Scale;
	int i = 0;
	for (auto it = Data.begin(); it != Data.end(); ++it)
	{
		Action->Data[i] = *it;
		i++;
	}

	Action->RegisterWithGameInstance(WorldContextObject);

	return Action;
}