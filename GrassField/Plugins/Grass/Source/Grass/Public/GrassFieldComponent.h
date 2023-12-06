// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/PrimitiveComponent.h"
#include "ProceduralMeshComponent.h"
#include "Engine/World.h"

#include "Math.h"
#include "GrassSceneProxy.h"
#include "GrassData.h"

#include "GrassFieldComponent.generated.h"


class UMaterialInterface;

UCLASS(Blueprintable, ClassGroup = Rendering, hideCategories = (Activation, Collision, Cooking, HLOD, Navigation, Object, Physics, VirtualTexture))
class UGrassMeshSection : public UObject
{
	GENERATED_BODY()

protected:

	/** Material applied to each instance. */
	UPROPERTY(EditAnywhere, Category = Rendering)
		UMaterialInterface* Material = nullptr;

	UPROPERTY(EditAnywhere, Category = Rendering)
		FBox Bounds = FBox();
	
	TArray<GrassMesh::FPackedGrassData>* GrassData = nullptr;

public:


	UGrassMeshSection(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	void Empty();

	void AddGrassData(GrassMesh::FPackedGrassData Data);

	void SetBounds(FBox InBounds)
	{
		Bounds = InBounds;
	}

};

UCLASS(Blueprintable, ClassGroup = Rendering, hideCategories = (Activation, Collision, Cooking, HLOD, Navigation, Object, Physics, VirtualTexture))
class GRASS_API UGrassFieldComponent : public UPrimitiveComponent
{
	GENERATED_UCLASS_BODY()

protected:
	/** Material applied to each instance. */
	UPROPERTY(EditAnywhere, Category = Rendering)
		UMaterialInterface* Material = nullptr;

	UPROPERTY(EditAnywhere, Category = Rendering)
		uint32 Dimension = 2;

	UPROPERTY(EditAnywhere, Category = Rendering)
		AActor* Terrain = nullptr;

	UPROPERTY(EditAnywhere, Category = Rendering)
		float lambda = 3.0f;

	UPROPERTY(EditAnywhere, Category = Rendering)
		float cutoffDistance = 1000.0f;

	UPROPERTY(EditAnywhere, Category = Rendering)
		int32 density = 10;

	UPROPERTY(EditAnywhere, Category = Rendering)
		int32 displacement = 8;

	UPROPERTY(EditAnywhere, Category = Rendering)
		int32 maxHeight = 12;

	UPROPERTY(EditAnywhere, Category = Rendering)
		int32 minHeight = 7;

	UPROPERTY(EditAnywhere, Category = Rendering)
		FUintVector2 LodStepsRange = FUintVector2(1, 7);

	UPROPERTY(EditAnywhere, Category = Rendering)
		TArray<UGrassMeshSection *> Sections;


public:

	UMaterialInterface* GetMaterial() const { return Material; }

	UFUNCTION(CallInEditor, Category = Rendering)
		void SampleGrassData();


	UFUNCTION(CallInEditor, Category = Rendering)
		void EmptyGrassData();


	UFUNCTION(CallInEditor, Category = Rendering)
		void InitSections();

protected:

	//~ Begin UActorComponent Interface
	virtual void OnRegister() override;
	virtual void OnUnregister() override;
	virtual void ApplyWorldOffset(const FVector& InOffset, bool bWorldShift) override;
	//~ End UActorComponent Interface

	//~ Begin USceneComponent Interface
	virtual bool IsVisible() const override;
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
	//~ EndUSceneComponent Interface

	//~ Begin UPrimitiveComponent Interface
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	virtual bool SupportsStaticLighting() const override { return true; }
	virtual void SetMaterial(int32 ElementIndex, class UMaterialInterface* Material) override;
	virtual UMaterialInterface* GetMaterial(int32 Index) const override { return Material; }
	virtual void GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials, bool bGetDebugMaterials = false) const override;
	//~ End UPrimitiveComponent Interface

private:
	TResourceArray<GrassMesh::FPackedGrassData>* GrassData;

};

