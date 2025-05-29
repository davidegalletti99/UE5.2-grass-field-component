// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/PrimitiveComponent.h"
#include "ProceduralMeshComponent.h"
#include "Engine/World.h"

#include "Math.h"
#include "GrassData.h"

#include "GrassInstancingSceneProxy.h"
// #include "GrassSceneProxy.h"
#include "GrassUtils.h"
#include "GrassFieldComponent.generated.h"


class UMaterialInterface;
class UGrassFieldComponent;
class UGrassMeshSection;

UCLASS(Blueprintable, ClassGroup = Rendering, hideCategories = (Activation, Collision, Cooking, HLOD, Navigation, Object, Physics, VirtualTexture))
class UGrassMeshSection : public UObject
{
	GENERATED_BODY()
protected:

	UPROPERTY(VisibleAnywhere, Category = Rendering)
		FBox Bounds = FBox();
	
	UPROPERTY(VisibleAnywhere, Category = Rendering)
		mutable uint32 DataNum = 0;
	
	TResourceArray<GrassUtils::FPackedGrassData> GrassData = nullptr;

public:
	explicit UGrassMeshSection(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	bool AddGrassData(GrassUtils::FPackedGrassData& Data);
	void Empty();
	
	TResourceArray<GrassUtils::FPackedGrassData>& GetGrassData()
	{
		return GrassData;
	}

	FBox GetBounds() const
	{
		return Bounds;
	}
	
	void SetBounds(const FBox& InBounds)
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
		bool bIsGPUCullingEnabled = true;
	
	UPROPERTY(EditAnywhere, Category = Rendering)
		bool bIsCPUCullingEnabled = true;
	
	UPROPERTY(EditAnywhere, Category = Rendering)
		uint32 Divisions = 3;

	UPROPERTY(EditAnywhere, Category = Rendering)
		AActor* Terrain = nullptr;

	UPROPERTY(EditAnywhere, Category = Rendering)
		float CutoffDistance = 1000.0f;

	UPROPERTY(EditAnywhere, Category = Rendering)
		float Density = 1.0f;

	UPROPERTY(EditAnywhere, Category = Rendering)
		float MaxHeight = 12;

	UPROPERTY(EditAnywhere, Category = Rendering)
		float MinHeight = 7;
	
	UPROPERTY(EditAnywhere, Category = Rendering)
		float MaxWidth = .4;

	UPROPERTY(EditAnywhere, Category = Rendering)
		float MinWidth = .3;

	UPROPERTY(EditAnywhere, Category = Rendering)
		FUintVector2 LodStepsRange = FUintVector2(0, 6);

	UPROPERTY(VisibleAnywhere, Category = Rendering)
		uint32 TotalBladesCount = 0;
	
	UPROPERTY(EditAnywhere, Category = Rendering)
		TArray<UGrassMeshSection *> Sections;
public:

	UFUNCTION(CallInEditor, Category = Rendering)
		void SampleGrassData();


	UFUNCTION(CallInEditor, Category = Rendering)
		void EmptyGrassData();

	
	UFUNCTION(CallInEditor, Category = Rendering)
		void UpdateRenderThread();


	UFUNCTION(CallInEditor, Category = Rendering)
		void InitSections();

	
	UMaterialInterface* GetMaterial() const { return Material; }

	float GetCutoffDistance() const { return CutoffDistance; }
	
	bool IsGPUCullingEnabled() const { return bIsGPUCullingEnabled; }
	bool IsCPUCullingEnabled() const { return bIsCPUCullingEnabled; }
	
	FUintVector2 GetLodStepsRange() const { return LodStepsRange; }
	TArray<UGrassMeshSection *>& GetMeshSections() { return Sections; }

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

};

