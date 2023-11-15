// Fill out your copyright notice in the Description page of Project Settings.


#include "GrassFieldComponent.h"
#include "DispatchGrassGPUFrustumCulling.h"
#include <Kismet/GameplayStatics.h>
#include "GameFramework/HUD.h"

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
				float height = FMath::Lerp(minHeight, maxHeight, FMath::SRand());
				AddGrassData(hit.ImpactPoint, height);
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
			chunks.Add(GrassChunk(FBox(pMin, pMax), id));
		}
	}

}

void UGrassFieldComponent::AddGrassData(FVector point, float height)
{
	bool isAdded = false;
	for (int i = 0; i < chunks.Num() && !isAdded; i++)
	{
		isAdded = chunks[i].AddGrassData(point, height);
	}
}

void UGrassFieldComponent::ComputeGrass()
{
	ULocalPlayer* LocalPlayer = GetWorld()->GetFirstLocalPlayerFromController();

	FSceneViewProjectionData ProjectionData;
	LocalPlayer->GetProjectionData(LocalPlayer->ViewportClient->Viewport, ProjectionData);
	FMatrix VP = ProjectionData.ComputeViewProjectionMatrix();
	FVector4f cameraPosition = FVector4f(FVector3f(LocalPlayer->LastViewLocation), 1.0f);
	FIntRect theViewRect = ProjectionData.GetViewRect();

	for (GrassChunk& chunk : chunks)
	{
		chunk.ComputeGrass(cutoffDistance,
			VP, cameraPosition,
			lambda, grassMesh);
	}
}