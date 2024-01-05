#pragma once
#include "GrassData.h"

namespace GrassUtils
{
	static GrassMesh::FPackedGrassData ComputeData(const FVector& Position, const FVector& Up, const float MinHeight, const float MaxHeight, const float MinWidth, const float MaxWidth)
	{
		FVector V = FMath::VRand();
		while (V.Dot(Up) >= .95f)
			V = FMath::VRand();
		
		const FVector3f Facing = FVector3f(V - Up * FVector::DotProduct(Up, V)).GetSafeNormal();
		
		const float Extraction = FMath::SRand();
		const float Height = Extraction * (MaxHeight - MinHeight) + MinHeight;
		const float Width = Extraction * (MaxWidth - MinWidth) + MinWidth;
		const float Stiffness = FMath::SRand();
		return GrassMesh::FPackedGrassData
		{
			0,
			static_cast<FVector3f>(Position),
			static_cast<FVector3f>(Up),
			Facing,
			Height,
			Width,
			Stiffness
		};
	}

	static bool IsPointValid(const FVector& Point, const FVector& BoundsSize, const float CellSize,  TArray<FVector>& Points, int** Grid)
	{
		if (Point.X >= 0 && Point.X < BoundsSize.X && Point.Y >= 0 && Point.Y < BoundsSize.Y)
		{
			const int CellX = FMath::FloorToInt(Point.X / CellSize);
			const int CellY = FMath::FloorToInt(Point.Y / CellSize);

			const int SearchStartX = FMath::Max(CellX - 2, 0);
			const int SearchEndX = FMath::Min(CellX + 2, FMath::CeilToInt(BoundsSize.X / CellSize) - 1);
			
			const int SearchStartY = FMath::Max(CellY - 2, 0);
			const int SearchEndY = FMath::Min(CellY + 2, FMath::CeilToInt(BoundsSize.Y / CellSize) - 1);

			for (int i = SearchStartX; i <= SearchEndX; i++)
			{
				for (int j = SearchStartY; j <= SearchEndY; j++)
				{
					const int Index = Grid[i][j] - 1;
					if (Index != -1)
					{
						float Distance = FVector::DistSquared(Point, Points[Index]);
						if (Distance < 2 * CellSize * CellSize)
							return false;
					}
				}
			}
			return true;
		}
		return false;
	}

	static constexpr int NumSamplesBeforeRejection = 5;
	static void PoissonSampling(const FBox& Bounds, const float Radius, TArray<FVector>& OutSamples)
	{
		OutSamples.Empty();
		FVector BoundsSize = Bounds.GetSize();
		float CellSize = Radius / FMath::Sqrt(2.0f);

		int GridXSize = FMath::CeilToInt(BoundsSize.X / CellSize);
		int GridYSize = FMath::CeilToInt(BoundsSize.Y / CellSize);
		
		int** Grid = new int*[GridXSize];
		for (int i = 0; i < GridXSize; i++)
		{
			Grid[i] = new int[GridYSize];
			for (int j = 0; j < GridYSize; j++)
				Grid[i][j] = 0;
		}
		TArray<FVector> SpawnPoints = TArray<FVector>();

		SpawnPoints.Add(BoundsSize / 2);
		while (SpawnPoints.Num() > 0)
		{
			int Index = FMath::RandRange(0, SpawnPoints.Num() - 1);
			FVector Point = SpawnPoints[Index];
			bool CandidateAccepted = false;

			for (int i = 0; i < NumSamplesBeforeRejection; i++)
			{
				float Angle = FMath::FRand() * 2 * UE_PI;
				FVector Dir = FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0);
				float Distance = FMath::FRand() * Radius + Radius;
				FVector NewPoint = Point + Dir * Distance;

				if (IsPointValid(NewPoint, BoundsSize, CellSize, OutSamples, Grid))
				{
					OutSamples.Add(NewPoint);
					SpawnPoints.Add(NewPoint);
					Grid[FMath::FloorToInt(NewPoint.X / CellSize)][FMath::FloorToInt(NewPoint.Y / CellSize)] = OutSamples.Num();
					CandidateAccepted = true;
					break;
				}
			}
			if (!CandidateAccepted)
				SpawnPoints.RemoveAt(Index);
		}

		delete Grid;
	}
	
};
