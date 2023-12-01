// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
#include "VectorTypes.h"


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
		float Height;
		float Offset;

		FGrassData(FVector3f Position, float Height, float Offset);
		FGrassData(FPackedGrassData& InData);
	};

	struct COMPUTESHADERS_API FPackedGrassData
	{
		FVector3f Position;
		uint32 HeightAndOffset;

		FPackedGrassData(FVector3f Position, float Height, float Offset);
		FPackedGrassData(FGrassData& InData);
	};


	struct COMPUTESHADERS_API FLodGrassData
	{
		FVector3f Position;
		uint32 Lod;
		float Height;
		float Offset;

		FLodGrassData(FPackedLodGrassData& InData);
		FLodGrassData(FVector3f Position, uint32 Lod, float Height, float Offset);
	};

	struct COMPUTESHADERS_API FPackedLodGrassData
	{
		FVector3f Position;
		uint32 Lod;
		uint32 HeightAndOffset;

		FPackedLodGrassData(FVector3f Position, uint32 Lod, float Height, float Offset);
		FPackedLodGrassData(FLodGrassData& InData);
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
		uint32 uv;
	};


	//// FGrassData
	//inline FGrassData Unpack(FPackedGrassData InData)
	//{
	//	FGrassData OutData;
	//	OutData.Position = InData.Position;
	//	OutData.Height = convert<float>((InData.HeightAndOffset & 0xffff0000) >> 1);
	//	OutData.Offset = convert<float>((InData.HeightAndOffset & 0x0000ffff) << 14);

	//	return OutData;
	//}


	//inline FPackedGrassData Pack(FVector3f Position, float Height, float Offset)
	//{
	//	FPackedGrassData OutData;
	//	// prendo i 4 bit medno significativi dall'esponente e 12 dalla mantissa
	//	// 0    7    f    f    f    8    0    0
	//	// 0000 0111 1111 1111 1111 1000 0000 0000
	//	OutData.HeightAndOffset = (convert<uint32>(Height) & 0x7fff8000) << 1;

	//	// prendo i 3 bit medno significativi dall'esponente e 13 dalla mantissa    
	//	// 0    3    f    f    f    a    0    0
	//	// 0000 0011 1111 1111 1111 1100 0000 0000
	//	OutData.HeightAndOffset |= (convert<uint32>(Offset) & 0x3fffa000) >> 14;

	//	OutData.Position = Position;

	//	return OutData;
	//}

	//inline FPackedGrassData Pack(FGrassData InData)
	//{
	//	return Pack(InData.Position, InData.Height, InData.Offset);
	//}


	//// FLodGrassData
	//inline FLodGrassData Unpack(FPackedLodGrassData InData)
	//{
	//	FLodGrassData OutData;
	//	OutData.Position = InData.Position;
	//	OutData.Lod = InData.Lod;
	//	OutData.Height = convert<float>((InData.HeightAndOffset & 0xffff0000) >> 1);
	//	OutData.Offset = convert<float>((InData.HeightAndOffset & 0x0000ffff) << 14);

	//	return OutData;
	//}


	//inline FPackedLodGrassData Pack(FVector3f Position, uint32 Lod, float Height, float Offset)
	//{
	//	FPackedLodGrassData OutData;
	//	// prendo i 4 bit medno significativi dall'esponente e 12 dalla mantissa
	//	// 0    7    f    f    f    8    0    0
	//	// 0000 0111 1111 1111 1111 1000 0000 0000
	//	OutData.HeightAndOffset = (convert<uint32>(Height) & 0x7fff8000) << 1;

	//	// prendo i 3 bit medno significativi dall'esponente e 13 dalla mantissa    
	//	// 0    3    f    f    f    a    0    0
	//	// 0000 0011 1111 1111 1111 1100 0000 0000
	//	OutData.HeightAndOffset |= (convert<uint32>(Offset) & 0x3fffa000) >> 14;

	//	OutData.Position = Position;
	//	OutData.Lod = Lod;

	//	return OutData;
	//}

	//inline FPackedLodGrassData Pack(FLodGrassData InData)
	//{
	//	return Pack(InData.Position, InData.Lod, InData.Height, InData.Offset);
	//}
}
