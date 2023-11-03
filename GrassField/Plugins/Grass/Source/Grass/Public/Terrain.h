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
		int32 width = 256;

	UPROPERTY(EditAnywhere, Category = "Terrain")
		int32 height = 256;

	UPROPERTY(EditAnywhere, Category = "Terrain")
		int32 amplitude = 200;

	UPROPERTY(EditAnywhere, Category = "Terrain")
		int32 spacing = 10;

	UPROPERTY(EditAnywhere, Category = "Terrain")
		UProceduralMeshComponent* meshComponent;

	UPROPERTY(EditAnywhere, Category = "Terrain")
		UProceduralMeshComponent* grassMeshComponent;
	
	UPROPERTY(EditAnywhere, Category = "Terrain")
		UGrassFieldComponent* grassFieldComponent;

	UFUNCTION(CallInEditor, Category = "Terrain")
		void ComputeMesh();

public:	
	// Sets default values for this actor's properties
	ATerrain();

	int32 GetWidth() 
	{
		return width;
	}

	int32 GetHeight()
	{
		return height;
	}

	int32 GetAmplitude()
	{
		return amplitude;
	}

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;


};
