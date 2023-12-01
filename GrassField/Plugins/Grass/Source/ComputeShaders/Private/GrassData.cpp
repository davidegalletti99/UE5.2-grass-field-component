#include  "GrassData.h"

namespace GrassMesh
{

	// FGrassData
	FGrassData::FGrassData(FVector3f Position, float Height, float Offset)
		: Position(Position), Height(Height), Offset(Offset)
	{
	}

	FGrassData::FGrassData(FPackedGrassData& InData)
	{
		this->Position = InData.Position;
		this->Height = convert<float>((InData.HeightAndOffset & 0xffff0000) >> 1);
		this->Offset = convert<float>((InData.HeightAndOffset & 0x0000ffff) << 14);
	}

	FPackedGrassData::FPackedGrassData(FVector3f Position, float Height, float Offset)
	{
		// prendo i 4 bit medno significativi dall'esponente e 12 dalla mantissa
		// 7    f    f    f    8    0    0    0
		// 0111 1111 1111 1111 1000 1000 0000 0000
		this->HeightAndOffset = (convert<uint32>(Height) & 0x7fff8000) << 1;

		// prendo i 3 bit medno significativi dall'esponente e 13 dalla mantissa    
		// 3    f    f    f    a    0    0    0
		// 0011 1111 1111 1111 1100 0000 0000 0000
		this->HeightAndOffset |= (convert<uint32>(Offset) & 0x3fffa000) >> 14;

		this->Position = Position;
	}

	FPackedGrassData::FPackedGrassData(FGrassData& InData)
		: FPackedGrassData(InData.Position, InData.Height, InData.Offset)
	{
	}


	// FLodGrassData
	FLodGrassData::FLodGrassData(FVector3f Position, uint32 Lod, float Height, float Offset)
		: Position(Position), Lod(Lod), Height(Height), Offset(Offset)
	{
	}

	FLodGrassData::FLodGrassData(FPackedLodGrassData& InData)
	{
		this->Position = InData.Position;
		this->Lod = InData.Lod;
		this->Height = convert<float>((InData.HeightAndOffset & 0xffff0000) >> 1);
		this->Offset = convert<float>((InData.HeightAndOffset & 0x0000ffff) << 14);

	}



	FPackedLodGrassData::FPackedLodGrassData(FVector3f Position, uint32 Lod, float Height, float Offset)
	{
		// prendo i 4 bit medno significativi dall'esponente e 12 dalla mantissa
		// 0    7    f    f    f    8    0    0
		// 0000 0111 1111 1111 1111 1000 0000 0000
		this->HeightAndOffset = (convert<uint32>(Height) & 0x7fff8000) << 1;

		// prendo i 3 bit medno significativi dall'esponente e 13 dalla mantissa    
		// 0    3    f    f    f    a    0    0
		// 0000 0011 1111 1111 1111 1100 0000 0000
		this->HeightAndOffset |= (convert<uint32>(Offset) & 0x3fffa000) >> 14;

		this->Position = Position;
		this->Lod = Lod;
	}

	FPackedLodGrassData::FPackedLodGrassData(FLodGrassData& InData)
		: FPackedLodGrassData(InData.Position, InData.Lod, InData.Height, InData.Offset)
	{}

}
