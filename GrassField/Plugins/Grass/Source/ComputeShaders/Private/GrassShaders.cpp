// Fill out your copyright notice in the Description page of Project Settings.


#include "GrassShaders.h"

// Begin GrassMesh implementations
IMPLEMENT_GLOBAL_SHADER(GrassMesh::FInitBuffers_CS, "/Shaders/GrassCompute.usf", "InitBuffersCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(GrassMesh::FCullGrassData_CS, "/Shaders/GrassCompute.usf", "CullGrassDataCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(GrassMesh::FComputeGrassMesh_CS, "/Shaders/GrassCompute.usf", "ComputeGrassMeshCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(GrassMesh::FInitInstanceBuffer_CS, "/Shaders/GrassCompute.usf", "InitIndirectArgsCS", SF_Compute);