// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
#include "VectorTypes.h"

#ifndef USE_INSTANCING
	#define USE_INSTANCING 0
#endif // USE_INSTANCING


namespace GrassMesh {
	struct FGrassData;
	struct FPackedGrassData;
	struct FLodGrassData;
	struct FPackedLodGrassData;

	template <typename OutType, typename InType>
	inline OutType convert(InType value)
	{
		union converter
		{
			InType in;
			OutType out;
		};
		converter conv{};
		conv.in = value;

		return conv.out;
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
		FVector2f Facing;
		// float WindStrength;
		int Hash;

		uint32 HeightAndWidth;
		// uint32 TiltAndBand;
	};

	struct COMPUTESHADERS_API FGrassInstance
	{
		FVector3f Position;
		float Offset;
	};

	// TODO: Implementare packig e unpacking per i vertici
	struct COMPUTESHADERS_API FGrassVertex
	{
		FVector3f Position;
		FVector2f UV;
		FVector3f Normal;
	};


	struct COMPUTESHADERS_API FPackedGrassVertex
	{
		FVector3f Position;
		uint32 UV;
	};

}