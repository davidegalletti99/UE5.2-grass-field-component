// Fill out your copyright notice in the Description page of Project Settings.


#include "GrassFieldComponent.h"

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

void UGrassFieldComponent::SampleGrassData()
{
	GrassData->Empty();
	FProcMeshSection* meshSection = SurfaceMesh->GetProcMeshSection(0);
	for (FProcMeshVertex vertex : meshSection->ProcVertexBuffer)
	{
		FVector p = vertex.Position + SurfaceMesh->GetActorPositionForRenderer();
		for (float i = 0; i / density < 1.0f; i++)
		{
			float offsetX = (FMath::SRand() - 0.5) * displacement;
			float offsetY = (FMath::SRand() - 0.5) * displacement;
			FVector start = p;
			start.X += offsetX;
			start.Y += offsetY;
			start.Z += 10;

			FVector end = p;
			end.X += offsetX;
			end.Y += offsetY;
			end.Z -= 10;

			FHitResult hit;

			if (GetWorld()->LineTraceSingleByChannel(hit, start, end, SurfaceMesh->GetCollisionObjectType()))
			{
				float height = FMath::Lerp(minHeight, maxHeight, FMath::SRand());

				GrassData->Add(GrassMesh::FPackedGrassData(FVector3f(hit.ImpactPoint), height, 0.1f));
			}
		}
	}
	MarkRenderStateDirty();
}
