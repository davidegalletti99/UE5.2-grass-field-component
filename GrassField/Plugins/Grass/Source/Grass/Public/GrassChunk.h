// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GrassShader.h"
#include "DispatchGrassGPUFrustumCulling.h"
#include "ProceduralMeshComponent.h"

/**
 * 
 */
class GRASS_API GrassChunk
{

public:

	uint32 id;

	FVector center;
	FBox bounds;
	TArray<FGrassData> data;

	GrassChunk();
	GrassChunk(FBox bounds, uint32 id);
	GrassChunk(TArray<FGrassData> data, FBox bounds, uint32 id);
	
	~GrassChunk();
	bool AddGrassData(FVector point, FVector2D uv);
	void ComputeGrass(
		float globalTime, float cutoffDistance,
		FMatrix& VP, FVector4f& cameraPosition,
		float lambda, float minHeight, float maxHeight,
		UProceduralMeshComponent* grassMesh);
	void Empty();
};
