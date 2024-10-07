// Copyright 2023 CoC All rights reserved


#include "FlowDialogue/FlowNode_DialogueSequencer.h"

#include "Components/DialogueInteractableComponent.h"

void UFlowNode_DialogueSequencer::ExecuteInput(const FName& PinName)
{
	Super::ExecuteInput(PinName);

	if (UDialogueInteractableComponent* DialogueComp = Cast<UDialogueInteractableComponent>(TryGetRootFlowObjectOwner()))
	{
		DialogueComp->SetActiveSequencer(SequencePlayer);
	}
}
