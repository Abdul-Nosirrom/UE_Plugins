// Copyright 2023 CoC All rights reserved


#include "FlowDialogue/DialogueChoice.h"

#include "Components/DialogueInteractableComponent.h"

void UDialogueChoice::ExecuteInput(const FName& PinName)
{
	OnDialogueChoiceSelected.BindUObject(this, &UDialogueChoice::OnChoiceSelected);
	
	// Register with dialogue component?
	GetOwnerDialogueComponent()->RegisterDialogueChoice(OnDialogueChoiceSelected, Choices);

	Super::ExecuteInput(PinName);

	// Instantly execute the default 'Out' output
	TriggerFirstOutput(false);

	// Bind to UI event
	//GetOwnerDialogueComponent()
}

void UDialogueChoice::Cleanup()
{
	OnDialogueChoiceSelected.Unbind();
	// Unbind from UI event
	GetOwnerDialogueComponent()->UnregisterDialogueChoice();

	Super::Cleanup();
}

void UDialogueChoice::OnChoiceSelected(int ChoiceIdx)
{
	TriggerOutput(FName(Choices[ChoiceIdx].ToString()));
}
