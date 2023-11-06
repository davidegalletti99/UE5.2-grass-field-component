// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GrassGPUFrustumCulling.h"

#include "RendererInterface.h"
#include "RenderResource.h"
#include "RenderTargetPool.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphResources.h"
#include "RenderGraphUtils.h"
#include "Renderer/Private/ScenePrivate.h"

/**
 * Class that dispatch the GPU Frustum Culling Algo.
 * to the Render Thread
 */
class COMPUTESHADERS_API FDispatchGrassGPUFrustumCulling {
public:
	// Executes this shader on the render thread
	static void DispatchRenderThread(
		FRHICommandListImmediate& RHICmdList,
		FGPUFrustumCullingParams& Params,
		TFunction<void()> AsyncCallback);

	// Executes this shader on the render thread from the game thread via EnqueueRenderThreadCommand
	static void DispatchGameThread(
		FGPUFrustumCullingParams& Params,
		TFunction<void()> AsyncCallback);

	// Dispatches this shader. Can be called from any thread
	static void Dispatch(
		FGPUFrustumCullingParams& Params,
		TFunction<void()> AsyncCallback);
};