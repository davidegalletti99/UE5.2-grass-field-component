// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GrassData.h"
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
	TArray<GrassMesh::FPackedGrassData> data;

	GrassChunk();
	GrassChunk(FBox bounds, uint32 id);
	GrassChunk(TArray<GrassMesh::FPackedGrassData> data, FBox bounds, uint32 id);
	
	~GrassChunk();
	bool AddGrassData(FVector point, float height);
	void ComputeGrass(
		float cutoffDistance,
		FMatrix& VP, FVector4f& cameraPosition,
		float lambda, UProceduralMeshComponent* grassMesh);
	void Empty();
};
