// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
#include "CoreMinimal.h"
#include "DataDrivenShaderPlatformInfo.h"
#include "GrassData.h"

#include "GlobalShader.h"
#include "ShaderParameterUtils.h"
#include "Shader.h"
#include "ShaderParameterStruct.h"
#include "ShaderCompilerCore.h"

namespace GrassMesh // GrassShaders
{

	// ************************************************************************************************************** //
	// ********************************************* Compute Shaders ************************************************ //
	// ************************************************************************************************************** //
	
	/** InitInstanceBuffer compute shader. */
	class COMPUTESHADERS_API FInitInstanceBuffer_CS : public FGlobalShader
	{
	public:
		DECLARE_GLOBAL_SHADER(FInitInstanceBuffer_CS);
		SHADER_USE_PARAMETER_STRUCT(FInitInstanceBuffer_CS, FGlobalShader);

		BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
			SHADER_PARAMETER(uint32, NumIndices)
			SHADER_PARAMETER_UAV(RWBuffer<uint>, RWIndirectArgsBuffer)
		END_SHADER_PARAMETER_STRUCT()

			static bool ShouldCompilePermutation(FGlobalShaderPermutationParameters const& Parameters)
		{
			return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM6);
		}
	};

	class COMPUTESHADERS_API FComputeInstanceData_CS : public FGlobalShader
	{

	public:
		DECLARE_GLOBAL_SHADER(FComputeInstanceData_CS);
		SHADER_USE_PARAMETER_STRUCT(FComputeInstanceData_CS, FGlobalShader);

		BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
			SHADER_PARAMETER(FVector3f, CameraPosition)
			SHADER_PARAMETER(uint32, GrassDataSize)
			SHADER_PARAMETER(float, CutoffDistance)
			SHADER_PARAMETER(FMatrix44f, VP_MATRIX)
			SHADER_PARAMETER_SRV(StructuredBuffer<FPackedGrassData>, GrassDataBuffer)
			SHADER_PARAMETER_UAV(RWStructuredBuffer<FGrassInstance>, RWInstanceBuffer)
			SHADER_PARAMETER_UAV(RWStructuredBuffer<uint32>, RWIndirectArgsBuffer)
		END_SHADER_PARAMETER_STRUCT()

		static bool ShouldCompilePermutation(FGlobalShaderPermutationParameters const& Parameters)
		{
			return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM6);
		}
	};
}