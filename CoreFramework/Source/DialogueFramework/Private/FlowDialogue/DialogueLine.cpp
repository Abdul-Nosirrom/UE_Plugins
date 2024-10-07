// Copyright 2023 CoC All rights reserved


#include "FlowDialogue/DialogueLine.h"
#include "FlowDialogue/DialogueAsset.h"

UDialogueLine::UDialogueLine()
{
	if (const UDialogueAsset* DialogueAsset = GetOwnerDialogueAsset())
	{
		//Speaker = DialogueAsset->PrimarySpeaker;
	}
}

void UDialogueLine::ExecuteInput(const FName& PinName)
{
	OnDialogueLinesCompleted.BindLambda([this](int32 Payload)
	{
		TriggerFirstOutput(false);	
	});
	
	GetOwnerDialogueComponent()->RegisterDialogueLine(OnDialogueLinesCompleted, FText::FromString(Speaker), Lines);
	
	Super::ExecuteInput(PinName);
}

void UDialogueLine::Cleanup()
{
	OnDialogueLinesCompleted.Unbind();
	
	GetOwnerDialogueComponent()->UnregisterDialogueLine();

	Super::Cleanup();
}

TArray<FString> UDialogueLine::GetParticipatingSpeakers() const
{
	if (GetOwnerDialogueAsset())
	{
		TArray<FText> Speakers = GetOwnerDialogueAsset()->AvailableSpeakers;
		TArray<FString> FNameSpeakers; // Need FName to get "GetOptions" to work, the one in the data asset is an FText for localization
		for (auto SpeakerName : Speakers)
		{
			FNameSpeakers.Add(SpeakerName.ToString());
		}

		return FNameSpeakers;
	}
	
	return TArray<FString>();
}
