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

// This will tell the engine to create the shader and where the shader entry point is.
//																	 (Entry Point)
//																		   |
//																		   v
//                       ShaderType           ShaderPath         Shader function name    Type
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

	/*
	* Here you define constants that can be used statically in the shader code.
	* Example:
	*/
	// OutEnvironment.SetDefine(TEXT("MY_CUSTOM_CONST"), TEXT("1"));

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
	TFunction<void(TArray<FVector>& GrassPoints, TArray<int32>& GrassTriangles)> AsyncCallback)
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

		if (bIsShaderValid) {
			FGrassShader::FParameters* PassParameters = GraphBuilder.AllocParameters<FGrassShader::FParameters>();

			PassParameters->GlobalWorldTime = Params.GlobalWorldTime;
			PassParameters->MinHeight = Params.MinHeight;
			PassParameters->MaxHeight = Params.MaxHeight;
			PassParameters->Lambda = Params.Lambda;
			PassParameters->Size = Params.Points.Num();

			PassParameters->Position = FVector3f(Params.Position);
			PassParameters->CameraDirection = FVector3f(Params.CameraDirection);

			const void* RawPointsData = (void*)Params.Points.GetData();
			int NumPoints = Params.Points.Num();
			int PointsSize = sizeof(FVector4f);
			FRDGBufferRef PointsBuffer = 
				CreateUploadBuffer(GraphBuilder, TEXT("PointsBuffer"), PointsSize, NumPoints, 
					RawPointsData, PointsSize * NumPoints);

			PassParameters->Points = GraphBuilder.CreateSRV(FRDGBufferSRVDesc(PointsBuffer, EPixelFormat::PF_A32B32G32R32F));

			int32 steps = 7;
			int32 NumGrassPoints = Params.Points.Num() * (steps * 2 + 1);
			FRDGBufferRef GrassPointsBuffer = GraphBuilder.CreateBuffer(
				FRDGBufferDesc::CreateBufferDesc(sizeof(FVector4f), NumGrassPoints), TEXT("GrassPointsBuffer"));
			PassParameters->GrassPoints = GraphBuilder.CreateUAV(
				FRDGBufferUAVDesc(GrassPointsBuffer, EPixelFormat::PF_A32B32G32R32F));

			int32 NumGrassTriangles = Params.Points.Num() * (steps * 2 - 1) * 2 * 3;
			FRDGBufferRef GrassTrianglesBuffer = GraphBuilder.CreateBuffer(
				FRDGBufferDesc::CreateBufferDesc(sizeof(int32), NumGrassTriangles), TEXT("GrassTrianglesBuffer"));
			PassParameters->GrassTriangles = GraphBuilder.CreateUAV(
				FRDGBufferUAVDesc(GrassTrianglesBuffer, EPixelFormat::PF_R32_SINT));



			auto GroupCount = FIntVector(Params.X, Params.Y, Params.Z);
			GraphBuilder.AddPass(
				RDG_EVENT_NAME("ExecuteGrassShader"),
				PassParameters,
				ERDGPassFlags::AsyncCompute,
				[&PassParameters, ComputeShader, GroupCount](FRHIComputeCommandList& RHICmdList)
				{
					FComputeShaderUtils::Dispatch(RHICmdList, ComputeShader, *PassParameters, GroupCount);
				});

			int32 pointsBytes = sizeof(FVector4f) * NumGrassPoints;
			int32 trianglesBytes = sizeof(int32) * NumGrassTriangles;

			FRHIGPUBufferReadback* GPUPointsBufferReadback = new FRHIGPUBufferReadback(TEXT("ExecuteTerrainShaderPoints"));
			FRHIGPUBufferReadback* GPUTrianglesBufferReadback = new FRHIGPUBufferReadback(TEXT("ExecuteTerrainShaderTriangles"));
			AddEnqueueCopyPass(GraphBuilder, GPUPointsBufferReadback, GrassPointsBuffer, pointsBytes);
			AddEnqueueCopyPass(GraphBuilder, GPUTrianglesBufferReadback, GrassTrianglesBuffer, trianglesBytes);

			auto RunnerFunc = [GPUPointsBufferReadback, GPUTrianglesBufferReadback,
				pointsBytes, NumGrassPoints, trianglesBytes, NumGrassTriangles, AsyncCallback](auto&& RunnerFunc) -> void
			{
				if (GPUPointsBufferReadback->IsReady() && GPUTrianglesBufferReadback->IsReady())
				{

					FVector4f *points = (FVector4f*)GPUPointsBufferReadback->Lock(pointsBytes);
					GPUPointsBufferReadback->Unlock();

					int32* triangles = (int32*)GPUTrianglesBufferReadback->Lock(trianglesBytes);
					GPUTrianglesBufferReadback->Unlock();


					TArray<FVector>* GrassPoints = new TArray<FVector>();
					for (int i = 0; i < NumGrassPoints; i ++)
					{
						GrassPoints->Add(FVector(	points[i][0],
													points[i][1],
													points[i][2] ));
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

void GrassShaderExecutor::Execute(float globalWorldTime, float lambda, TArray<FVector> points, float minHeight, float maxHeight, FVector cameraDirection, UProceduralMeshComponent* meshComponent, int sectionId)
{
	// Create a dispatch parameters struct and fill it the input array with our args
	FGrassShaderDispatchParams* Params =
		new FGrassShaderDispatchParams(globalWorldTime, FVector(0, 0, 0), points, minHeight, maxHeight, cameraDirection, lambda);
	

	// Dispatch the compute shader and wait until it completes
	// Executes the compute shader and calls the TFunction when complete.
	// Called when the results are back from the GPU.
	FGrassShaderInterface::Dispatch(*Params, [this, meshComponent, Params, sectionId](TArray<FVector>& Points, TArray<int32>& Triangles)
		{
			meshComponent->ClearMeshSection(sectionId);
			meshComponent->CreateMeshSection(sectionId, Points, Triangles,
				TArray<FVector>(), TArray<FVector2D>(), TArray<FColor>(), TArray<FProcMeshTangent>(), true);

			delete& Points;
			delete& Triangles;
			delete Params;
		});
}
