// Fill out your copyright notice in the Description page of Project Settings.


#include "GrassFieldComponent.h"

UGrassFieldComponent::UGrassFieldComponent()
{

}


void UGrassFieldComponent::SampleGrassData()
{
	this->points.Empty();
	grassMesh->ClearAllMeshSections();
	FProcMeshSection* meshSection = mesh->GetProcMeshSection(0);

	for (int i = 0; i < gridSize; i++)
	{
		for (int j = 0; j < gridSize; j++)
		{
			int chunkId = i + j * gridSize;
		}
	}
	for (FProcMeshVertex vertex : meshSection->ProcVertexBuffer)
	{
		FVector p = vertex.Position;
		for (float i = 0; i / density < 1.0f; i++)
		{
			float offsetX = (FMath::SRand() - 0.5) * displacement;
			float offsetY = (FMath::SRand() - 0.5) * displacement;

			FVector start = p;
			start.X += offsetX;
			start.Y += offsetY;
			start.Z += 10;

			FVector end = p;
			end.X += offsetX;
			end.Y += offsetY;
			end.Z -= 10;

			FHitResult hit;

			if (GetWorld()->LineTraceSingleByChannel(hit, start, end, mesh->GetCollisionObjectType())) 
			{
				points.Add(hit.ImpactPoint);
			}


		}
	}
}


void UGrassFieldComponent::ComputeGrass()
{
	GrassShaderExecutor exec;
	exec.Execute((float)GetWorld()->TimeSeconds, lambda, points, minHeight, maxHeight, FVector(0, 1, 0), grassMesh, 0);
}


void UGrassFieldComponent::Init()
{
	chunks.Empty();

	double stepX = bounds.GetSize().X / gridSize;
	double stepY = bounds.GetSize().Y / gridSize;

	for (int i = 0; i < gridSize; i++)
	{
		double cx = stepX * i + stepX / 2 - bounds.GetSize().X / 2 + bounds.GetCenter().X;
		for (int j = 0; j < gridSize; j++)
		{
			double cy = stepY * j + stepY / 2 - bounds.GetSize().Y / 2 + bounds.GetCenter().Y;

			uint32 id = i + j * gridSize;
			chunks.Add(GrassChunk(TArray<FVector>(), FVector(cx, cy, 0), id));
		}
	}

}

void UGrassFieldComponent::AddGrassData(FVector point)
{
	GrassChunk& nearestChunk = chunks[0];
	float minDist = (point - nearestChunk.center).Length();

	for (GrassChunk& chunk : chunks)
	{
		float dist = (point - chunk.center).Length();
		if (dist < minDist)
		{
			minDist = dist;
			nearestChunk = chunk;
		}
	}

	nearestChunk.AddGrassData(point, FVector2D());
}

//void UGrassFieldComponent::ComputeGrass()
//{
//	float globalTime = (float)GetWorld()->TimeSeconds;
//	for (GrassChunk& chunk : chunks)
//	{
//		chunk.ComputeGrass(globalTime, lambda, minHeight, maxHeight, grassMesh);
//	}
//}
