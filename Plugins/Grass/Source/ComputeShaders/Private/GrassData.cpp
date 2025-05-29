#include  "GrassData.h"

#include "Dataflow/DataflowConnection.h"

namespace GrassUtils
{

	// FGrassData
	FGrassData::FGrassData(
			const uint32 Index,
			const FVector3f Position,
			const FVector3f Up, const FVector3f Facing,
			const float Height, const float Width, const float Stiffness)
		: Index(Index), Position(Position), Up(Up), Facing(Facing), Height(Height), Width(Width), Stiffness(Stiffness)
	{
	}

	FGrassData::FGrassData(FPackedGrassData& InData)
	{
		this->Height = Convert<float>((InData.HeightAndWidth & 0xffff0000) >> 1);
		this->Width = Convert<float>((InData.HeightAndWidth & 0x0000ffff) << 15);
		
		this->Facing = FVector3f(UnpackNormal(InData.Facing));
		this->Up = FVector3f(UnpackNormal(InData.Up));

		this->Position = InData.Position;
		this->Index = InData.Index;
		this->Stiffness = InData.Stiffness;
	}

	FPackedGrassData::FPackedGrassData(
		const uint32 Index,
		const FVector3f Position,
		const FVector3f Up, const FVector3f Facing,
		const float Height, float Width, const float Stiffness)
	{
		
		// prendo i 4 bit medno significativi dall'esponente e 12 dalla mantissa
		// 7    f    f    f    8    0    0    0
		// 0111 1111 1111 1111 1000 0000 0000 0000
		this->HeightAndWidth = (Convert<uint32>(Height) & 0x7fff8000) << 1;
		this->HeightAndWidth |= (Convert<uint32>(Width) & 0x7fff8000) >> 15;

		this->Facing = PackNormal(Facing);
		this->Up = PackNormal(Up);

		this->Position = Position;
		this->Index = Index;
		this->Stiffness = Stiffness;
	}

	FPackedGrassData::FPackedGrassData(FGrassData& InData)
		: FPackedGrassData(InData.Index, InData.Position, InData.Up, InData.Facing, InData.Height, InData.Width, InData.Stiffness)
	{
	}
}
