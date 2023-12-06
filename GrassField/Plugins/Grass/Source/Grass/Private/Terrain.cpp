// Fill out your copyright notice in the Description page of Project Settings.


#include "Terrain.h"

// Sets default values
ATerrain::ATerrain()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;

	meshComponent = this->CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("TerrainMesh"));
	this->AddComponent(TEXT("Terrain Mesh"), false, FTransform(), meshComponent);

	grassFieldComponent = this->CreateDefaultSubobject<UGrassFieldComponent>(TEXT("GrassField"));
	this->AddComponent(TEXT("Grass Field"), false, FTransform(), grassFieldComponent);

}

// Called when the game starts or when spawned
void ATerrain::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void ATerrain::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

void ATerrain::ComputeMesh()
{
	TerrainShaderExecutor texec;
	texec.Execute((float)GetWorld()->TimeSeconds, width, height, amplitude, spacing, meshComponent);
}

