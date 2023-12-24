// Fill out your copyright notice in the Description page of Project Settings.


#include "GrassFieldComponent.h"

#include "GrassInstancingSceneProxy.h"
// #include "GrassSceneProxy.h"

UGrassMeshSection::UGrassMeshSection(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	GrassData = TResourceArray<GrassMesh::FPackedGrassData>();
}

bool UGrassMeshSection::AddGrassData(const GrassMesh::FPackedGrassData& Data)
{
	const bool Result = FMath::PointBoxIntersection(FVector(Data.Position), Bounds);

	if (Result)
	{
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
	return SurfaceMesh->GetLocalBounds().TransformBy(LocalToWorld);
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

	UProceduralMeshComponent* SurfaceMesh = Terrain->GetComponentByClass<UProceduralMeshComponent>();
	FProcMeshSection* MeshSection = SurfaceMesh->GetProcMeshSection(0);
	for (auto& Vertex : MeshSection->ProcVertexBuffer)
	{
		FVector p = Vertex.Position + SurfaceMesh->GetActorPositionForRenderer();
		for (float i = 0; i < Density; i++)
		{
			FVector Offset = (FMath::VRand() - 0.5) * Displacement;
			
			FVector Start = p + FVector(Offset.X, Offset.Y, 10);
			FVector End = p + FVector(Offset.X, Offset.Y, -10);

			if (FHitResult Hit; GetWorld()->LineTraceSingleByChannel(Hit, Start, End, SurfaceMesh->GetCollisionObjectType()))
			{
				if (FMath::Abs(Vertex.Normal.Dot(FVector(0, 0, 1))) <= 0.8)
					continue;
				
				FVector2f Facing = FVector2f(FVector3f(FMath::VRand() - 0.5));
				Facing.Normalize();
				const float Extraction = FMath::SRand();
				const float Height = Extraction * (MaxHeight - MinHeight) + MinHeight;
				const float Width = Extraction * (MaxWidth - MinWidth) + MinWidth;
				
				GrassMesh::FPackedGrassData GrassPoint
				{
					FVector3f(Hit.ImpactPoint),
					FVector2f(Facing.X, Facing.Y),
					0,
					Height,
					Width
				};
				
				for (auto& Section : Sections)
					if(Section->AddGrassData(GrassPoint))
						break;
			}
		}
	}
}
