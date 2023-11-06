// Fill out your copyright notice in the Description page of Project Settings.


#include "DispatchGrassGPUFrustumCulling.h"

DECLARE_STATS_GROUP(TEXT("GrassGPUFrustumCulling"), STATGROUP_GrassGPUFrustumCulling, STATCAT_Advanced);
DECLARE_CYCLE_STAT(TEXT("GrassGPUFrustumCulling Execute"), STAT_GrassGPUFrustumCulling_Execute, STATGROUP_GrassGPUFrustumCulling);


void FDispatchGrassGPUFrustumCulling::DispatchRenderThread(
	FRHICommandListImmediate& RHICmdList,
	FGPUFrustumCullingParams& Params,
	TFunction<void()> AsyncCallback)
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

		TShaderMapRef<FVoteShader> VoteComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel), VotePermutationVector);
		TShaderMapRef<FScanShader> ScanComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel), ScanPermutationVector);
		TShaderMapRef<FScanGroupSumsShader> ScanGroupSumsComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel), ScanGroupSumsPermutationVector);
		TShaderMapRef<FCompactShader> CompactComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel), CompactPermutationVector);


		bool bIsShaderValid =  VoteComputeShader.IsValid() 
							&& ScanComputeShader.IsValid() 
							&& ScanGroupSumsComputeShader.IsValid() 
							&& CompactComputeShader.IsValid();

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

			// SRV creation


			// GrassDataBuffer
			const void* RawGrassData = (void*)Params.GrassDataBuffer.GetData();
			int GrassDataSize = sizeof(FVector4f);
			FRDGBufferRef GrassDataBuffer =
				CreateUploadBuffer(GraphBuilder, TEXT("GrassDataBuffer"), GrassDataSize, GrassDataNum,
					RawGrassData, GrassDataSize * GrassDataNum);
			FRDGBufferSRVRef SRVGrassDataBuffer = GraphBuilder.CreateSRV(
				FRDGBufferSRVDesc(GrassDataBuffer, EPixelFormat::PF_A32B32G32R32F));

			//FRDGBufferDesc GrassDataBufferDesc = FRDGBufferDesc::CreateStructuredDesc(GrassDataSize, GrassDataNum);
			//FRDGBufferRef GrassDataBuffer = GraphBuilder.CreateBuffer(GrassDataBufferDesc, TEXT("CustomBuffer"));
			//GraphBuilder.QueueBufferUpload(GrassDataBuffer, RawGrassData, GrassDataNum);

			//FRDGBufferSRVRef SRVGrassDataBuffer = GraphBuilder.CreateSRV(GrassDataBuffer);

			// UAV creation

			// ArgsBuffer 
			FRDGBufferRef ArgsBuffer = GraphBuilder.CreateBuffer(
				FRDGBufferDesc::CreateBufferDesc(
					sizeof(uint32), 5), TEXT("ArgsBuffer"));
			FRDGBufferUAVRef UAVArgsBuffer = GraphBuilder.CreateUAV(
				FRDGBufferUAVDesc(ArgsBuffer, EPixelFormat::PF_R32_UINT));

			// VoteBuffer
			FRDGBufferRef VoteBuffer = GraphBuilder.CreateBuffer(
				FRDGBufferDesc::CreateBufferDesc(
					sizeof(uint32), GrassDataNum), TEXT("VoteBuffer"));
			FRDGBufferUAVRef UAVVoteBuffer = GraphBuilder.CreateUAV(
				FRDGBufferUAVDesc(VoteBuffer, EPixelFormat::PF_R32_UINT));

			// ScanBuffer
			FRDGBufferRef ScanBuffer = GraphBuilder.CreateBuffer(
				FRDGBufferDesc::CreateBufferDesc(
					sizeof(uint32), GrassDataNum), TEXT("ScanBuffer"));
			FRDGBufferUAVRef UAVScanBuffer = GraphBuilder.CreateUAV(
				FRDGBufferUAVDesc(ScanBuffer, EPixelFormat::PF_R32_UINT));

			// GroupSumArrayBuffer
			FRDGBufferRef GroupSumArrayBuffer = GraphBuilder.CreateBuffer(
				FRDGBufferDesc::CreateBufferDesc(
					sizeof(uint32), numThreadGroups), TEXT("GroupSumArrayBuffer"));
			FRDGBufferUAVRef UAVGroupSumArrayBuffer = GraphBuilder.CreateUAV(
				FRDGBufferUAVDesc(GroupSumArrayBuffer, EPixelFormat::PF_R32_UINT));

			// ScannedGroupSumBuffer
			FRDGBufferRef ScannedGroupSumBuffer = GraphBuilder.CreateBuffer(
				FRDGBufferDesc::CreateBufferDesc(
					sizeof(uint32), numThreadGroups), TEXT("ScannedGroupSumBuffer"));
			FRDGBufferUAVRef UAVScannedGroupSumBuffer = GraphBuilder.CreateUAV(
				FRDGBufferUAVDesc(ScannedGroupSumBuffer, EPixelFormat::PF_R32_UINT));

			// CulledGrassDataBuffer
			FRDGBufferRef CulledGrassDataBuffer = GraphBuilder.CreateBuffer(
				FRDGBufferDesc::CreateBufferDesc(
					sizeof(FVector4f), GrassDataNum), TEXT("CulledGrassDataBuffer"));
			FRDGBufferUAVRef UAVCulledGrassDataBuffer = GraphBuilder.CreateUAV(
				FRDGBufferUAVDesc(CulledGrassDataBuffer, EPixelFormat::PF_A32B32G32R32F));

			// Parameters setup
			FVoteShader::FParameters* VotePassParameters =
				GraphBuilder.AllocParameters<FVoteShader::FParameters>();

			FScanShader::FParameters* ScanPassParameters =
				GraphBuilder.AllocParameters<FScanShader::FParameters>();

			FScanGroupSumsShader::FParameters* ScanGroupSumsPassParameters =
				GraphBuilder.AllocParameters<FScanGroupSumsShader::FParameters>();

			FCompactShader::FParameters* CompactPassParameters =
				GraphBuilder.AllocParameters<FCompactShader::FParameters>();

			VotePassParameters->MATRIX_VP = Params.VP;
			VotePassParameters->_CameraPosition = Params.CameraPosition;
			VotePassParameters->_Distance = Params.Distance;
			VotePassParameters->_GrassDataBuffer = SRVGrassDataBuffer;
			VotePassParameters->_VoteBuffer = UAVVoteBuffer;

			ScanPassParameters->_VoteBuffer = UAVVoteBuffer;
			ScanPassParameters->_ScanBuffer = UAVScanBuffer;
			ScanPassParameters->_GroupSumArray = UAVGroupSumArrayBuffer;

			ScanGroupSumsPassParameters->_NumOfGroups = numThreadGroups;
			ScanGroupSumsPassParameters->_GroupSumArrayIn = UAVGroupSumArrayBuffer;
			ScanGroupSumsPassParameters->_GroupSumArrayOut = UAVScannedGroupSumBuffer;

			CompactPassParameters->_GrassDataBuffer = SRVGrassDataBuffer;
			CompactPassParameters->_ArgsBuffer = UAVArgsBuffer;
			CompactPassParameters->_VoteBuffer = UAVVoteBuffer;
			CompactPassParameters->_ScanBuffer = UAVScanBuffer;
			CompactPassParameters->_GroupSumArray = UAVScannedGroupSumBuffer;
			CompactPassParameters->_CulledGrassOutputBuffer = UAVCulledGrassDataBuffer;

			//
			// Adding Render Passes
			//
			GraphBuilder.AddPass(
				RDG_EVENT_NAME("ExecuteVoteShader"),
				VotePassParameters,
				ERDGPassFlags::Compute,
				[&VotePassParameters, VoteComputeShader, VoteGroupCount](FRHIComputeCommandList& RHICmdList)
				{
					FComputeShaderUtils::Dispatch(
						RHICmdList, VoteComputeShader, *VotePassParameters, VoteGroupCount);
				}
			);

			GraphBuilder.AddPass(
				RDG_EVENT_NAME("ExecuteScanShader"),
				ScanPassParameters,
				ERDGPassFlags::Compute,
				[&ScanPassParameters, ScanComputeShader, ScanGroupCount](FRHIComputeCommandList& RHICmdList)
				{
					FComputeShaderUtils::Dispatch(
						RHICmdList, ScanComputeShader, *ScanPassParameters, ScanGroupCount);
				}
			);

			GraphBuilder.AddPass(
				RDG_EVENT_NAME("ExecuteScanGroupSumsShader"),
				ScanGroupSumsPassParameters,
				ERDGPassFlags::Compute,
				[&ScanGroupSumsPassParameters, ScanGroupSumsComputeShader, ScanGroupSumsGroupCount](FRHIComputeCommandList& RHICmdList)
				{
					FComputeShaderUtils::Dispatch(
						RHICmdList, ScanGroupSumsComputeShader, *ScanGroupSumsPassParameters, ScanGroupSumsGroupCount);
				}
			);

			GraphBuilder.AddPass(
				RDG_EVENT_NAME("ExecuteCompactShader"),
				CompactPassParameters,
				ERDGPassFlags::Compute,
				[&CompactPassParameters, CompactComputeShader, CompactGroupCount](FRHIComputeCommandList& RHICmdList)
				{
					FComputeShaderUtils::Dispatch(
						RHICmdList, CompactComputeShader, *CompactPassParameters, CompactGroupCount);
				}
			);

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
	TFunction<void()> AsyncCallback)
{
	ENQUEUE_RENDER_COMMAND(SceneDrawCompletion)(
		[&Params, AsyncCallback](FRHICommandListImmediate& RHICmdList)
		{
			DispatchRenderThread(RHICmdList, Params, AsyncCallback);
		});
}

void FDispatchGrassGPUFrustumCulling::Dispatch(
	FGPUFrustumCullingParams& Params,
	TFunction<void()> AsyncCallback)
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


