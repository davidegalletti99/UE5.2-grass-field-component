// Fill out your copyright notice in the Description page of Project Settings.


#include "GrassChunk.h"

GrassChunk::GrassChunk(TArray<FVector> data, FVector center, uint32 id)
	: data(data), center(center), id(id)
{
}

GrassChunk::GrassChunk(FVector center, uint32 id)
	: GrassChunk(TArray<FVector>(), center, id)
{}

GrassChunk::GrassChunk()
	: GrassChunk(TArray<FVector>(), FVector(), 0)
{}

GrassChunk::~GrassChunk() 
{}

void GrassChunk::AddGrassData(FVector point, FVector2D uv)
{
	data.Add(point);
}

void GrassChunk::Empty()
{
	data.Empty();
}

void GrassChunk::ComputeGrass(float globalTime, float lambda, float minHeight, float maxHeight, UProceduralMeshComponent* grassMesh)
{
	if (data.Num() <= 0)
		return;

	GrassShaderExecutor exec;
	exec.Execute(globalTime, lambda, data, minHeight, maxHeight, FVector(0, 1, 0), grassMesh, id);
}