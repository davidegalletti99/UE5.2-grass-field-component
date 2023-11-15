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

	const FPermutationDomain PermutationVector(Parameters.PermutationId);

	// These defines are used in the thread count section of our shader
	OutEnvironment.SetDefine(TEXT("THREADS_X"), NUM_THREADS_GrassShader_X);
	OutEnvironment.SetDefine(TEXT("THREADS_Y"), NUM_THREADS_GrassShader_Y);
	OutEnvironment.SetDefine(TEXT("THREADS_Z"), NUM_THREADS_GrassShader_Z);

}


// --------------------------------------------------------------
// ------------------ Compute Shader Interface ------------------
// --------------------------------------------------------------

void FGrassShaderInterface::DispatchRenderThread(
	FRHICommandListImmediate& RHICmdList,
	FGrassShaderDispatchParams& Params,
	TFunction<void(TArray<FVector>& GrassPoints, 
	TArray<int32>& GrassTriangles)> AsyncCallback)
{

	FRDGBuilder GraphBuilder(RHICmdList);

	{
		SCOPE_CYCLE_COUNTER(STAT_GrassShader_Execute);
		DECLARE_GPU_STAT(GrassShader)
			RDG_EVENT_SCOPE(GraphBuilder, "GrassShader");
		RDG_GPU_STAT_SCOPE(GraphBuilder, GrassShader);

		typename FGrassShader::FPermutationDomain PermutationVector;

		TShaderMapRef<FGrassShader> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel), PermutationVector);


		bool bIsShaderValid = ComputeShader.IsValid();

		if (Params.GrassDataBuffer.Num() > 0 && bIsShaderValid) {
			
			// SRV creation
			// GrassDataBuffer
			uint32 GrassDataNum = Params.GrassDataBuffer.Num();
			const void* RawGrassData = (void*)Params.GrassDataBuffer.GetData();
			FRDGBufferRef GrassDataBuffer = CreateStructuredBuffer(
				GraphBuilder,
				TEXT("GrassDataBuffer"),
				sizeof(FGrassData), GrassDataNum,
				RawGrassData, sizeof(FGrassData) * GrassDataNum);
			FRDGBufferSRVRef SRVGrassDataBuffer = GraphBuilder.CreateSRV(
				FRDGBufferSRVDesc(GrassDataBuffer, EPixelFormat::PF_A32B32G32R32F));


			// UAV creation
			// PointsBuffer 
			int32 steps = 7;
			int32 NumGrassPoints = GrassDataNum * (steps * 2 + 1);
			FRDGBufferRef PointsBuffer = GraphBuilder.CreateBuffer(
				FRDGBufferDesc::CreateStructuredDesc(sizeof(FVector3f), NumGrassPoints),
				TEXT("PointsBuffer"));
			FRDGBufferUAVRef UAVPointsBuffer = GraphBuilder.CreateUAV(PointsBuffer);

			// TrianglesBuffer
			int32 NumGrassTriangles = GrassDataNum * (steps * 2 - 1) * 2 * 3;
			FRDGBufferRef TrianglesBuffer = GraphBuilder.CreateBuffer(
				FRDGBufferDesc::CreateStructuredDesc(sizeof(int32), NumGrassTriangles),
				TEXT("TrianglesBuffer"));
			FRDGBufferUAVRef UAVTrianglesBuffer = GraphBuilder.CreateUAV(TrianglesBuffer);


			FGrassShader::FParameters* PassParameters = GraphBuilder.AllocParameters<FGrassShader::FParameters>();

			PassParameters->GlobalWorldTime = Params.GlobalWorldTime;
			PassParameters->MinHeight = Params.MinHeight;
			PassParameters->MaxHeight = Params.MaxHeight;
			PassParameters->Lambda = Params.Lambda;
			PassParameters->Size = Params.GrassDataBuffer.Num();

			PassParameters->GrassDataBuffer = SRVGrassDataBuffer;
			PassParameters->GrassPoints = UAVPointsBuffer;
			PassParameters->GrassTriangles = UAVTrianglesBuffer;



			auto GroupCount = FIntVector(Params.X, Params.Y, Params.Z);
			GraphBuilder.AddPass(
				RDG_EVENT_NAME("ExecuteGrassShader"),
				PassParameters,
				ERDGPassFlags::AsyncCompute,
				[&PassParameters, ComputeShader, GroupCount](FRHIComputeCommandList& RHICmdList)
				{
					FComputeShaderUtils::Dispatch(RHICmdList, ComputeShader, *PassParameters, GroupCount);
				});

			int32 pointsBytes = sizeof(FVector3f) * NumGrassPoints;
			int32 trianglesBytes = sizeof(int32) * NumGrassTriangles;

			FRHIGPUBufferReadback* GPUPointsBufferReadback = new FRHIGPUBufferReadback(TEXT("ExecuteGrassShaderPoints"));
			FRHIGPUBufferReadback* GPUTrianglesBufferReadback = new FRHIGPUBufferReadback(TEXT("ExecuteGrassShaderTriangles"));
			
			AddEnqueueCopyPass(GraphBuilder, GPUPointsBufferReadback, PointsBuffer, pointsBytes);
			AddEnqueueCopyPass(GraphBuilder, GPUTrianglesBufferReadback, TrianglesBuffer, trianglesBytes);

			auto RunnerFunc = [GPUPointsBufferReadback, GPUTrianglesBufferReadback,
				pointsBytes, NumGrassPoints, trianglesBytes, NumGrassTriangles, AsyncCallback](auto&& RunnerFunc) -> void
			{
				if (GPUPointsBufferReadback->IsReady() && GPUTrianglesBufferReadback->IsReady())
				{

					FVector3f *points = (FVector3f*)GPUPointsBufferReadback->Lock(pointsBytes);
					GPUPointsBufferReadback->Unlock();

					int32* triangles = (int32*)GPUTrianglesBufferReadback->Lock(trianglesBytes);
					GPUTrianglesBufferReadback->Unlock();


					TArray<FVector>* GrassPoints = new TArray<FVector>();
					for (int i = 0; i < NumGrassPoints; ++i)
					{
						GrassPoints->Add(FVector(points[i]));
					}
					TArray<int32>* GrassTriangles = new TArray<int32>(triangles, NumGrassTriangles);

					AsyncTask(ENamedThreads::GameThread, [AsyncCallback, GrassPoints, GrassTriangles]()
						{
							AsyncCallback(*GrassPoints, *GrassTriangles);
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

void FGrassShaderInterface::DispatchGameThread(
	FGrassShaderDispatchParams& Params,
	TFunction<void(TArray<FVector>& GrassPoints, TArray<int32>& GrassTriangles)> AsyncCallback)
{
	ENQUEUE_RENDER_COMMAND(SceneDrawCompletion)(
		[&Params, AsyncCallback](FRHICommandListImmediate& RHICmdList)
		{
			DispatchRenderThread(RHICmdList, Params, AsyncCallback);
		});
}

void FGrassShaderInterface::Dispatch(
	FGrassShaderDispatchParams& Params,
	TFunction<void(TArray<FVector>& GrassPoints, TArray<int32>& GrassTriangles)> AsyncCallback)
{
	if (IsInRenderingThread()) 
	{
		DispatchRenderThread(GetImmediateCommandList_ForRenderCommand(), Params, AsyncCallback);
	}
	else 
	{
		DispatchGameThread(Params, AsyncCallback);
	}
}


// --------------------------------------------------------------
// ------------------- Compute Shader Library -------------------
// --------------------------------------------------------------

GrassShaderExecutor::GrassShaderExecutor()
{}

void GrassShaderExecutor::DoTaskWork() 
{
	// Create a dispatch parameters struct and fill it the input array with our args
	FGrassShaderDispatchParams* Params =
		new FGrassShaderDispatchParams(
			globalWorldTime,
			points,
			minHeight,
			maxHeight,
			lambda
		);


	// Dispatch the compute shader and wait until it completes
	// Executes the compute shader and calls the TFunction when complete.

	// Called when the results are back from the GPU.
	FGrassShaderInterface::Dispatch(*Params, 
		[this, Params](TArray<FVector>& Points, TArray<int32>& Triangles)
			{
				// Chiamata bloccante al MainThread
				meshComponent->ClearMeshSection(sectionId);
				meshComponent->CreateMeshSection(sectionId, Points, Triangles,
					TArray<FVector>(), TArray<FVector2D>(), TArray<FColor>(), TArray<FProcMeshTangent>(), true);

				delete& Points;
				delete& Triangles;
				delete Params;
			});
}

bool GrassShaderExecutor::TryAbandonTask()
{
	return true;
}