// Copyright 2023 CoC All rights reserved


#include "ComponentVisualizer/ScriptComponentVisualizerRegistry.h"

#include "AngelscriptCodeModule.h"
#include "AngelscriptManager.h"
#include "UnrealEdGlobals.h"
#include "ClassGenerator/AngelscriptClassGenerator.h"
#include "ComponentVisualizer/ScriptComponentVisualizer.h"
#include "Editor/UnrealEdEngine.h"


void UScriptComponentVisualizerRegistry::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	RegisterComponentVisualizers();

	ConsoleCommands.Add(IConsoleManager::Get().RegisterConsoleCommand(
		*RegisterVisualizers,
		TEXT("Registers component visualizers for AS scripts if new ones have been created"),
		FConsoleCommandDelegate::CreateUObject(this, &UScriptComponentVisualizerRegistry::RegisterComponentVisualizers),
		ECVF_Cheat));

	// Bind post hot reload as class references can break i think? Specifically the WeakObjPtr in the FComponentVisualizer 
	FAngelscriptClassGenerator::OnPostReload.AddLambda([this] (bool bVal) { RegisterComponentVisualizers(); });
}

void UScriptComponentVisualizerRegistry::Deinitialize()
{
	Super::Deinitialize();

	UnRegisterComponentVisualizers();

	for (IConsoleObject* ConsoleCommand : ConsoleCommands) IConsoleManager::Get().UnregisterConsoleObject(ConsoleCommand);
}

void UScriptComponentVisualizerRegistry::RegisterComponentVisualizers()
{
	UnRegisterComponentVisualizers();
	
	// Gather all UScriptComponentVisualizer classes
	for (TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt)
	{
		UClass* Class = *ClassIt;
		if (Class->IsChildOf(UScriptComponentVisualizer::StaticClass()) && !Class->ClassGeneratedBy)
		{
			// Parent class is abstract, skip it
			if (Class == UScriptComponentVisualizer::StaticClass()) continue;

			auto scriptVis = NewObject<UScriptComponentVisualizer>(this, Class);
			const FName compName = scriptVis->VisualizedClass->GetFName();
			
			ScriptVisualizers.Add(scriptVis);
			RegisteredComponents.Add(compName);
			GUnrealEd->RegisterComponentVisualizer(compName, MakeShared<FScriptComponentVisualizer>(scriptVis));
		}
	}
}

void UScriptComponentVisualizerRegistry::UnRegisterComponentVisualizers()
{
	for (const auto& compName : RegisteredComponents)
	{
		GUnrealEd->UnregisterComponentVisualizer(compName);
	}

	RegisteredComponents.Empty();
	ScriptVisualizers.Empty();
}
