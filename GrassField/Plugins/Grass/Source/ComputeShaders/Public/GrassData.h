// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
struct COMPUTESHADERS_API FGrassData
{
	struct float3 {
		float x, y, z;

		float3(float x, float y, float z) :
			x(x), y(y), z(z)
		{}

		float3() : float3(0.0f, 0.0f, 0.0f)
		{}
	};

	struct float2 {
		float x, y;

		float2(float x, float y) :
			x(x), y(y)
		{}

		float2() : float2(0.0f, 0.0f)
		{}
	};

	float3 position;
	float2 uv;
	float displacement;

	FGrassData(FVector3f position, FVector2f uv, float displacement) :
		position(float3(position.X, position.Y, position.Z)),
		uv(float2(uv.X, uv.Y)),
		displacement(displacement)
	{}

	FGrassData() : FGrassData(FVector3f(), FVector2f(), 0.0f)
	{}
};