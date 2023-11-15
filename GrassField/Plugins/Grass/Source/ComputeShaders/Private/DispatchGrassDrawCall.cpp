// Fill out your copyright notice in the Description page of Project Settings.


#include "DispatchGrassDrawCall.h"

FDispatchGrassDrawCall::FDispatchGrassDrawCall()
{
}

FDispatchGrassDrawCall::~FDispatchGrassDrawCall()
{
}


DECLARE_STATS_GROUP(TEXT("GrassDrawCall"), STATGROUP_GrassDrawCall, STATCAT_Advanced);
DECLARE_CYCLE_STAT(TEXT("GrassDrawCall Execute"), STAT_GrassDrawCall_Execute, STATGROUP_GrassDrawCall);


void FDispatchGrassDrawCall::DispatchRenderThread(
	FRHICommandListImmediate& RHICmdList,
	FGrassDrawCallParams& Params,
	TFunction<void(TArray<FVector>& GrassPoints, TArray<int32>& GrassTriangles)> AsyncCallback)
{

	FRDGBuilder GraphBuilder(RHICmdList);

	{
		SCOPE_CYCLE_COUNTER(STAT_GrassDrawCall_Execute);
		DECLARE_GPU_STAT(GrassDrawCall)
			RDG_EVENT_SCOPE(GraphBuilder, "GrassDrawCall");
		RDG_GPU_STAT_SCOPE(GraphBuilder, GrassDrawCall);

		typename FVoteShader::FPermutationDomain VotePermutationVector;
		typename FScanShader::FPermutationDomain ScanPermutationVector;
		typename FScanGroupSumsShader::FPermutationDomain ScanGroupSumsPermutationVector;
		typename FCompactShader::FPermutationDomain CompactPermutationVector;
		typename FResetArgsShader::FPermutationDomain ResetArgsPermutationVector;
		typename FGrassShader::FPermutationDomain GrassPermutationVector;

		TShaderMapRef<FVoteShader> VoteComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel), VotePermutationVector);
		TShaderMapRef<FScanShader> ScanComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel), ScanPermutationVector);
		TShaderMapRef<FScanGroupSumsShader> ScanGroupSumsComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel), ScanGroupSumsPermutationVector);
		TShaderMapRef<FCompactShader> CompactComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel), CompactPermutationVector);
		TShaderMapRef<FResetArgsShader> ResetArgsComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel), ResetArgsPermutationVector);
		TShaderMapRef<FGrassShader> GrassComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel), GrassPermutationVector);


		bool bIsShaderValid = VoteComputeShader.IsValid()
			&& ScanComputeShader.IsValid()
			&& ScanGroupSumsComputeShader.IsValid()
			&& CompactComputeShader.IsValid()
			&& ResetArgsComputeShader.IsValid()
			&& GrassComputeShader.IsValid();

		if (Params.GrassDataBuffer.Num() > 0 && bIsShaderValid)
		{
			// Groups calculation
			int GrassDataNum = Params.GrassDataBuffer.Num();
			int numThreadGroups = FMath::CeilToInt(GrassDataNum / 128.0f);
			if (numThreadGroups > 128) {
				int powerOfTwo = 128;
				while (powerOfTwo < numThreadGroups)
					powerOfTwo *= 2;

				numThreadGroups = powerOfTwo;
			}
			else {
				while (128 % numThreadGroups != 0)
					numThreadGroups++;
			}

			int numVoteThreadGroups = FMath::CeilToInt(GrassDataNum / 128.0f);
			int numGroupScanThreadGroups = FMath::CeilToInt(GrassDataNum / 1024.0f);

			FIntVector VoteGroupCount = FIntVector(numVoteThreadGroups, 1, 1);
			FIntVector ScanGroupCount = FIntVector(numThreadGroups, 1, 1);
			FIntVector ScanGroupSumsGroupCount = FIntVector(numGroupScanThreadGroups, 1, 1);
			FIntVector CompactGroupCount = FIntVector(numThreadGroups, 1, 1);
			FIntVector ResetArgsGroupCount = FIntVector(1, 1, 1);
			FIntVector GrassGroupCount = FIntVector(numGroupScanThreadGroups, 1, 1);

			// SRV creation
			// GrassDataBuffer
			const void* RawGrassData = (void*)Params.GrassDataBuffer.GetData();
			FRDGBufferRef GrassDataBuffer = CreateStructuredBuffer(
				GraphBuilder,
				TEXT("GrassDataBuffer"),
				sizeof(FGrassData), GrassDataNum,
				RawGrassData, sizeof(FGrassData) * GrassDataNum);
			FRDGBufferSRVRef SRVGrassDataBuffer = GraphBuilder.CreateSRV(
				FRDGBufferSRVDesc(GrassDataBuffer, EPixelFormat::PF_R32_UINT));

			// UAV creation
			// ArgsBuffer
			FRDGBufferRef ArgsBuffer = GraphBuilder.CreateBuffer(
				FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), 5),
				TEXT("ArgsBuffer"));
			FRDGBufferUAVRef UAVArgsBuffer = GraphBuilder.CreateUAV(ArgsBuffer);

			// VoteBuffer
			FRDGBufferRef VoteBuffer = GraphBuilder.CreateBuffer(
				FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), GrassDataNum),
				TEXT("VoteBuffer"));
			FRDGBufferUAVRef UAVVoteBuffer = GraphBuilder.CreateUAV(VoteBuffer);

			// ScanBuffer
			FRDGBufferRef ScanBuffer = GraphBuilder.CreateBuffer(
				FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), GrassDataNum),
				TEXT("ScanBuffer"));
			FRDGBufferUAVRef UAVScanBuffer = GraphBuilder.CreateUAV(ScanBuffer);

			// GroupSumArrayBuffer
			FRDGBufferRef GroupSumArrayBuffer = GraphBuilder.CreateBuffer(
				FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), numThreadGroups),
				TEXT("GroupSumArrayBuffer"));
			FRDGBufferUAVRef UAVGroupSumArrayBuffer = GraphBuilder.CreateUAV(GroupSumArrayBuffer);

			// ScannedGroupSumBuffer
			FRDGBufferRef ScannedGroupSumBuffer = GraphBuilder.CreateBuffer(
				FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), numThreadGroups),
				TEXT("ScannedGroupSumBuffer"));
			FRDGBufferUAVRef UAVScannedGroupSumBuffer = GraphBuilder.CreateUAV(ScannedGroupSumBuffer);

			// CulledGrassDataBuffer
			FRDGBufferRef CulledGrassDataBuffer = GraphBuilder.CreateBuffer(
				FRDGBufferDesc::CreateStructuredDesc(sizeof(FGrassData), GrassDataNum),
				TEXT("CulledGrassDataBuffer"));
			FRDGBufferUAVRef UAVCulledGrassDataBuffer = GraphBuilder.CreateUAV(CulledGrassDataBuffer);

			int32 steps = 7;
			int32 GrassPointsNum = GrassDataNum * (steps * 2 + 1);
			int32 GrassTrianglesNum = GrassDataNum * (steps * 2 - 1) * 3 * 2;
			// GrassPointsBuffer
			FRDGBufferRef GrassPointsBuffer = GraphBuilder.CreateBuffer(
				FRDGBufferDesc::CreateStructuredDesc(sizeof(FVector3f), GrassPointsNum),
				TEXT("GrassPointsBuffer"));
			FRDGBufferUAVRef UAVGrassPointsBuffer = GraphBuilder.CreateUAV(GrassPointsBuffer);

			// GrassTrianglesBuffer
			FRDGBufferRef GrassTrianglesBuffer = GraphBuilder.CreateBuffer(
				FRDGBufferDesc::CreateStructuredDesc(sizeof(int32), GrassTrianglesNum),
				TEXT("GrassTrianglesBuffer"));
			FRDGBufferUAVRef UAVGrassTrianglesBuffer = GraphBuilder.CreateUAV(GrassTrianglesBuffer);

			// Parameters setup
			FResetArgsShader::FParameters* ResetArgsPassParameters =
				GraphBuilder.AllocParameters<FResetArgsShader::FParameters>();

			FVoteShader::FParameters* VotePassParameters =
				GraphBuilder.AllocParameters<FVoteShader::FParameters>();

			FScanShader::FParameters* ScanPassParameters =
				GraphBuilder.AllocParameters<FScanShader::FParameters>();

			FScanGroupSumsShader::FParameters* ScanGroupSumsPassParameters =
				GraphBuilder.AllocParameters<FScanGroupSumsShader::FParameters>();

			FCompactShader::FParameters* CompactPassParameters =
				GraphBuilder.AllocParameters<FCompactShader::FParameters>();

			FGrassShader::FParameters* GrassPassParameters =
				GraphBuilder.AllocParameters<FGrassShader::FParameters>();

			ResetArgsPassParameters->ArgsBuffer = UAVArgsBuffer;

			VotePassParameters->MATRIX_VP = Params.VP;
			VotePassParameters->CameraPosition = Params.CameraPosition;
			VotePassParameters->Distance = Params.Distance;
			VotePassParameters->GrassDataBuffer = SRVGrassDataBuffer;
			VotePassParameters->VoteBuffer = UAVVoteBuffer;

			ScanPassParameters->VoteBuffer = UAVVoteBuffer;
			ScanPassParameters->ScanBuffer = UAVScanBuffer;
			ScanPassParameters->GroupSumArray = UAVGroupSumArrayBuffer;

			ScanGroupSumsPassParameters->NumOfGroups = numThreadGroups;
			ScanGroupSumsPassParameters->GroupSumArrayIn = UAVGroupSumArrayBuffer;
			ScanGroupSumsPassParameters->GroupSumArrayOut = UAVScannedGroupSumBuffer;

			CompactPassParameters->GrassDataBuffer = SRVGrassDataBuffer;
			CompactPassParameters->ArgsBuffer = UAVArgsBuffer;
			CompactPassParameters->VoteBuffer = UAVVoteBuffer;
			CompactPassParameters->ScanBuffer = UAVScanBuffer;
			CompactPassParameters->GroupSumArray = UAVScannedGroupSumBuffer;
			CompactPassParameters->CulledGrassOutputBuffer = UAVCulledGrassDataBuffer;

			GrassPassParameters->Lambda = Params.Lambda;
			GrassPassParameters->ArgsBuffer = UAVArgsBuffer;
			GrassPassParameters->GrassDataBuffer = UAVCulledGrassDataBuffer;
			GrassPassParameters->GrassPoints = UAVGrassPointsBuffer;
			GrassPassParameters->GrassTriangles = UAVGrassTrianglesBuffer;

			//
			// Adding Render Passes
			//
			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("ExecuteResetArgsShader"),
				ResetArgsComputeShader,
				ResetArgsPassParameters,
				ResetArgsGroupCount
			);

			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("ExecuteVoteShader"),
				VoteComputeShader,
				VotePassParameters,
				VoteGroupCount
			);

			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("ExecuteScanShader"),
				ScanComputeShader,
				ScanPassParameters,
				ScanGroupCount
			);

			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("ExecuteScanGroupSumsShader"),
				ScanGroupSumsComputeShader,
				ScanGroupSumsPassParameters,
				ScanGroupSumsGroupCount
			);

			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("ExecuteCompactShader"),
				CompactComputeShader,
				CompactPassParameters,
				CompactGroupCount
			);

			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("ExecuteGrassShader"),
				GrassComputeShader,
				GrassPassParameters,
				GrassGroupCount
			);

			int32 pointsBytes = sizeof(FVector3f) * GrassPointsNum;
			int32 trianglesBytes = sizeof(int32) * GrassTrianglesNum;
			int32 argsBytes = sizeof(uint32) * 5;

			FRHIGPUBufferReadback* GPUArgsBufferReadback = new FRHIGPUBufferReadback(TEXT("ExecuteArgsReadback"));
			FRHIGPUBufferReadback* GPUPointsBufferReadback = new FRHIGPUBufferReadback(TEXT("ExecuteGrassPointsReadback"));
			FRHIGPUBufferReadback* GPUTrianglesBufferReadback = new FRHIGPUBufferReadback(TEXT("ExecuteGrassTrianglesReadback"));

			AddEnqueueCopyPass(GraphBuilder, GPUArgsBufferReadback, ArgsBuffer, argsBytes);
			AddEnqueueCopyPass(GraphBuilder, GPUPointsBufferReadback, GrassPointsBuffer, pointsBytes);
			AddEnqueueCopyPass(GraphBuilder, GPUTrianglesBufferReadback, GrassTrianglesBuffer, trianglesBytes);
			
			UE_LOG(LogTemp, Warning, TEXT("GrassDrawCall"));
			auto RunnerFunc = [GPUArgsBufferReadback, GPUPointsBufferReadback, GPUTrianglesBufferReadback, argsBytes, AsyncCallback](auto&& RunnerFunc) -> void
			{
				if (   GPUPointsBufferReadback->IsReady() 
					&& GPUTrianglesBufferReadback->IsReady() 
					&& GPUArgsBufferReadback->IsReady())
				{
					uint32* args = (uint32*)GPUArgsBufferReadback->Lock(argsBytes);
					uint32 numPoints = args[2];
					uint32 numTriangles = args[3];

					int32 pointsBytes = numPoints * sizeof(FVector3f);
					FVector3f* points = (FVector3f*)GPUPointsBufferReadback->Lock(pointsBytes);

					int32 trianglesBytes = numTriangles * sizeof(int32);
					int32* triangles = (int32*)GPUTrianglesBufferReadback->Lock(trianglesBytes);

					GPUArgsBufferReadback->Unlock();
					GPUPointsBufferReadback->Unlock();
					GPUTrianglesBufferReadback->Unlock();

					TArray<FVector>* GrassPoints = new TArray<FVector>();
					for (uint32 i = 0; i < numPoints; ++i)
					{
						GrassPoints->Add(FVector(points[i]));
					}
					TArray<int32>* GrassTriangles = new TArray<int32>(triangles, numTriangles);

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

void FDispatchGrassDrawCall::DispatchGameThread(
	FGrassDrawCallParams& Params,
	TFunction<void(TArray<FVector>& GrassPoints, TArray<int32>& GrassTriangles)> AsyncCallback)
{
	ENQUEUE_RENDER_COMMAND(SceneDrawCompletion)(
		[&Params, AsyncCallback](FRHICommandListImmediate& RHICmdList)
		{
			DispatchRenderThread(RHICmdList, Params, AsyncCallback);
		});
}

void FDispatchGrassDrawCall::Dispatch(
	FGrassDrawCallParams& Params,
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


