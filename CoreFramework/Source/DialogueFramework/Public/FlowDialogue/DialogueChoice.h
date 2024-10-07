// Copyright 2023 CoC All rights reserved

#pragma once

#include "CoreMinimal.h"
#include "DialogueNodeBase.h"
#include "Components/DialogueInteractableComponent.h"
#include "DialogueChoice.generated.h"

/**
 * 
 */
UCLASS()
class DIALOGUEFRAMEWORK_API UDialogueChoice : public UDialogueNodeBase
{
	GENERATED_BODY()

	friend class UDialogueInteractableComponent;
	
protected:

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TArray<FText> Choices;
	FDialogueEvent OnDialogueChoiceSelected;

	virtual void ExecuteInput(const FName& PinName) override;
	virtual void Cleanup() override;

	void OnChoiceSelected(int ChoiceIdx);

#if WITH_EDITOR 
	virtual bool SupportsContextPins() const override { return true; }
	virtual TArray<FFlowPin> GetContextOutputs() const override
	{
		TArray<FFlowPin> OutPins;
		for (const FText& Choice : Choices) OutPins.Add(FFlowPin(Choice));

		return OutPins;
	}
#endif
};
