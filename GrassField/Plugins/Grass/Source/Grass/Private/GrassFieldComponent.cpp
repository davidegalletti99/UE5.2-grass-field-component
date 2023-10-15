// Fill out your copyright notice in the Description page of Project Settings.


#include "GrassFieldComponent.h"
#include "Math.h"
#include "GrassShader.h"

UGrassFieldComponent::UGrassFieldComponent()
{

}

void UGrassFieldComponent::SampleGrassData()
{
	this->points.Empty();
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
				points.Add(hit.ImpactPoint);
			}


		}
	}

}

void UGrassFieldComponent::ComputeGrass()
{
	GrassShaderExecutor exec;
	exec.Execute((float)GetWorld()->TimeSeconds, lambda, points, minHeight, maxHeight, grassMesh);
}