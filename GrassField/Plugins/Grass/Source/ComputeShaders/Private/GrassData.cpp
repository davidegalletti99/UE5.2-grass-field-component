#include  "GrassData.h"

namespace GrassMesh
{

	// FGrassData
	FGrassData::FGrassData(FVector3f Position, FVector2f Facing, int Hash, float Height, float Width)
		: Position(Position), Facing(Facing), Height(Height), Width(Width)
	{
	}

	FGrassData::FGrassData(FPackedGrassData& InData)
	{
		this->Height = convert<float>((InData.HeightAndWidth & 0xffff0000) >> 1);
		this->Width = convert<float>((InData.HeightAndWidth & 0x0000ffff) << 15);

		uint32 SignX = InData.Facing & 0x80000000;
		uint32 SignY = (InData.Facing & 0x00008000) << 16;
		this->Facing.X = convert<float>(SignX | (InData.Facing & 0x7fff0000) >> 1);
		this->Facing.Y = convert<float>(SignY | (InData.Facing & 0x00007fff) << 15);

		this->Position = InData.Position;
		this->Hash = InData.Hash;
	}

	FPackedGrassData::FPackedGrassData(FVector3f Position, FVector2f Facing, int Hash, float Height, float Width)
	{
		// prendo i 4 bit medno significativi dall'esponente e 12 dalla mantissa
		// 7    f    f    f    8    0    0    0
		// 0111 1111 1111 1111 1000 0000 0000 0000
		this->HeightAndWidth = (convert<uint32>(Height) & 0x7fff8000) << 1;
		this->HeightAndWidth |= (convert<uint32>(Width) & 0x7fff8000) >> 15;

		// prendo i 3 bit medno significativi dall'esponente e 11 dalla mantissa + il segno
		// 0    3    f    f    f    8    0    0
		// 0000 0011 1111 1111 1111 1000 0000 0000
		uint32 Signes = (convert<uint32>(Facing.X) & 0x80000000);
		Signes |= (convert<uint32>(Facing.Y) & 0x80000000) >> 16;

		this->Facing = (convert<uint32>(Facing.X) & 0x3fff8000) << 1;
		this->Facing |= (convert<uint32>(Facing.Y) & 0x3fff8000) >> 15;
		this->Facing |= Signes;

		this->Position = Position;
		this->Hash = Hash;
	}

	FPackedGrassData::FPackedGrassData(FGrassData& InData)
		: FPackedGrassData(InData.Position, InData.Facing, InData.Hash, InData.Height, InData.Width)
	{
	}
}
