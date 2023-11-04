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
	FBox bounds;
	TArray<FVector> data;

	GrassChunk();
	GrassChunk(FBox bounds, uint32 id);
	GrassChunk(TArray<FVector> data, FBox bounds, uint32 id);
	
	~GrassChunk();
	bool AddGrassData(FVector point, FVector2D uv);
	void ComputeGrass(float globalTime, float lambda, float minHeight, float maxHeight, UProceduralMeshComponent* grassMesh);
	void Empty();
};
