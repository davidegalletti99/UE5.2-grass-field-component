// Fill out your copyright notice in the Description page of Project Settings.


#include "GrassFieldComponent.h"

#include "Terrain.h"
#include "Kismet/GameplayStatics.h"


UGrassMeshSection::UGrassMeshSection(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	GrassData = TResourceArray<GrassMesh::FPackedGrassData>();
}

bool UGrassMeshSection::AddGrassData(GrassMesh::FPackedGrassData& Data)
{
	const bool Result = FMath::PointBoxIntersection(FVector(Data.Position), Bounds);

	if (Result)
	{
		Data.Index = DataNum;
		GrassData.Add(Data);
		DataNum++;
	}
	return Result;
}

void UGrassMeshSection::Empty()
{
	GrassData.Empty();
	DataNum = 0;
}

UGrassFieldComponent::UGrassFieldComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	CastShadow = true;
	bCastContactShadow = false;
	bUseAsOccluder = true;
	bAffectDynamicIndirectLighting = false;
	bAffectDistanceFieldLighting = false;
	bNeverDistanceCull = true;
#if WITH_EDITORONLY_DATA
	bEnableAutoLODGeneration = false;
#endif
	Mobility = EComponentMobility::Static;
	Sections = TArray<UGrassMeshSection *>();
}

void UGrassFieldComponent::OnRegister()
{
	Super::OnRegister();
}

void UGrassFieldComponent::OnUnregister()
{
	Super::OnUnregister();
}

void UGrassFieldComponent::ApplyWorldOffset(const FVector& InOffset, bool bWorldShift)
{
	Super::ApplyWorldOffset(InOffset, bWorldShift);
	MarkRenderStateDirty();
}

bool UGrassFieldComponent::IsVisible() const
{
	return Super::IsVisible();
}

FBoxSphereBounds UGrassFieldComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	if (Terrain == nullptr)
	{
		return FBoxSphereBounds(FVector(0, 0, 0), FVector(0, 0, 0), 10000);
	}

	UProceduralMeshComponent* SurfaceMesh = Terrain->GetComponentByClass<UProceduralMeshComponent>();
	
	return SurfaceMesh->GetLocalBounds().TransformBy(Terrain->GetTransform());
}

FPrimitiveSceneProxy* UGrassFieldComponent::CreateSceneProxy()
{
	FGrassInstancingSceneProxy* Proxy = new FGrassInstancingSceneProxy(this);
	return Proxy;
}

void UGrassFieldComponent::SetMaterial(int32 InElementIndex, UMaterialInterface* InMaterial)
{
	if (InElementIndex == 0 && Material != InMaterial)
	{
		Material = InMaterial;
		MarkRenderStateDirty();
	}
}

void UGrassFieldComponent::GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials, bool bGetDebugMaterials) const
{
	if (Material != nullptr)
	{
		OutMaterials.Add(Material);
	}
}

void UGrassFieldComponent::EmptyGrassData()
{
	for(const auto& Section : Sections)
		Section->Empty();
	
	MarkRenderStateDirty();
}

void UGrassFieldComponent::UpdateRenderThread()
{
	
	MarkRenderStateDirty();
}

void UGrassFieldComponent::InitSections()
{
	if (Terrain == nullptr)
		return;

	Sections.Empty();
	const FBox LocalBounds = Bounds.GetBox();

	const FVector BoundsSize = LocalBounds.GetSize();
	const FVector Center = LocalBounds.GetCenter();
	const FVector Extent = LocalBounds.GetExtent();

	DrawDebugBox(GetWorld(), Center, Extent, FColor::Red, false, 10, 0, 10);
	const double StepX = BoundsSize.X / Dimension;
	const double StepY = BoundsSize.Y / Dimension;

	const double WorldOffX = Center.X - Extent.X;
	const double WorldOffY = Center.Y - Extent.Y;

	FVector PMin, PMax;
	PMin.Z = Center.Z - Extent.Z;
	PMax.Z = Center.Z + Extent.Z;

	for (uint32 i = 0; i < Dimension; i++)
	{
		PMin.X = StepX * i + WorldOffX;
		PMax.X = StepX * (i + 1) + WorldOffX;
		for (uint32 j = 0; j < Dimension; j++)
		{
			PMin.Y = StepY * j + WorldOffY;
			PMax.Y = StepY * (j + 1) + WorldOffY;

			UGrassMeshSection* Section = NewObject<UGrassMeshSection>();
			FBox Box = FBox(PMin, PMax);
			DrawDebugBox(GetWorld(), Box.GetCenter(), Box.GetExtent(), FColor::Red, false, 15, 0, 10);
			Section->SetBounds(Box);
			Sections.Add(Section);
		}
	}

}

void UGrassFieldComponent::SampleGrassData()
{
	if (Terrain == nullptr)
		return;
	
	for (const auto& Section : Sections)
		Section->Empty();

	TArray<AActor*> IgnoredActors;
	UGameplayStatics::GetAllActorsWithTag(GetWorld(), TEXT("Scene"), IgnoredActors);
	FCollisionQueryParams Params = FCollisionQueryParams(Terrain->Tags[0]);
	Params.AddIgnoredActors(IgnoredActors);

	FBox Box = Bounds.GetBox();
	TArray<FVector> Points = TArray<FVector>();

	GrassUtils::PoissonSampling(Box, 2 / Density, Points);
	
	float MaxZ = Box.GetCenter().Z + Box.GetExtent().Z;
	float MinZ = Box.GetCenter().Z - Box.GetExtent().Z;
	
	UProceduralMeshComponent* SurfaceMesh = Terrain->GetComponentByClass<UProceduralMeshComponent>();
	for (auto Point : Points)
	{
		Point += Box.GetCenter() - Box.GetExtent();
		
		FVector Start = Point;
		Start.Z = MaxZ;
		
		FVector End = Point;
		End.Z = MinZ;

		
		if (FHitResult Hit;
			GetWorld()->LineTraceSingleByProfile(Hit, Start, End, SurfaceMesh->GetCollisionProfileName(), Params)
			&& Hit.GetActor() == Terrain)
		{
			FVector Up = Hit.ImpactNormal;
			FVector Position = Hit.ImpactPoint;
			
			GrassMesh::FPackedGrassData Data = GrassUtils::ComputeData(Position, Up, MinHeight, MaxHeight, MinWidth, MaxWidth);
			DrawDebugLine(GetWorld(), Position, Position + Up * 12, FColor::Red, false, 20, 0, 1);

			// GrassMesh::FGrassData GrassData = GrassMesh::FGrassData(Data);
			// DrawDebugLine(GetWorld(), Position, Position + static_cast<FVector>(GrassData.Facing) * 5, FColor::Green, false, 20, 0, 1);
			for (const auto& Section : Sections)
			{
				if (Section->AddGrassData(Data))
					break;
			}
		}
		
	}
}
