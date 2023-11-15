#include "GrassChunk.h"

GrassChunk::GrassChunk(TArray<FGrassData> data, FBox bounds, uint32 id)
	: data(data), bounds(bounds), id(id)
{}

GrassChunk::GrassChunk(FBox bounds, uint32 id)
	: GrassChunk(TArray<FGrassData>(), bounds, id)
{}

GrassChunk::GrassChunk()
	: GrassChunk(TArray<FGrassData>(), FBox(), 0)
{}

GrassChunk::~GrassChunk() 
{}

bool GrassChunk::AddGrassData(FVector point, FVector2D uv)
{
	bool result = FMath::PointBoxIntersection(point, bounds);

	if (result) 
	{
		FGrassData d = FGrassData(FVector3f(point), FVector2f(uv.X, uv.Y), 0.0f);
		data.Add(d);
	}

	return result;
}

void GrassChunk::Empty()
{
	data.Empty();
}

void GrassChunk::ComputeGrass(
	float globalTime, float cutoffDistance,
	FMatrix& VP, FVector4f& cameraPosition,
	float lambda, float minHeight, float maxHeight, 
	UProceduralMeshComponent* grassMesh)
{
	if (data.Num() <= 0)
		return;

	FGPUFrustumCullingParams* params = new FGPUFrustumCullingParams();
	params->CameraPosition = cameraPosition;
	params->VP = FMatrix44f(VP);
	params->Distance = cutoffDistance;
	params->GrassDataBuffer = data;

	UE_LOG(LogTemp, Warning, TEXT("Draw Call"));
	FDispatchGrassGPUFrustumCulling dispatcer;
	dispatcer.Dispatch(*params, [this, globalTime, lambda, minHeight, maxHeight, grassMesh](TArray<FGrassData>& grassData)
		{

			GrassShaderExecutor* exec = new GrassShaderExecutor();
			exec->globalWorldTime = globalTime;
			exec->lambda = lambda;
			exec->minHeight = minHeight;
			exec->maxHeight = maxHeight;
			exec->meshComponent = grassMesh;

			exec->sectionId = id;
			exec->points = grassData;
			exec->StartBackgroundTask();

		});

}