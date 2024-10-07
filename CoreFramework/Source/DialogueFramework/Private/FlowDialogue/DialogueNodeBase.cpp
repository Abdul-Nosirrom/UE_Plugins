// Copyright 2023 CoC All rights reserved


#include "FlowDialogue/DialogueNodeBase.h"
#include "Components/DialogueInteractableComponent.h"
#include "FlowDialogue/DialogueAsset.h"

UDialogueNodeBase::UDialogueNodeBase()
{
#if WITH_EDITOR
	Category = TEXT("Dialogue");
#endif 
}

UDialogueInteractableComponent* UDialogueNodeBase::GetOwnerDialogueComponent() const
{
	if (UDialogueInteractableComponent* DialogueComp = Cast<UDialogueInteractableComponent>(TryGetRootFlowObjectOwner()))
	{
		return DialogueComp;
	}

	return nullptr;
}

UDialogueAsset* UDialogueNodeBase::GetOwnerDialogueAsset() const
{
	if (UDialogueAsset* DialogueAsset = Cast<UDialogueAsset>(GetFlowAsset()))
	{
		return DialogueAsset;
	}

	return nullptr;
}

void UDialogueHideUI::ExecuteInput(const FName& PinName)
{
	if (UDialogueInteractableComponent* DialogueComp = GetOwnerDialogueComponent())
	{
		DialogueComp->HideDialogueUI();
	}
	
	Super::ExecuteInput(PinName);

	TriggerFirstOutput(true);
}

#if WITH_EDITOR 
EDataValidationResult UDialogueNodeBase::ValidateNode()
{
	if (!Cast<UDialogueAsset>(GetFlowAsset()))
	{
		ValidationLog.Error<UFlowNode>(TEXT("Dialogue Nodes Can Only Be Placed In Dialogue Flow Graphs!"), this);
		return EDataValidationResult::Invalid;
	}
	return Super::ValidateNode();
}
#endif 
