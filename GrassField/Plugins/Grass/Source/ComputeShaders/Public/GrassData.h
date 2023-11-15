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
	float height;

	FGrassData(FVector3f position, float height) :
		position(float3(position.X, position.Y, position.Z)),
		height(height)
	{}

	FGrassData() : FGrassData(FVector3f(), 10.0f)
	{}
};