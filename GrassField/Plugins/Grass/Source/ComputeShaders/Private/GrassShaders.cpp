// Fill out your copyright notice in the Description page of Project Settings.


#include "GrassShaders.h"

// Begin implementations
IMPLEMENT_GLOBAL_SHADER(GrassUtils::FInitForceMap_CS, "/Shaders/GrassPhysicsModel.usf", "InitForceMapCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(GrassUtils::FComputePhysics_CS, "/Shaders/GrassPhysicsModel.usf", "ComputePhysicsCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(GrassUtils::FInitInstanceBuffer_CS, "/Shaders/GrassCompute.usf", "InitIndirectArgsCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(GrassUtils::FCullInstances_CS, "/Shaders/GrassCompute.usf", "CullInstancesCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(GrassUtils::FComputeInstanceData_CS, "/Shaders/GrassCompute.usf", "ComputeInstanceGrassDataCS", SF_Compute);

