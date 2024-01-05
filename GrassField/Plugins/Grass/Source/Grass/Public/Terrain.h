// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProceduralMeshComponent.h"
#include "Math/UnrealMathUtility.h"
#include "GrassFieldComponent.h"
#include "TerrainShader.h"

#include "Terrain.generated.h"

UCLASS()
class GRASS_API ATerrain : public AActor
{
	GENERATED_BODY()


	UPROPERTY(EditAnywhere, Category = "Terrain")
		int32 Width = 128;

	UPROPERTY(EditAnywhere, Category = "Terrain")
		int32 Height = 128;

	UPROPERTY(EditAnywhere, Category = "Terrain")
		float Amplitude = 200;

	UPROPERTY(EditAnywhere, Category = "Terrain")
		float Spacing = 10;

	UPROPERTY(EditAnywhere, Category = "Terrain")
		FVector2D Scale = FVector2D(1, 1);

	UFUNCTION(CallInEditor, Category = "Terrain")
		void ComputeMesh();

public:
	UPROPERTY(EditAnywhere, Category = "Terrain")
		UProceduralMeshComponent* MeshComponent;
	
	UPROPERTY(EditAnywhere, Category = "Terrain")
		UGrassFieldComponent* GrassFieldComponent ;




protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	ATerrain();

};
