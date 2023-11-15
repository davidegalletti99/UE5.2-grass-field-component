// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "RendererInterface.h"
#include "RenderResource.h"
#include "RenderTargetPool.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphResources.h"
#include "RenderGraphUtils.h"
#include "Renderer/Private/ScenePrivate.h"

#include "GrassShader.h"
#include "GrassGPUFrustumCulling.h"

struct COMPUTESHADERS_API FGrassDrawCallParams
{
	FMatrix44f VP;
	FVector4f CameraPosition;
	float Distance; // Cutoff distance
	TArray<FGrassData> GrassDataBuffer;
	float Lambda;
};

/**
 * 
 */
class COMPUTESHADERS_API FDispatchGrassDrawCall
{
public:
	FDispatchGrassDrawCall();
	~FDispatchGrassDrawCall();

	// Executes this shader on the render thread
	static void DispatchRenderThread(
		FRHICommandListImmediate& RHICmdList,
		FGrassDrawCallParams& Params,
		TFunction<void(TArray<FVector>& GrassPoints, TArray<int32>& GrassTriangles)> AsyncCallback);

	// Executes this shader on the render thread from the game thread via EnqueueRenderThreadCommand
	static void DispatchGameThread(
		FGrassDrawCallParams& Params,
		TFunction<void(TArray<FVector>& GrassPoints, TArray<int32>& GrassTriangles)> AsyncCallback);

	// Dispatches this shader. Can be called from any thread
	static void Dispatch(
		FGrassDrawCallParams& Params,
		TFunction<void(TArray<FVector>& GrassPoints, TArray<int32>& GrassTriangles)> AsyncCallback);
};
