#include "TerrainShader.h"


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

		FTerrainShader::FPermutationDomain PermutationVector;

		TShaderMapRef<FTerrainShader> ComputeShader(
			GetGlobalShaderMap(GMaxRHIFeatureLevel), PermutationVector);


		bool bIsShaderValid = ComputeShader.IsValid();

		if (bIsShaderValid) {
			FTerrainShader::FParameters* PassParameters = GraphBuilder.AllocParameters<FTerrainShader::FParameters>();
			
			const int32 VerticesNumber = Params.Size.X * Params.Size.Y;
			FRDGBufferRef PointsBuffer = GraphBuilder.CreateBuffer(
				FRDGBufferDesc::CreateStructuredDesc(sizeof(FVector3f), VerticesNumber), TEXT("PointsBuffer"));
			
			FRDGBufferRef NormalsBuffer = GraphBuilder.CreateBuffer(
			FRDGBufferDesc::CreateStructuredDesc(sizeof(FVector3f), VerticesNumber), TEXT("NormalsBuffer"));
			
			FRDGBufferRef TangentsBuffer = GraphBuilder.CreateBuffer(
				FRDGBufferDesc::CreateStructuredDesc(sizeof(FVector3f), VerticesNumber), TEXT("TangentsBuffer"));

			const int32 TrianglesNumber = (Params.Size.X - 1) * (Params.Size.Y - 1) * 2 * 3;
			FRDGBufferRef TrianglesBuffer = GraphBuilder.CreateBuffer(
				FRDGBufferDesc::CreateStructuredDesc(sizeof(int32), TrianglesNumber), TEXT("TrianglesBuffer"));

			PassParameters->Time = static_cast<float>(FApp::GetCurrentTime());
			PassParameters->Size = Params.Size;
			PassParameters->Spacing = Params.Spacing;
			PassParameters->Scale = static_cast<FVector2f>(Params.Scale);
			PassParameters->Points = GraphBuilder.CreateUAV(
				FRDGBufferUAVDesc(PointsBuffer, EPixelFormat::PF_R32G32B32F));
			PassParameters->Normals = GraphBuilder.CreateUAV(
				FRDGBufferUAVDesc(NormalsBuffer, EPixelFormat::PF_R32G32B32F));
			PassParameters->Tangents = GraphBuilder.CreateUAV(
				FRDGBufferUAVDesc(TangentsBuffer, EPixelFormat::PF_R32G32B32F));
			PassParameters->Triangles = GraphBuilder.CreateUAV(
				FRDGBufferUAVDesc(TrianglesBuffer, EPixelFormat::PF_R32_SINT));
			
			FIntVector GroupCount = FIntVector(
				FMath::CeilToInt(Params.Size.X / static_cast<float>(NUM_THREADS_TerrainShader_X)),
				FMath::CeilToInt(Params.Size.Y / static_cast<float>(NUM_THREADS_TerrainShader_Y)),
				NUM_THREADS_TerrainShader_Z);

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

			FRHIGPUBufferReadback* GPUPointsBufferReadBack = new FRHIGPUBufferReadback(TEXT("ReadBackPoints"));
			FRHIGPUBufferReadback* GPUNormalsBufferReadBack = new FRHIGPUBufferReadback(TEXT("ReadBackNormals"));
			FRHIGPUBufferReadback* GPUTangentsBufferReadBack = new FRHIGPUBufferReadback(TEXT("ReadBackTangents"));
			FRHIGPUBufferReadback* GPUTrianglesBufferReadBack = new FRHIGPUBufferReadback(TEXT("ReadBackTriangles"));
			
			AddEnqueueCopyPass(GraphBuilder, GPUPointsBufferReadBack, PointsBuffer, VerticesBytes);
			AddEnqueueCopyPass(GraphBuilder, GPUNormalsBufferReadBack, NormalsBuffer, VerticesBytes);
			AddEnqueueCopyPass(GraphBuilder, GPUTangentsBufferReadBack, TangentsBuffer, VerticesBytes);
			AddEnqueueCopyPass(GraphBuilder, GPUTrianglesBufferReadBack, TrianglesBuffer, TrianglesBytes);

			auto RunnerFunc = [=](auto&& RunnerFuncRec) -> void
			{
				if (GPUPointsBufferReadBack->IsReady()
					&& GPUNormalsBufferReadBack->IsReady()
					&& GPUTangentsBufferReadBack->IsReady()
					&& GPUTrianglesBufferReadBack->IsReady())
				{

					FVector3f* points = static_cast<FVector3f*>(GPUPointsBufferReadBack->Lock(VerticesBytes));
					GPUPointsBufferReadBack->Unlock();
					
					FVector3f* normals = static_cast<FVector3f*>(GPUNormalsBufferReadBack->Lock(VerticesBytes));
					GPUNormalsBufferReadBack->Unlock();

					FVector3f* tangents = static_cast<FVector3f*>(GPUTangentsBufferReadBack->Lock(VerticesBytes));
					GPUTangentsBufferReadBack->Unlock();

					int32* triangles = static_cast<int32*>(GPUTrianglesBufferReadBack->Lock(TrianglesBytes));
					GPUTrianglesBufferReadBack->Unlock();


					TArray<FVector>* Points = new TArray<FVector>();
					TArray<FVector>* Normals = new TArray<FVector>();
					TArray<FVector>* Tangents = new TArray<FVector>();
					for (int i = 0; i < VerticesNumber; i ++)
					{
						Points->Add(static_cast<FVector>(points[i]));
						Normals->Add(static_cast<FVector>(normals[i]));
						Tangents->Add(static_cast<FVector>(tangents[i]));
					}

					TArray<int32>* Triangles = new TArray(triangles, TrianglesNumber);

					AsyncTask(ENamedThreads::GameThread, [=]()
						{
							AsyncCallback(*Points, *Normals, *Tangents, *Triangles);
						});

					delete GPUPointsBufferReadBack;
					delete GPUNormalsBufferReadBack;
					delete GPUTangentsBufferReadBack;
					delete GPUTrianglesBufferReadBack;
				}
				else
				{
					AsyncTask(ENamedThreads::ActualRenderingThread, [RunnerFuncRec]()
						{
							RunnerFuncRec(RunnerFuncRec);
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

void TerrainShaderExecutor::Execute(FUintVector Size, float Spacing, FVector2D Scale, UProceduralMeshComponent* MeshComponent)
{
	// Create a dispatch parameters struct and fill it the input array with our args
	FTerrainShaderDispatchParams* Params = 
		new FTerrainShaderDispatchParams(Size, Spacing, Scale);


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
			MeshComponent->ClearAllMeshSections();
			MeshComponent->CreateMeshSection(0, Points, Triangles, Normals,
				TArray<FVector2D>(), TArray<FColor>(), Tangents_T, true);

			delete &Points;
			delete &Normals;
			delete &Tangents;
			delete &Triangles;
			delete Params;
		});
}