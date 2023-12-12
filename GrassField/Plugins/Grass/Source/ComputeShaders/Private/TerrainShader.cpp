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
		TFunction<void(TArray<FVector>& Points, TArray<FVector>& Normals, TArray<FVector>& Tangents, TArray<int32>& Triangles)> AsyncCallback)
{
	FRDGBuilder GraphBuilder(RHICmdList);
	
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
			
			const int32 VerticesNumber = Params.Width * Params.Height;
			FRDGBufferRef PointsBuffer = GraphBuilder.CreateBuffer(
				FRDGBufferDesc::CreateStructuredDesc(sizeof(FVector3f), VerticesNumber), TEXT("PointsBuffer"));
			
			FRDGBufferRef NormalsBuffer = GraphBuilder.CreateBuffer(
			FRDGBufferDesc::CreateStructuredDesc(sizeof(FVector3f), VerticesNumber), TEXT("NormalsBuffer"));
			
			FRDGBufferRef TangentsBuffer = GraphBuilder.CreateBuffer(
				FRDGBufferDesc::CreateStructuredDesc(sizeof(FVector3f), VerticesNumber), TEXT("TangentsBuffer"));

			const int32 TrianglesNumber = (Params.Width - 1) * (Params.Height - 1) * 2 * 3;
			FRDGBufferRef TrianglesBuffer = GraphBuilder.CreateBuffer(
				FRDGBufferDesc::CreateStructuredDesc(sizeof(int32), TrianglesNumber), TEXT("TrianglesBuffer"));
			

			PassParameters->GlobalWorldTime = Params.GlobalWorldTime;
			PassParameters->Width = Params.Width;
			PassParameters->Height = Params.Height;
			PassParameters->MaxAltitude = Params.MaxAltitude;
			PassParameters->Spacing = Params.Spacing;
			PassParameters->Scale = FVector2f(Params.Scale);
			PassParameters->Points = GraphBuilder.CreateUAV(
				FRDGBufferUAVDesc(PointsBuffer, EPixelFormat::PF_R32G32B32F));
			PassParameters->Normals = GraphBuilder.CreateUAV(
				FRDGBufferUAVDesc(NormalsBuffer, EPixelFormat::PF_R32G32B32F));
			PassParameters->Tangents = GraphBuilder.CreateUAV(
				FRDGBufferUAVDesc(TangentsBuffer, EPixelFormat::PF_R32G32B32F));
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

			const int32 VerticesBytes = sizeof(FVector3f) * VerticesNumber;
			const int32 TrianglesBytes = sizeof(int32) * TrianglesNumber;

			FRHIGPUBufferReadback* GPUPointsBufferReadback = new FRHIGPUBufferReadback(TEXT("ExecuteTerrainShaderPoints"));
			FRHIGPUBufferReadback* GPUNormalsBufferReadback = new FRHIGPUBufferReadback(TEXT("ExecuteTerrainShaderNormals"));
			FRHIGPUBufferReadback* GPUTangentsBufferReadback = new FRHIGPUBufferReadback(TEXT("ExecuteTerrainShaderTangents"));
			FRHIGPUBufferReadback* GPUTrianglesBufferReadback = new FRHIGPUBufferReadback(TEXT("ExecuteTerrainShaderTriangles"));
			AddEnqueueCopyPass(GraphBuilder, GPUPointsBufferReadback, PointsBuffer, VerticesBytes);
			AddEnqueueCopyPass(GraphBuilder, GPUNormalsBufferReadback, NormalsBuffer, VerticesBytes);
			AddEnqueueCopyPass(GraphBuilder, GPUTangentsBufferReadback, TangentsBuffer, VerticesBytes);
			AddEnqueueCopyPass(GraphBuilder, GPUTrianglesBufferReadback, TrianglesBuffer, TrianglesBytes);

			auto RunnerFunc = [=](auto&& RunnerFunc) -> void
			{
				if (GPUPointsBufferReadback->IsReady()
					&& GPUNormalsBufferReadback->IsReady()
					&& GPUTangentsBufferReadback->IsReady()
					&& GPUTrianglesBufferReadback->IsReady())
				{

					FVector3f* points = (FVector3f*) GPUPointsBufferReadback->Lock(VerticesBytes);
					GPUPointsBufferReadback->Unlock();
					
					FVector3f* normals = (FVector3f*) GPUNormalsBufferReadback->Lock(VerticesBytes);
					GPUNormalsBufferReadback->Unlock();

					FVector3f* tangents = (FVector3f*) GPUTangentsBufferReadback->Lock(VerticesBytes);
					GPUTangentsBufferReadback->Unlock();

					int32* triangles = (int32*) GPUTrianglesBufferReadback->Lock(TrianglesBytes);
					GPUTrianglesBufferReadback->Unlock();


					TArray<FVector>* Points = new TArray<FVector>();
					TArray<FVector>* Normals = new TArray<FVector>();
					TArray<FVector>* Tangents = new TArray<FVector>();
					for (int i = 0; i < VerticesNumber; i ++)
					{
						Points->Add(FVector(points[i]));
						Normals->Add(FVector(normals[i]));
						Tangents->Add(FVector(tangents[i]));
					}

					TArray<int32>* Triangles = new TArray(triangles, TrianglesNumber);

					AsyncTask(ENamedThreads::GameThread, [=]()
						{
							AsyncCallback(*Points, *Normals, *Tangents, *Triangles);
						});

					delete GPUPointsBufferReadback;
					delete GPUNormalsBufferReadback;
					delete GPUTangentsBufferReadback;
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
		TFunction<void(TArray<FVector>& Points, TArray<FVector>& Normals, TArray<FVector>& Tangents, TArray<int32>& Triangles)> AsyncCallback)
{
	ENQUEUE_RENDER_COMMAND(SceneDrawCompletion)(
		[&Params, AsyncCallback](FRHICommandListImmediate& RHICmdList)
		{
			DispatchRenderThread(RHICmdList, Params, AsyncCallback);
		});
}

void FTerrainShaderInterface::Dispatch(
		FTerrainShaderDispatchParams& Params,
		TFunction<void(TArray<FVector>& Points, TArray<FVector>& Normals, TArray<FVector>& Tangents, TArray<int32>& Triangles)> AsyncCallback)
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


void TerrainShaderExecutor::Execute(float globalWorldTime, int32 width, int32 height, float maxAltitude, float spacing, FVector2D scale, UProceduralMeshComponent* meshComponent)
{
	// Create a dispatch parameters struct and fill it the input array with our args
	FTerrainShaderDispatchParams* Params = 
		new FTerrainShaderDispatchParams(globalWorldTime, width, height, maxAltitude, spacing, scale);


	// Dispatch the compute shader and wait until it completes
	// Executes the compute shader and calls the TFunction when complete.
	// Called when the results are back from the GPU.
	FTerrainShaderInterface::Dispatch(*Params,
		[=](
			TArray<FVector>& Points,
			TArray<FVector>& Normals,
			TArray<FVector>& Tangents,
			TArray<int32>& Triangles)
		{
			TArray<FProcMeshTangent> Tangents_T = TArray<FProcMeshTangent>();
			for (const auto T : Tangents)
			{
				Tangents_T.Add(FProcMeshTangent(T, false));
			}
			meshComponent->ClearAllMeshSections();
			meshComponent->CreateMeshSection(0, Points, Triangles, Normals,
				TArray<FVector2D>(), TArray<FColor>(), Tangents_T, true);

			delete &Points;
			delete &Normals;
			delete &Tangents;
			delete &Triangles;
			delete Params;
		});
}