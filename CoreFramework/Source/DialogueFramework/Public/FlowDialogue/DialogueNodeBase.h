// Copyright 2023 CoC All rights reserved

#pragma once

#include "CoreMinimal.h"
#include "Nodes/FlowNode.h"
#include "DialogueNodeBase.generated.h"

/// @brief	Contains common functionality that dialogue nodes may want to use
UCLASS(Abstract)
class DIALOGUEFRAMEWORK_API UDialogueNodeBase : public UFlowNode
{
	GENERATED_BODY()

protected:
	UDialogueNodeBase();
	
	class UDialogueInteractableComponent* GetOwnerDialogueComponent() const;
	class UDialogueAsset* GetOwnerDialogueAsset() const;

#if WITH_EDITOR
	virtual EDataValidationResult ValidateNode() override;
#endif 
};

UCLASS()
class DIALOGUEFRAMEWORK_API UDialogueHideUI : public UDialogueNodeBase
{
	GENERATED_BODY()

protected:
	virtual void ExecuteInput(const FName& PinName) override;
};
