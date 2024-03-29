﻿// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "K2Node_BaseAsyncTask.h"

#include "GameFramework/RootMotionSource.h"
#include "RootMotionTasks/RootMotionTask_Base.h"
#include "RootMotionTasks/RootMotionTask_ConstantForce.h"
#include "RootMotionTasks/RootMotionTask_JumpForce.h"
#include "RootMotionTasks/RootMotionTask_MoveToActorForce.h"
#include "RootMotionTasks/RootMotionTask_MoveToForce.h"

#include "K2Node_RootMotionTaskCall.generated.h"

UCLASS()
class EDITORUTILITIES_API UK2Node_RootMotionTaskCall : public UK2Node_BaseAsyncTask
{
	GENERATED_BODY()

public:
	UK2Node_RootMotionTaskCall(const FObjectInitializer& ObjectInitializer);

	// UEdGraphNode interface
	virtual bool CanCreateUnderSpecifiedSchema(const UEdGraphSchema* DesiredSchema) const override;
	virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
	// End of UEdGraphNode interface

	virtual void ReallocatePinsDuringReconstruction(TArray<UEdGraphPin*>& OldPins) override;
	virtual void PinDefaultValueChanged(UEdGraphPin* Pin) override;
	bool ConnectSpawnProperties(UClass* ClassToSpawn, const UEdGraphSchema_K2* Schema, class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph, UEdGraphPin*& LastThenPin, UEdGraphPin* SpawnedActorReturnPin);		//Helper
	virtual void ExpandNode(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph) override;

	void CreatePinsForClass(UClass* InClass);
	UEdGraphPin* GetClassPin(const TArray<UEdGraphPin*>* InPinsToSearch = NULL) const;
	UClass* GetClassToSpawn(const TArray<UEdGraphPin*>* InPinsToSearch = NULL) const;
	UEdGraphPin* GetResultPin() const;
	bool IsSpawnVarPin(UEdGraphPin* Pin);
	bool ValidateActorSpawning(class FKismetCompilerContext& CompilerContext, bool bGenerateErrors);
	bool ValidateActorArraySpawning(class FKismetCompilerContext& CompilerContext, bool bGenerateErrors);

	UPROPERTY()
	TArray<FName> SpawnParamPins;

	static void RegisterSpecializedTaskNodeClass(TSubclassOf<UK2Node_RootMotionTaskCall> NodeClass);
protected:
	static bool HasDedicatedNodeClass(TSubclassOf<URootMotionTask_Base> TaskClass);

	virtual bool IsHandling(TSubclassOf<URootMotionTask_Base> TaskClass) const { return true; }

private:
	static TArray<TWeakObjectPtr<UClass> > NodeClasses;
};
