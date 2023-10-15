// Fill out your copyright notice in the Description page of Project Settings.


#include "Terrain.h"

// Sets default values
ATerrain::ATerrain()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	meshComponent = this->CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("TerrainMesh"));
	this->AddComponent(TEXT("Terrain Mesh"), false, FTransform(), meshComponent);

	grassMeshComponent = this->CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("GrassMesh"));
	this->AddComponent(TEXT("Grass Mesh"), false, FTransform(), grassMeshComponent);

	grassFieldComponent = this->CreateDefaultSubobject<UGrassFieldComponent>(TEXT("GrassField"));
	grassFieldComponent->grassMesh = grassMeshComponent;
	grassFieldComponent->mesh = meshComponent;
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
	texec.Execute((float)GetWorld()->TimeSeconds, width, height, maxAltitude, spacing, meshComponent);
}

