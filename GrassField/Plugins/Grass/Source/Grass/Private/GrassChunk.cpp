// Fill out your copyright notice in the Description page of Project Settings.


#include "GrassChunk.h"

GrassChunk::GrassChunk(TArray<FVector> data, FBox bounds, uint32 id)
	: data(data), bounds(bounds), id(id)
{}

GrassChunk::GrassChunk(FBox bounds, uint32 id)
	: GrassChunk(TArray<FVector>(), bounds, id)
{}

GrassChunk::GrassChunk()
	: GrassChunk(TArray<FVector>(), FBox(), 0)
{}

GrassChunk::~GrassChunk() 
{}

bool GrassChunk::AddGrassData(FVector point, FVector2D uv)
{
	bool result = FMath::PointBoxIntersection(point, bounds);

	if(result)
		data.Add(point);

	return result;
}

void GrassChunk::Empty()
{
	data.Empty();
}

void GrassChunk::ComputeGrass(float globalTime, float lambda, float minHeight, float maxHeight, UProceduralMeshComponent* grassMesh)
{
	if (data.Num() <= 0)
		return;

	GrassShaderExecutor* exec = new GrassShaderExecutor();
	exec->cameraDirection = FVector(0, 1, 0);
	exec->globalWorldTime = globalTime;
	exec->lambda = lambda;
	exec->minHeight = minHeight;
	exec->maxHeight = maxHeight;
	exec->meshComponent = grassMesh;

	exec->sectionId = id;
	exec->points = data;

	exec->StartBackgroundTask();
	//delete exec;
	//exec.Execute(globalTime, lambda, data, minHeight, maxHeight, FVector(0, 1, 0), grassMesh, id);
}