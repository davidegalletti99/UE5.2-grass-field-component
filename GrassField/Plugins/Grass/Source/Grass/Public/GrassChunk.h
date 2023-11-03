// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GrassShader.h"
#include "ProceduralMeshComponent.h"

struct GrassData
{
	FVector point;
	FVector2D uv;

	GrassData(FVector point, FVector2D uv) 
		: point(point), uv(uv) 
	{}
};

/**
 * 
 */
class GRASS_API GrassChunk
{

public:

	uint32 id;

	FVector center;
	TArray<FVector> data;

	GrassChunk();
	GrassChunk(FVector center, uint32 id);
	GrassChunk(TArray<FVector> data, FVector center, uint32 id);
	
	~GrassChunk();
	void AddGrassData(FVector point, FVector2D uv);
	void Empty();
	void ComputeGrass(float globalTime, float lambda, float minHeight, float maxHeight, UProceduralMeshComponent* grassMesh);
};
