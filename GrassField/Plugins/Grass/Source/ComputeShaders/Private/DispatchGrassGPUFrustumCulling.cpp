// Fill out your copyright notice in the Description page of Project Settings.


#include "DispatchGrassGPUFrustumCulling.h"

DECLARE_STATS_GROUP(TEXT("GrassGPUFrustumCulling"), STATGROUP_GrassGPUFrustumCulling, STATCAT_Advanced);
DECLARE_CYCLE_STAT(TEXT("GrassGPUFrustumCulling Execute"), STAT_GrassGPUFrustumCulling_Execute, STATGROUP_GrassGPUFrustumCulling);


void FDispatchGrassGPUFrustumCulling::DispatchRenderThread(
	FRHICommandListImmediate& RHICmdList,
	FGPUFrustumCullingParams& Params,
	TFunction<void(TArray<FGrassData>& grassData)> AsyncCallback)
{

	FRDGBuilder GraphBuilder(RHICmdList);

	{
		SCOPE_CYCLE_COUNTER(STAT_GrassGPUFrustumCulling_Execute);
		DECLARE_GPU_STAT(GrassGPUFrustumCulling)
			RDG_EVENT_SCOPE(GraphBuilder, "GrassGPUFrustumCulling");
		RDG_GPU_STAT_SCOPE(GraphBuilder, GrassGPUFrustumCulling);

		typename FVoteShader::FPermutationDomain VotePermutationVector;
		typename FScanShader::FPermutationDomain ScanPermutationVector;
		typename FScanGroupSumsShader::FPermutationDomain ScanGroupSumsPermutationVector;
		typename FCompactShader::FPermutationDomain CompactPermutationVector;
		typename FResetArgsShader::FPermutationDomain ResetArgsPermutationVector;

		TShaderMapRef<FVoteShader> VoteComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel), VotePermutationVector);
		TShaderMapRef<FScanShader> ScanComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel), ScanPermutationVector);
		TShaderMapRef<FScanGroupSumsShader> ScanGroupSumsComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel), ScanGroupSumsPermutationVector);
		TShaderMapRef<FCompactShader> CompactComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel), CompactPermutationVector);
		TShaderMapRef<FResetArgsShader> ResetArgsComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel), ResetArgsPermutationVector);



		bool bIsShaderValid =  VoteComputeShader.IsValid() 
							&& ScanComputeShader.IsValid() 
							&& ScanGroupSumsComputeShader.IsValid() 
							&& CompactComputeShader.IsValid()
							&& ResetArgsComputeShader.IsValid();

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

			int32 grassBytes = sizeof(FGrassData) * GrassDataNum;
			int32 argsBytes = sizeof(uint32) * 5;

			FRHIGPUBufferReadback* GPUCulledGrassDataReadback = new FRHIGPUBufferReadback(TEXT("ExecuteCulledGrassData"));
			FRHIGPUBufferReadback* GPUArgsReadback = new FRHIGPUBufferReadback(TEXT("ExecuteArgs"));

			AddEnqueueCopyPass(GraphBuilder, GPUCulledGrassDataReadback, CulledGrassDataBuffer, grassBytes);
			AddEnqueueCopyPass(GraphBuilder, GPUArgsReadback, ArgsBuffer, argsBytes);

			auto RunnerFunc = [GPUCulledGrassDataReadback, grassBytes, GPUArgsReadback, argsBytes, AsyncCallback]
				(auto&& RunnerFunc) -> void
			{
				if (GPUCulledGrassDataReadback->IsReady() && GPUArgsReadback->IsReady())
				{
					uint32* args = (uint32*) GPUArgsReadback->Lock(argsBytes);
					uint32 CulledGrassNum = args[1];
					uint32 CulledGrassSize = CulledGrassNum * sizeof(FGrassData);

					FGrassData* grassDataArr = (FGrassData*) GPUCulledGrassDataReadback->Lock(CulledGrassSize);
					TArray<FGrassData>* GrassData = new TArray<FGrassData>(grassDataArr, CulledGrassNum);

					GPUArgsReadback->Unlock();
					GPUCulledGrassDataReadback->Unlock();

					AsyncTask(ENamedThreads::GameThread, [GrassData, AsyncCallback]()
						{
							AsyncCallback(*GrassData);
						});

					delete GPUCulledGrassDataReadback;
					delete GPUArgsReadback;
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

void FDispatchGrassGPUFrustumCulling::DispatchGameThread(
	FGPUFrustumCullingParams& Params,
	TFunction<void(TArray<FGrassData>& grassData)> AsyncCallback)
{
	ENQUEUE_RENDER_COMMAND(SceneDrawCompletion)(
		[&Params, AsyncCallback](FRHICommandListImmediate& RHICmdList)
		{
			DispatchRenderThread(RHICmdList, Params, AsyncCallback);
		});
}

void FDispatchGrassGPUFrustumCulling::Dispatch(
	FGPUFrustumCullingParams& Params,
	TFunction<void(TArray<FGrassData>& grassData)> AsyncCallback)
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


