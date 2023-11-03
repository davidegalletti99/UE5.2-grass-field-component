// Fill out your copyright notice in the Description page of Project Settings.


#include "GrassFieldComponent.h"

UGrassFieldComponent::UGrassFieldComponent()
{

}


void UGrassFieldComponent::SampleGrassData()
{
	Init();

	grassMesh->ClearAllMeshSections();
	FProcMeshSection* meshSection = mesh->GetProcMeshSection(0);
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
				AddGrassData(hit.ImpactPoint);
			}


		}
	}
}


void UGrassFieldComponent::Init()
{
	chunks.Empty();

	double stepX = bounds.GetSize().X / gridSize;
	double stepY = bounds.GetSize().Y / gridSize;

	for (int i = 0; i < gridSize; i++)
	{
		double cx = (stepX * i + stepX / 2) + (bounds.GetCenter().X - bounds.GetSize().X / 2);
		for (int j = 0; j < gridSize; j++)
		{
			double cy = (stepY * j + stepY / 2) + (bounds.GetCenter().Y - bounds.GetSize().Y / 2);

			uint32 id = i + j * gridSize;
			chunks.Add(GrassChunk(TArray<FVector>(), FVector(cx, cy, 0), id));
		}
	}

}

void UGrassFieldComponent::AddGrassData(FVector point)
{
	int id = 0;
	float minDist = (point - chunks[0].center).Length();

	for (int i = 1; i < chunks.Num(); i++)
	{
		float dist = (point - chunks[i].center).Length();
		if (dist < minDist)
		{
			minDist = dist;
			id = i;
		}
	}

	chunks[id].AddGrassData(point, FVector2D());
}

void UGrassFieldComponent::ComputeGrass()
{
	float globalTime = (float)GetWorld()->TimeSeconds;
	for (GrassChunk& chunk : chunks)
	{
		chunk.ComputeGrass(globalTime, lambda, minHeight, maxHeight, grassMesh);
	}
}
