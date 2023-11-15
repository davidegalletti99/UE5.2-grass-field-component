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

bool GrassChunk::AddGrassData(FVector point, float height)
{
	bool result = FMath::PointBoxIntersection(point, bounds);

	if (result) 
	{
		FGrassData d = FGrassData(FVector3f(point), height);
		data.Add(d);
	}

	return result;
}

void GrassChunk::Empty()
{
	data.Empty();
}

void GrassChunk::ComputeGrass(
	float cutoffDistance,
	FMatrix& VP, FVector4f& cameraPosition,
	float lambda, UProceduralMeshComponent* grassMesh)
{
	if (data.Num() <= 0)
		return;

	FGrassDrawCallParams* params = new FGrassDrawCallParams();
	params->CameraPosition = cameraPosition;
	params->VP = FMatrix44f(VP);
	params->Distance = cutoffDistance;
	params->GrassDataBuffer = data;
	params->Lambda = lambda;
	
	FDispatchGrassDrawCall dispatcer;
	dispatcer.Dispatch(*params, [this, grassMesh, params](TArray<FVector>& Points, TArray<int32>& Triangles)
		{
			// Chiamata bloccante al MainThread
			grassMesh->ClearMeshSection(id);
			grassMesh->CreateMeshSection(id, Points, Triangles,
				TArray<FVector>(), TArray<FVector2D>(), TArray<FColor>(), TArray<FProcMeshTangent>(), true);
			delete& Points;
			delete& Triangles;
			delete params;
		});

}