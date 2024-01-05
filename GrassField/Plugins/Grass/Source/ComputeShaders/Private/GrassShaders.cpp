// Fill out your copyright notice in the Description page of Project Settings.


#include "GrassShaders.h"

// Begin implementations
IMPLEMENT_GLOBAL_SHADER(GrassMesh::FComputePhysics_CS, "/Shaders/GrassPhysicsModel.usf", "ComputePhysicsCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(GrassMesh::FInitInstanceBuffer_CS, "/Shaders/GrassCompute.usf", "InitIndirectArgsCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(GrassMesh::FComputeInstanceData_CS, "/Shaders/GrassCompute.usf", "ComputeInstanceGrassDataCS", SF_Compute);