// Fill out your copyright notice in the Description page of Project Settings.


#include "GrassShaders.h"

// Begin implementations
IMPLEMENT_GLOBAL_SHADER(GrassMesh::FInitInstancingBuffers_CS, "/Shaders/GrassCompute.usf", "InitInstancingBuffersCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(GrassMesh::FInitInstancingInstanceBuffer_CS, "/Shaders/GrassCompute.usf", "InitInstancingIndirectArgsCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(GrassMesh::FCullInstancingGrassData_CS, "/Shaders/GrassCompute.usf", "CullInstancingGrassDataCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(GrassMesh::FComputeInstancingData_CS, "/Shaders/GrassCompute.usf", "ComputeInstancingDataCS", SF_Compute);


IMPLEMENT_GLOBAL_SHADER(GrassMesh::FInitBuffers_CS, "/Shaders/GrassCompute.usf", "InitBuffersCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(GrassMesh::FInitInstanceBuffer_CS, "/Shaders/GrassCompute.usf", "InitIndirectArgsCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(GrassMesh::FCullGrassData_CS, "/Shaders/GrassCompute.usf", "CullGrassDataCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(GrassMesh::FComputeGrassMesh_CS, "/Shaders/GrassCompute.usf", "ComputeGrassMeshCS", SF_Compute);