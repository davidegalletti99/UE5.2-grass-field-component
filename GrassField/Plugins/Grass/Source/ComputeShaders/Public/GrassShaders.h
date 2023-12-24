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
	class COMPUTESHADERS_API FInitInstancingInstanceBuffer_CS : public FGlobalShader
	{
	public:
		DECLARE_GLOBAL_SHADER(FInitInstancingInstanceBuffer_CS);
		SHADER_USE_PARAMETER_STRUCT(FInitInstancingInstanceBuffer_CS, FGlobalShader);

		BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
			SHADER_PARAMETER(uint32, NumIndices)
			SHADER_PARAMETER_UAV(RWBuffer<uint>, RWIndirectArgsBuffer)
		END_SHADER_PARAMETER_STRUCT()

			static bool ShouldCompilePermutation(FGlobalShaderPermutationParameters const& Parameters)
		{
			return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM6);
		}
	};

	class COMPUTESHADERS_API FComputeInstanceGrassData_CS : public FGlobalShader
	{

	public:
		DECLARE_GLOBAL_SHADER(FComputeInstanceGrassData_CS);
		SHADER_USE_PARAMETER_STRUCT(FComputeInstanceGrassData_CS, FGlobalShader);

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

	// -------------------------------------------------------------------------------------------------------------- //
	/** Compute shader to initialize all buffers, including adding the lowest mip page(s) to the QuadBuffer. */
	class COMPUTESHADERS_API FInitBuffers_CS : public FGlobalShader
	{
	public:
		DECLARE_GLOBAL_SHADER(FInitBuffers_CS);
		SHADER_USE_PARAMETER_STRUCT(FInitBuffers_CS, FGlobalShader);

		BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
			SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint32>, RWIndirectArgsBuffer)
			SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint32>, RWCounter)
		END_SHADER_PARAMETER_STRUCT()

		static bool ShouldCompilePermutation(FGlobalShaderPermutationParameters const& Parameters)
		{
			return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM6);
		}
	};

	/** InitInstanceBuffer compute shader. */
	class COMPUTESHADERS_API FInitInstanceBuffer_CS : public FGlobalShader
	{
	public:
		DECLARE_GLOBAL_SHADER(FInitInstanceBuffer_CS);
		SHADER_USE_PARAMETER_STRUCT(FInitInstanceBuffer_CS, FGlobalShader);

		BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
			SHADER_PARAMETER_UAV(RWBuffer<uint>, RWIndirectArgsBuffer)
		END_SHADER_PARAMETER_STRUCT()

			static bool ShouldCompilePermutation(FGlobalShaderPermutationParameters const& Parameters)
		{
			return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM6);
		}
	};

	class COMPUTESHADERS_API FCullGrassData_CS : public FGlobalShader
	{

	public:
		DECLARE_GLOBAL_SHADER(FCullGrassData_CS);
		SHADER_USE_PARAMETER_STRUCT(FCullGrassData_CS, FGlobalShader);

		BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
			SHADER_PARAMETER(FVector3f, CameraPosition)
			SHADER_PARAMETER(uint32, GrassDataSize)
			SHADER_PARAMETER(float, CutoffDistance)
			SHADER_PARAMETER(FMatrix44f, VP_MATRIX)
			SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint32>, RWCounter)
			SHADER_PARAMETER_SRV(StructuredBuffer<FPackedGrassData>, GrassDataBuffer)
			SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<FPackedLodGrassData>, RWCulledGrassDataBuffer)
			SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint32>, RWIndirectArgsBuffer)
		END_SHADER_PARAMETER_STRUCT()

		static bool ShouldCompilePermutation(FGlobalShaderPermutationParameters const& Parameters)
		{
			return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM6);
		}
	};

	class COMPUTESHADERS_API FComputeGrassMesh_CS : public FGlobalShader
	{

	public:
		DECLARE_GLOBAL_SHADER(FComputeGrassMesh_CS);
		SHADER_USE_PARAMETER_STRUCT(FComputeGrassMesh_CS, FGlobalShader);


		class FReuseCullDim : SHADER_PERMUTATION_BOOL("REUSE_CULL");
		using FPermutationDomain = TShaderPermutationDomain<FReuseCullDim>;

		BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
			SHADER_PARAMETER(FVector3f, CameraPosition)
			SHADER_PARAMETER(FMatrix44f, ViewMatrix)
			SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint32>, RWCounter)
			SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FPackedLodGrassData>, CulledGrassDataBuffer)
			SHADER_PARAMETER_UAV(RWStructuredBuffer<FGrassVertex>, RWVertexBuffer)
			SHADER_PARAMETER_UAV(RWStructuredBuffer<uint32>, RWIndexBuffer)
			SHADER_PARAMETER_UAV(RWStructuredBuffer<FGrassInstance>, RWInstanceBuffer)
			SHADER_PARAMETER_UAV(RWStructuredBuffer<uint32>, RWIndirectArgsBuffer)
		END_SHADER_PARAMETER_STRUCT()

		static bool ShouldCompilePermutation(FGlobalShaderPermutationParameters const& Parameters)
		{
			return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM6);
		}
	};

}