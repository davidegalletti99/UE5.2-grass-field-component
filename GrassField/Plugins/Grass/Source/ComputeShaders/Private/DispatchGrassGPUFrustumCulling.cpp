// Fill out your copyright notice in the Description page of Project Settings.


#include "DispatchGrassGPUFrustumCulling.h"

DECLARE_STATS_GROUP(TEXT("GrassGPUFrustumCulling"), STATGROUP_GrassGPUFrustumCulling, STATCAT_Advanced);
DECLARE_CYCLE_STAT(TEXT("GrassGPUFrustumCulling Execute"), STAT_GrassGPUFrustumCulling_Execute, STATGROUP_GrassGPUFrustumCulling);


void FDispatchGrassGPUFrustumCulling::DispatchRenderThread(
	FRHICommandListImmediate& RHICmdList,
	FBaseGPUFrustumCullingParams& Params,
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

		if (bIsShaderValid) 
		{
			FVoteShader::FParameters* VotePassParameters = 
				GraphBuilder.AllocParameters<FVoteShader::FParameters>();

			FScanShader::FParameters* ScanPassParameters =
				GraphBuilder.AllocParameters<FScanShader::FParameters>();

			FScanGroupSumsShader::FParameters* ScanGroupSumsPassParameters =
				GraphBuilder.AllocParameters<FScanGroupSumsShader::FParameters>();

			FCompactShader::FParameters* CompactPassParameters =
				GraphBuilder.AllocParameters<FCompactShader::FParameters>();

			FIntVector VoteGroupCount = FIntVector(1, 1, 1);
			FIntVector ScanGroupCount = FIntVector(1, 1, 1);
			FIntVector ScanGroupSumsGroupCount = FIntVector(1, 1, 1);
			FIntVector CompactGroupCount = FIntVector(1, 1, 1);

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
	FBaseGPUFrustumCullingParams& Params,
	TFunction<void()> AsyncCallback)
{
	ENQUEUE_RENDER_COMMAND(SceneDrawCompletion)(
		[&Params, AsyncCallback](FRHICommandListImmediate& RHICmdList)
		{
			DispatchRenderThread(RHICmdList, Params, AsyncCallback);
		});
}

void FDispatchGrassGPUFrustumCulling::Dispatch(
	FBaseGPUFrustumCullingParams& Params,
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


