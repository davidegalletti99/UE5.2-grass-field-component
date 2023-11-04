// Fill out your copyright notice in the Description page of Project Settings.


#include "GrassFieldComponent.h"

UGrassFieldComponent::UGrassFieldComponent()
{

}


void UGrassFieldComponent::SampleGrassData()
{
	for (GrassChunk& chunk : chunks)
		chunk.Empty();

	grassMesh->ClearAllMeshSections();
	FProcMeshSection* meshSection = mesh->GetProcMeshSection(0);
	for (FProcMeshVertex vertex : meshSection->ProcVertexBuffer)
	{
		FVector p = vertex.Position + mesh->GetActorPositionForRenderer();
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


void UGrassFieldComponent::ChunksInit()
{
	bounds = mesh->GetLocalBounds().GetBox();

	chunks.Empty();

	double stepX = bounds.GetSize().X / gridSize;
	double stepY = bounds.GetSize().Y / gridSize;
	double worldOffX = bounds.GetCenter().X - bounds.GetSize().X / 2;
	double worldOffY = bounds.GetCenter().Y - bounds.GetSize().Y / 2;

	FVector pMin, pMax;
	pMin.Z = bounds.GetCenter().Z - bounds.GetSize().Z / 2;
	pMax.Z = bounds.GetCenter().Z + bounds.GetSize().Z / 2;

	for (int i = 0; i < gridSize; i++)
	{
		pMin.X = stepX * i + worldOffX;
		pMax.X = stepX * (i + 1) + worldOffX;
		for (int j = 0; j < gridSize; j++)
		{
			pMin.Y = stepY * j + worldOffY;
			pMax.Y = stepY * (j + 1) + worldOffY;

			uint32 id = i + j * gridSize;
			chunks.Add(GrassChunk(TArray<FVector>(), FBox(pMin, pMax), id));
		}
	}

}

void UGrassFieldComponent::AddGrassData(FVector point)
{
	bool isAdded = false;
	for (int i = 0; i < chunks.Num() && !isAdded; i++)
	{
		isAdded = chunks[i].AddGrassData(point, FVector2D());
	}
}

void UGrassFieldComponent::ComputeGrass()
{
	float globalTime = (float)GetWorld()->TimeSeconds;
	for (GrassChunk& chunk : chunks)
	{
		chunk.ComputeGrass(globalTime, lambda, minHeight, maxHeight, grassMesh);
	}
}