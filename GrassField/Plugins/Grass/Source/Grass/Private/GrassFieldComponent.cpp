// Fill out your copyright notice in the Description page of Project Settings.


#include "GrassFieldComponent.h"

UGrassMeshSection::UGrassMeshSection(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	GrassData = new TArray<GrassMesh::FPackedGrassData>();
}

void UGrassMeshSection::AddGrassData(GrassMesh::FPackedGrassData Data)
{
	bool result = FMath::PointBoxIntersection(FVector(Data.Position), Bounds);

	if (result)
		GrassData->Add(Data);
}

void UGrassMeshSection::Empty()
{
	GrassData->Empty();
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
	GrassData = new TResourceArray<GrassMesh::FPackedGrassData>();
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
	return SurfaceMesh->GetLocalBounds().TransformBy(LocalToWorld);
}

FPrimitiveSceneProxy* UGrassFieldComponent::CreateSceneProxy()
{
	FGrassSceneProxy* proxy = new FGrassSceneProxy(this, GrassData, LodStepsRange, lambda, cutoffDistance);
	return proxy;
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
	GrassData->Empty();
}

void UGrassFieldComponent::InitSections()
{

	if (Terrain == nullptr)
		return;

	Sections.Empty();

	UProceduralMeshComponent* SurfaceMesh = Terrain->GetComponentByClass<UProceduralMeshComponent>();
	FBox LocalBounds = SurfaceMesh->GetLocalBounds().GetBox();

	FVector BoundsSize = LocalBounds.GetSize();
	FVector Center = LocalBounds.GetCenter();

	double stepX = BoundsSize.X / Dimension;
	double stepY = BoundsSize.Y / Dimension;

	double worldOffX = Center.X - BoundsSize.X / 2;
	double worldOffY = Center.Y - BoundsSize.Y / 2;

	FVector pMin, pMax;
	pMin.Z = Center.Z - BoundsSize.Z / 2;
	pMax.Z = Center.Z + BoundsSize.Z / 2;

	for (uint32 i = 0; i < Dimension; i++)
	{
		pMin.X = stepX * i + worldOffX;
		pMax.X = stepX * (i + 1) + worldOffX;
		for (uint32 j = 0; j < Dimension; j++)
		{
			pMin.Y = stepY * j + worldOffY;
			pMax.Y = stepY * (j + 1) + worldOffY;

			UGrassMeshSection* section = NewObject<UGrassMeshSection>();
			section->SetBounds(FBox(pMin, pMax));
			Sections.Add(section);
		}
	}

}

void UGrassFieldComponent::SampleGrassData()
{
	if (Terrain == nullptr)
		return;
	GrassData->Empty();


	UProceduralMeshComponent* SurfaceMesh = Terrain->GetComponentByClass<UProceduralMeshComponent>();
	FProcMeshSection* section = SurfaceMesh->GetProcMeshSection(0);
	for (auto& vertex : section->ProcVertexBuffer)
	{
		FVector p = vertex.Position + SurfaceMesh->GetActorPositionForRenderer();
		for (float i = 0; i < density; i++)
		{
			FVector offset = (FMath::VRand() - 0.5) * displacement;
			FVector facing = (FMath::VRand() - 0.5);
			FVector start = p + FVector(offset.X, offset.Y, 10);

			FVector end = p + FVector(offset.X, offset.Y, -10);
			FHitResult hit;

			if (GetWorld()->LineTraceSingleByChannel(hit, start, end, SurfaceMesh->GetCollisionObjectType()))
			{
				float height = FMath::Lerp(minHeight, maxHeight, FMath::SRand());
				float width = FMath::Lerp(minHeight, maxHeight, FMath::SRand()) * 0.02;

				GrassData->Add(GrassMesh::FPackedGrassData(FVector3f(hit.ImpactPoint), FVector2f(FVector3f(facing)), 0, height, width));
			}
		}
	}
	MarkRenderStateDirty();
}
