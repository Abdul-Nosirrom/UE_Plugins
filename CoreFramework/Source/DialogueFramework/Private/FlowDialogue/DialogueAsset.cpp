// Copyright 2023 CoC All rights reserved


#include "FlowDialogue/DialogueAsset.h"

#include "Components/DialogueInteractableComponent.h"

UDialogueAsset::UDialogueAsset()
{
	ExpectedOwnerClass = UDialogueInteractableComponent::StaticClass();
}

void UDialogueAsset::StartFlow()
{
	// Load save data
	LoadInstance(SaveData);
	
	Super::StartFlow();
}

void UDialogueAsset::FinishFlow(const EFlowFinishPolicy InFinishPolicy, const bool bRemoveInstance)
{
	// Save asset
	TArray<FFlowAssetSaveData> DummySaves;
	SaveData = SaveInstance(DummySaves);
	
	Super::FinishFlow(InFinishPolicy, bRemoveInstance);

	OnDialogueGraphCompleted.Broadcast();
}