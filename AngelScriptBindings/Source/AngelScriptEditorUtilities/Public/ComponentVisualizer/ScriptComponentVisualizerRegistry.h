// Copyright 2023 CoC All rights reserved

#pragma once

#include "CoreMinimal.h"
#include "ScriptComponentVisualizerRegistry.generated.h"
UCLASS()
class ANGELSCRIPTEDITORUTILITIES_API UScriptComponentVisualizerRegistry : public UEditorSubsystem
{
	GENERATED_BODY()
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

protected:
	void RegisterComponentVisualizers();
	void UnRegisterComponentVisualizers();

	TArray<FName> RegisteredComponents;
	UPROPERTY(Transient)
	TArray<TObjectPtr<class UScriptComponentVisualizer>> ScriptVisualizers;

	TArray<IConsoleCommand*> ConsoleCommands;

	inline static FString RegisterVisualizers = "Editor.RegisterASVisualizers";
};
