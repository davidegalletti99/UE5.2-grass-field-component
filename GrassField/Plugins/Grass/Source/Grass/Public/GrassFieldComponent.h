// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/PrimitiveComponent.h"
#include "Components/BrushComponent.h"
#include "ProceduralMeshComponent.h"
#include "Engine/World.h"
#include "GrassShader.h"

#include "GrassFieldComponent.generated.h"

/**
 * 
 */
UCLASS()
class GRASS_API UGrassFieldComponent : public UPrimitiveComponent
{
	GENERATED_BODY()
public:
	UGrassFieldComponent();


	UProceduralMeshComponent* mesh;

	UProceduralMeshComponent* grassMesh;
private:

	TArray<FVector> points;


	UPROPERTY(EditAnywhere, Category = "Grass")
		float lambda = 10;

	UPROPERTY(EditAnywhere, Category = "Grass")
		int32 density = 10;

	UPROPERTY(EditAnywhere, Category = "Grass")
		int32 displacement = 8;

	UPROPERTY(EditAnywhere, Category = "Grass")
		int32 maxHeight = 5;

	UPROPERTY(EditAnywhere, Category = "Grass")
		int32 minHeight = 1;


	UFUNCTION(CallInEditor, Category = "Grass")
		void SampleGrassData();

	UFUNCTION(CallInEditor, Category = "Grass")
		void ComputeGrass();
	
};