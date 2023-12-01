#include "GrassChunk.h"

GrassChunk::GrassChunk(TArray<GrassMesh::FPackedGrassData> data, FBox bounds, uint32 id)
	: data(data), bounds(bounds), id(id)
{}

GrassChunk::GrassChunk(FBox bounds, uint32 id)
	: GrassChunk(TArray<GrassMesh::FPackedGrassData>(), bounds, id)
{}

GrassChunk::GrassChunk()
	: GrassChunk(TArray<GrassMesh::FPackedGrassData>(), FBox(), 0)
{}

GrassChunk::~GrassChunk() 
{}

bool GrassChunk::AddGrassData(FVector point, float height)
{
	bool result = FMath::PointBoxIntersection(point, bounds);

	if (result) 
	{
		
		data.Add(GrassMesh::FPackedGrassData(FVector3f(point), height, 0));
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

	//FGrassDrawCallParams* params = new FGrassDrawCallParams();
	//params->CameraPosition = cameraPosition;
	//params->VP = FMatrix44f(VP);
	//params->Distance = cutoffDistance;
	//params->GrassDataBuffer = data;
	//params->Lambda = lambda;
	
	//FDispatchGrassDrawCall dispatcer;
	//dispatcer.Dispatch(*params, [this, grassMesh, params](TArray<FVector>& Points, TArray<int32>& Triangles)
	//	{
	//		// Chiamata bloccante al MainThread
	//		grassMesh->ClearMeshSection(id);
	//		grassMesh->CreateMeshSection(id, Points, Triangles,
	//			TArray<FVector>(), TArray<FVector2D>(), TArray<FColor>(), TArray<FProcMeshTangent>(), true);
	//		delete& Points;
	//		delete& Triangles;
	//		delete params;
	//	});

}