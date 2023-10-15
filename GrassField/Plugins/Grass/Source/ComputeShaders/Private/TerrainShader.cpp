#include "TerrainShader.h"

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

DECLARE_STATS_GROUP(TEXT("TerrainShader"), STATGROUP_TerrainShader, STATCAT_Advanced);
DECLARE_CYCLE_STAT(TEXT("TerrainShader Execute"), STAT_TerrainShader_Execute, STATGROUP_TerrainShader);

// This will tell the engine to create the shader and where the shader entry point is.
//																	 (Entry Point)
//																		   |
//																		   v
//                        ShaderType             ShaderPath      Shader function name   Type
IMPLEMENT_GLOBAL_SHADER(FTerrainShader, "/Shaders/TerrainShader.usf", "Main", SF_Compute);


bool FTerrainShader::ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
{
	const FPermutationDomain PermutationVector(Parameters.PermutationId);

	return true;
}

void FTerrainShader::ModifyCompilationEnvironment(
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


// --------------------------------------------------------------
// ------------------ Compute Shader Interface ------------------
// --------------------------------------------------------------

void FTerrainShaderInterface::DispatchRenderThread(
	FRHICommandListImmediate& RHICmdList,
	FTerrainShaderDispatchParams& Params,
	TFunction<void(TArray<FVector>& Points, TArray<int32>& Triangles)> AsyncCallback)
{
	FRDGBuilder GraphBuilder(RHICmdList);

	// Blocco di codice necessario per qualche motivo (problemi con nullptr al GraphBuilder)
	// TODO: Capire il perche'
	{
		SCOPE_CYCLE_COUNTER(STAT_TerrainShader_Execute);
		DECLARE_GPU_STAT(TerrainShader)
			RDG_EVENT_SCOPE(GraphBuilder, "TerrainShader");
		RDG_GPU_STAT_SCOPE(GraphBuilder, TerrainShader);

		typename FTerrainShader::FPermutationDomain PermutationVector;

		TShaderMapRef<FTerrainShader> ComputeShader(
			GetGlobalShaderMap(GMaxRHIFeatureLevel), PermutationVector);


		bool bIsShaderValid = ComputeShader.IsValid();

		if (bIsShaderValid) {
			FTerrainShader::FParameters* PassParameters = GraphBuilder.AllocParameters<FTerrainShader::FParameters>();

			PassParameters->GlobalWorldTime = Params.GlobalWorldTime;
			PassParameters->Width = Params.Width;
			PassParameters->Height = Params.Height;
			PassParameters->MaxAltitude = Params.MaxAltitude;
			PassParameters->Spacing = Params.Spacing;

			int32 pointsNumber = Params.Width * Params.Height;
			FRDGBufferRef PointsBuffer = GraphBuilder.CreateBuffer(
				FRDGBufferDesc::CreateBufferDesc(sizeof(float), pointsNumber * 3), TEXT("PointsBuffer"));
			PassParameters->Points = GraphBuilder.CreateUAV(
				FRDGBufferUAVDesc(PointsBuffer, EPixelFormat::PF_R32_FLOAT));

			int32 trianglesNumber = 2 * (pointsNumber - Params.Width - Params.Height + 1);
			FRDGBufferRef TrianglesBuffer = GraphBuilder.CreateBuffer(
				FRDGBufferDesc::CreateBufferDesc(sizeof(int32), trianglesNumber * 3), TEXT("TrianglesBuffer"));
			PassParameters->Triangles = GraphBuilder.CreateUAV(
				FRDGBufferUAVDesc(TrianglesBuffer, EPixelFormat::PF_R32_SINT));

			FIntVector GroupCount = FIntVector(Params.X, Params.Y, Params.Z);

			GraphBuilder.AddPass(
				RDG_EVENT_NAME("ExecuteTerrainShader"),
				PassParameters,
				ERDGPassFlags::AsyncCompute,
				[&PassParameters, ComputeShader, GroupCount](FRHIComputeCommandList& RHICmdList)
				{
					FComputeShaderUtils::Dispatch(RHICmdList, ComputeShader, *PassParameters, GroupCount);
				});

			int32 pointsBytes = sizeof(float) * pointsNumber * 3;
			int32 trianglesBytes = sizeof(int32) * trianglesNumber * 3;

			FRHIGPUBufferReadback* GPUPointsBufferReadback = new FRHIGPUBufferReadback(TEXT("ExecuteTerrainShaderPoints"));
			FRHIGPUBufferReadback* GPUTrianglesBufferReadback = new FRHIGPUBufferReadback(TEXT("ExecuteTerrainShaderTriangles"));
			AddEnqueueCopyPass(GraphBuilder, GPUPointsBufferReadback, PointsBuffer, pointsBytes);
			AddEnqueueCopyPass(GraphBuilder, GPUTrianglesBufferReadback, TrianglesBuffer, trianglesBytes);

			auto RunnerFunc = [GPUPointsBufferReadback, GPUTrianglesBufferReadback, 
				pointsBytes, pointsNumber, trianglesBytes, trianglesNumber, AsyncCallback](auto&& RunnerFunc) -> void
			{
				if (GPUPointsBufferReadback->IsReady() && GPUTrianglesBufferReadback->IsReady())
				{

					float* points = (float*) GPUPointsBufferReadback->Lock(pointsBytes);
					GPUPointsBufferReadback->Unlock();

					int32* triangles = (int32*) GPUTrianglesBufferReadback->Lock(trianglesBytes);
					GPUTrianglesBufferReadback->Unlock();


					TArray<FVector>* Points = new TArray<FVector>();
					for (int i = 0; i < pointsNumber * 3; i += 3)
					{
						Points->Add(FVector(points[i], points[i + 1], points[i + 2]));
					}

					TArray<int32>* Triangles = new TArray<int32>(triangles, trianglesNumber * 3);

					AsyncTask(ENamedThreads::GameThread, [AsyncCallback, Points, Triangles]()
						{
							AsyncCallback(*Points, *Triangles);
						});

					delete GPUPointsBufferReadback;
					delete GPUTrianglesBufferReadback;
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

void FTerrainShaderInterface::DispatchGameThread(
	FTerrainShaderDispatchParams& Params,
	TFunction<void(TArray<FVector>& Points, TArray<int32>& Triangles)> AsyncCallback)
{
	ENQUEUE_RENDER_COMMAND(SceneDrawCompletion)(
		[&Params, AsyncCallback](FRHICommandListImmediate& RHICmdList)
		{
			DispatchRenderThread(RHICmdList, Params, AsyncCallback);
		});
}

void FTerrainShaderInterface::Dispatch(
	FTerrainShaderDispatchParams& Params,
	TFunction<void(TArray<FVector>& Points, TArray<int32>& Triangles)> AsyncCallback)
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

TerrainShaderExecutor::TerrainShaderExecutor()
{}


void TerrainShaderExecutor::Execute(float globalWorldTime, int32 width, int32 height, int32 maxAltitude, int32 spacing, UProceduralMeshComponent* meshComponent)
{
	// Create a dispatch parameters struct and fill it the input array with our args
	FTerrainShaderDispatchParams* Params = 
		new FTerrainShaderDispatchParams(globalWorldTime, width, height, maxAltitude, spacing);


	// Dispatch the compute shader and wait until it completes
	// Executes the compute shader and calls the TFunction when complete.
	// Called when the results are back from the GPU.
	FTerrainShaderInterface::Dispatch(*Params,[this, meshComponent, Params](TArray<FVector>& Points, TArray<int32>& Triangles)
		{
			meshComponent->ClearAllMeshSections();
			meshComponent->CreateMeshSection(0, Points, Triangles, 
				TArray<FVector>(), TArray<FVector2D>(), TArray<FColor>(), TArray<FProcMeshTangent>(), true);

			delete &Points;
			delete &Triangles;
			delete Params;
		});
}