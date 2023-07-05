// Copyright Epic Games, Inc. All Rights Reserved.

#include "ScalarViser.h"

#include <iostream>

#define LOCTEXT_NAMESPACE "FScalarViserModule"

void FScalarViserModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
	FString pluginShaderDir = FPaths::Combine(
		FPaths::ProjectPluginsDir(), TEXT("SciViser"), TEXT("Source"), TEXT("ScalarViser"), TEXT("Shaders"));
	AddShaderSourceDirectoryMapping(TEXT("/ScalarViser"), pluginShaderDir);
}

void FScalarViserModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FScalarViserModule, ScalarViser)