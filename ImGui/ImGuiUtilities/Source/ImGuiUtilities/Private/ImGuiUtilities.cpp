// Copyright Epic Games, Inc. All Rights Reserved.

#include "ImGuiUtilities.h"
#include "ImGuiWindowManager.h"
#include "ActionSystem/GameplayWindow_Actions.h"
#include "Engine/EngineWindow_ActorSelection.h"
#include "Engine/EngineWindow_Plots.h"
#include "Input/InputWindow_InputActions.h"
#include "Input/InputWindow_InputBuffer.h"

#define LOCTEXT_NAMESPACE "FImGuiUtilitiesModule"

void FImGuiUtilitiesModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
	UImGuiWindowManager::Get()->AddWindow<FEngineWindow_ActorSelection>("Engine.Selection");
	UImGuiWindowManager::Get()->AddWindow<FEngineWindow_Plots>("Engine.Plots");

	UImGuiWindowManager::Get()->AddWindow<FGameplayWindow_Actions>("Gameplay.Actions");

	UImGuiWindowManager::Get()->AddWindow<FInputWindow_InputBuffer>("Input.Buffer");
	UImGuiWindowManager::Get()->AddWindow<FInputWindow_InputActions>("Input.Actions");

}

void FImGuiUtilitiesModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FImGuiUtilitiesModule, ImGuiUtilities)