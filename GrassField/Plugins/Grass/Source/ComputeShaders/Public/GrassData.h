// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "VectorTypes.h"

#ifndef My_USE_INSTANCING
	#define My_USE_INSTANCING 0
#endif // USE_INSTANCING

namespace GrassMesh {
	struct FGrassData;
	struct FPackedGrassData;
	struct FLodGrassData;
	struct FPackedLodGrassData;

	template <typename OutType, typename InType>
	inline OutType convert(InType Value)
	{
		union FConverter
		{
			InType In;
			OutType Out;
		};
		FConverter Conv{};
		Conv.In = Value;

		return Conv.Out;
	}
	
	struct COMPUTESHADERS_API FGrassData
	{
		FVector3f Position;
		FVector2f Facing;
		// float WindStrength;
		int Hash;

		float Height;
		float Width;
		// float Tilt;
		// float Bend;

		FGrassData(FVector3f Position, FVector2f Facing, int Hash, float Height, float Width);
		FGrassData(FPackedGrassData& InData);
	};
	
	struct COMPUTESHADERS_API FPackedGrassData
	{
		FVector3f Position;
		uint32 Facing;
		// float WindStrength;

		int Hash;

		uint32 HeightAndWidth;
		// uint32 TiltAndBand;

		FPackedGrassData(FVector3f Position, FVector2f Facing, int Hash, float Height, float Width);
		FPackedGrassData(FGrassData& InData);
	};


	struct COMPUTESHADERS_API FLodGrassData
	{
		FVector3f Position;
		FVector2f Facing;
		// float WindStrength;
		int Hash;

		uint32 Lod;

		float Height;
		float Width;
		// float Tilt;
		// float Bend;
	};

	struct COMPUTESHADERS_API FPackedLodGrassData
	{
		FVector3f Position;
		uint32 Facing;
		// float WindStrength;
		int Hash;
		
		uint32 Lod;

		uint32 HeightAndWidth;
		// uint32 TiltAndBand;
	};

	struct COMPUTESHADERS_API FGrassInstance
	{
		float Transform[4][4];
		int Hash;
		
	};

	// TODO: Implementare packig e unpacking per i vertici
	struct COMPUTESHADERS_API FGrassVertex
	{
		FVector3f Position;
		FVector2f UV;
		FVector3f TangentX;
		FVector4f TangentZ; // TangentZ.w contains sign of tangent basis determinant
		// FPackedNormal TangentX;
		// FPackedNormal TangentZ;
	};

	// struct COMPUTESHADERS_API FPackedGrassVertex
	// {
	// 	FVector3f Position;
	// 	uint32 UV;
	// };

	inline float ComputeLodIndex(
		const FSceneView* View,
		const FBox& Bounds,
		const float CutoffDistance,
		const FUintVector2 MinMaxLod)
	{
		float Distance = FVector::DistXY(View->CullingOrigin, Bounds.GetCenter());
		Distance -= FVector::DistXY(FVector::Zero(), Bounds.GetExtent());
		
		const float LodPercentage = (Distance / CutoffDistance);
		const uint32 Lod = FMath::Clamp(FMath::Lerp(MinMaxLod.X, MinMaxLod.Y, LodPercentage), MinMaxLod.X, MinMaxLod.Y);
		return Lod;
	}
	
#define GRAVITATIONAL_ACCELERATION 9.81f

	inline FVector3f MovePoint(const FVector3f Pi, const FVector3f Vi, const float T)
	{
		FVector3f P = Pi + Vi * T;
		P.Z -= T * T * GRAVITATIONAL_ACCELERATION / 2;
		return P;
	}

#define QUAD_BEZ(P0, P1, P2, T) (P0 * (1 - T) * (1 - T) + P1 * 2 * (1 - T) * T + P2 * T * T)
	inline void CreateGrassModels(
	    TResourceArray<FGrassVertex>& VertexBuffer,
	    TResourceArray<uint32>& IndexBuffer,
	    uint32 LodStep)
	{
	    const float MaxHeight = 1.0f;
	    const float MaxWidth = 1.0f;
	    
		const FVector3f V0 = FVector3f(0, 0, FMath::Sqrt(2 * GRAVITATIONAL_ACCELERATION * MaxHeight));
		const float Te = V0.Z / GRAVITATIONAL_ACCELERATION;
		
		const FVector3f InitialPosition = FVector3f(0, 0, 0);
	    const FVector3f FinalPosition = MovePoint(InitialPosition, V0, V0.Z / GRAVITATIONAL_ACCELERATION);
	    const FVector2f Facing = FVector2f(0, 1);
		
	    constexpr float Angle = UE_PI * 0.1;
	    const FVector3f Normal = FVector3f(Facing, 0);
	    const FVector3f Tangent = Normal.Cross(FVector3f(0, 0, 1));
		
		const FVector3f Normal1 = Normal.RotateAngleAxis(Angle, FVector3f(0, 0, 1));
		const FVector3f Normal2 = Normal.RotateAngleAxis(-Angle, FVector3f(0, 0, 1));
		
		const FVector3f Tangent1 = Tangent.RotateAngleAxis(Angle, FVector3f(0, 0, 1));
		const FVector3f Tangent2 = Tangent.RotateAngleAxis(-Angle, FVector3f(0, 0, 1));
	    
		VertexBuffer.AddZeroed(LodStep * 2 + 3);
		IndexBuffer.AddZeroed((LodStep * 2 + 1) * 3);
		
	    uint32 VertexIndex = 0, PrimitiveIndex = 0;
	    for (uint32 i = 0; i <= LodStep; i++, VertexIndex += 2, PrimitiveIndex += 6)
	    {

	        const float Percentage = static_cast<float>(i) / (LodStep + 1);
	        const float CurrentFactor = QUAD_BEZ(1, 1 * 0.85, 0, Percentage);
	        const FVector3f CurrentPosition = MovePoint(InitialPosition, V0, Percentage * Te);
	        
	        const float CurrentHalfWidth = CurrentFactor * MaxWidth / 2;
	        
	        FGrassVertex P1, P2;
	        P1.Position = CurrentPosition - Tangent * CurrentHalfWidth;
	        P1.UV = FVector2f(0.5 - CurrentFactor, Percentage);
	        P1.TangentX = Tangent1;
	        P1.TangentZ = FVector4f(Normal1, 1);
	    	VertexBuffer[VertexIndex + 0] = P1;

	        P2.Position = CurrentPosition + Tangent * CurrentHalfWidth;
	        P2.UV = FVector2f(0.5 + CurrentFactor, Percentage);
	        P2.TangentX = Tangent2;
	        P2.TangentZ = FVector4f(Normal2, 1);
	        VertexBuffer[VertexIndex + 1] = P2;

	        
	        if (i < LodStep)
	        {
	            // t1
	            IndexBuffer[PrimitiveIndex + 0] = VertexIndex + 0;
	        	IndexBuffer[PrimitiveIndex + 1] = VertexIndex + 3;
	        	IndexBuffer[PrimitiveIndex + 2] = VertexIndex + 1;
	        	
	        	// t2
	        	IndexBuffer[PrimitiveIndex + 3] = VertexIndex + 0;
	        	IndexBuffer[PrimitiveIndex + 4] = VertexIndex + 2;
	        	IndexBuffer[PrimitiveIndex + 5] = VertexIndex + 3;
	        }
	        else
	        {
	        	// last triangle
	        	IndexBuffer[PrimitiveIndex + 0] = VertexIndex + 0;
	        	IndexBuffer[PrimitiveIndex + 1] = VertexIndex + 2;
	        	IndexBuffer[PrimitiveIndex + 2] = VertexIndex + 1;
	        	
	        }
	    }
	    
	    // last point
	    FGrassVertex Pe;
	    Pe.Position = FinalPosition;
	    Pe.UV = FVector2f(0.5, 1);
	    Pe.TangentX = Tangent;
	    Pe.TangentZ = FVector4f(Normal, 1);
	    VertexBuffer[VertexIndex + 0] = Pe;
	}
}