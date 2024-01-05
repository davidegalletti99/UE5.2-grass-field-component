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
		uint32 Index;
		FVector3f Position;
		
		FVector3f Up;
		FVector3f Facing;

		float Height;
		float Width;
		float Stiffness;

		FGrassData(
			const uint32 Index,
			const FVector3f Position,
			const FVector3f Up, const FVector3f Facing,
			const float Height, const float Width, const float Stiffness);
		FGrassData(FPackedGrassData& InData);
	};
	
	struct COMPUTESHADERS_API FPackedGrassData
	{
		uint32 Index;
		FVector3f Position;
		
		uint32 Up;
		uint32 Facing;

		uint32 HeightAndWidth;
		float Stiffness;

		FPackedGrassData(
			const uint32 Index,
			const FVector3f Position,
			const FVector3f Up, const FVector3f Facing,
			const float Height, float Width, const float Stiffness);
		FPackedGrassData(FGrassData& InData);
	};
	

	struct COMPUTESHADERS_API FGrassInstance
	{
		float Transform[4][4];
		// FVector3f V0;
		// FVector3f V1;
		// FVector3f V2;
		//
		// FVector3f Up;
		//
		// float Width;
		// FVector3f Facing;
	};

	struct COMPUTESHADERS_API FPackedGrassInstance
	{
		FVector3f V0;
		FVector3f V1;
		FVector3f V2;
		
		uint32 Up;

		float Width;
		uint32 Facing;
	};


	struct COMPUTESHADERS_API FGrassVertex
	{
		FVector3f Position;
		FVector2f UV;
		FVector3f TangentX;
		FVector4f TangentZ; // TangentZ.w contains sign of tangent basis determinant
	};

	struct COMPUTESHADERS_API FPackedGrassVertex
	{
		FVector3f Position;
		uint32 UV;
		uint32 TangentX;
		uint32 TangentZ; // TangentZ.w contains sign of tangent basis determinant
	};

	inline uint32 PackNormal(FVector4f Normal)
	{
		FVector4f MappedNormal = Normal + FVector4f(1, 1, 1, 1);
		MappedNormal *= 255.0f / 2.0f;

		uint32 Out = 0;
		Out |= (static_cast<uint32>(MappedNormal.X) << 24);
		Out |= (static_cast<uint32>(MappedNormal.Y) << 16);
		Out |= (static_cast<uint32>(MappedNormal.Z) << 8);
		Out |= (static_cast<uint32>(MappedNormal.W) << 0);

		return Out;
	}

	inline uint32 PackNormal(FVector3f Normal)
	{
		return PackNormal(FVector4f(Normal, 1));
	}
	
	inline FVector4f UnpackNormal(uint32 Normal)
	{
		FVector4f Out;
		Out.X = static_cast<float>((Normal & 0xff000000) >> 24);
		Out.Y = static_cast<float>((Normal & 0x00ff0000) >> 16);
		Out.Z = static_cast<float>((Normal & 0x0000ff00) >> 8);
		Out.W = static_cast<float>((Normal & 0x000000ff) >> 0);

		Out *= 2.0f / 255.0f;
		Out -= FVector4f(1, 1, 1, 1);
		
		return Out;
	}

	inline uint32 PackUV(const FVector2f UV)
	{
		constexpr uint32 MaxUint16 = 65535;
		const FVector2f MappedUV = UV * MaxUint16; // [0, 1] -> [0, MaxUint16]

		uint32 Out = 0;
		Out |= ((static_cast<uint32>(MappedUV.X) & 0x0000ffff) << 16);
		Out |= ((static_cast<uint32>(MappedUV.Y) & 0x0000ffff) << 0);
	
		return Out;
	}

	inline FVector2f UnpackUV(const uint32 UV)
	{
		constexpr uint32 MaxUint16 = 65535;
		FVector2f Out;
		Out.X = static_cast<float>((UV & 0xffff0000) >> 16);
		Out.Y = static_cast<float>((UV & 0x0000ffff) >> 0);
		
		Out /= MaxUint16;
		return Out;
	}

#define GRAVITATIONAL_ACCELERATION 9.81f
#define QUAD_BEZ(P0, P1, P2, T) ( FMath::Lerp(P0, FMath::Lerp(P1, P2, T), T) )
#define CUB_BEZ(P0, P1, P2, P3, T) ( FMath::Lerp(QUAD_BEZ(P0, P1, P2, T), QUAD_BEZ(P1, P2, P3, T), T) )
	
	inline float ComputeLodIndex(
		const FVector& CullOrigin,
		const FBox& Bounds,
		const float CutoffDistance,
		const FUintVector2 MinMaxLod)
	{
		float Distance = FVector::Dist(CullOrigin, Bounds.GetCenter());
		Distance -= Bounds.GetExtent().Size();
		
		const float LodPercentage = 1 - Distance / CutoffDistance;
		const uint32 Lod = FMath::Clamp(
			CUB_BEZ(MinMaxLod.X, MinMaxLod.Y * .05, MinMaxLod.Y * .2, MinMaxLod.Y, LodPercentage),
			MinMaxLod.X, MinMaxLod.Y);
		return Lod;
	}
	
	
	inline FVector3f MovePoint(const FVector3f Pi, const FVector3f Vi, const float T)
	{
		FVector3f P = Pi + Vi * T;
		P.Z -= T * T * GRAVITATIONAL_ACCELERATION / 2;
		return P;
	}

	inline void CreateGrassModels(
	    TResourceArray<FPackedGrassVertex>& VertexBuffer,
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
	        
	        FPackedGrassVertex P1, P2;
	        P1.Position = CurrentPosition - Tangent * CurrentHalfWidth;
	        P1.UV = PackUV(FVector2f(0.5 - CurrentFactor, Percentage));
	        P1.TangentX = PackNormal(Tangent1);
	        P1.TangentZ = PackNormal(FVector4f(Normal1, 1));
	    	VertexBuffer[VertexIndex + 0] = P1;

	        P2.Position = CurrentPosition + Tangent * CurrentHalfWidth;
	        P2.UV = PackUV(FVector2f(0.5 + CurrentFactor, Percentage));
	        P2.TangentX = PackNormal(Tangent2);
	        P2.TangentZ = PackNormal(FVector4f(Normal2, 1));
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
	    FPackedGrassVertex Pe;
	    Pe.Position = FinalPosition;
	    Pe.UV = PackUV(FVector2f(0.5, 1));
	    Pe.TangentX = PackNormal(Tangent);
	    Pe.TangentZ = PackNormal(FVector4f(Normal, 1));
	    VertexBuffer[VertexIndex + 0] = Pe;
	}
}