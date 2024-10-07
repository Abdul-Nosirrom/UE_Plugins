// Copyright 2023 CoC All rights reserved

#pragma once

#include "CoreMinimal.h"
#include "DialogueNodeBase.h"
#include "Components/DialogueInteractableComponent.h"
#include "DialogueLine.generated.h"

/**
 * 
 */
UCLASS()
class DIALOGUEFRAMEWORK_API UDialogueLine : public UDialogueNodeBase
{
	GENERATED_BODY()

	friend class UDialogueInteractableComponent;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(GetOptions="GetParticipatingSpeakers"))
	FString Speaker;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(MultiLine=true))
	TArray<FText> Lines;
	
	FDialogueEvent OnDialogueLinesCompleted;
	
	UDialogueLine();
	
	virtual void ExecuteInput(const FName& PinName) override;
	virtual void Cleanup() override;

	UFUNCTION()
	TArray<FString> GetParticipatingSpeakers() const;

#if WITH_EDITOR
	virtual FString GetNodeDescription() const override
	{
		return "(" + Speaker + "): " + FString::FromInt(Lines.Num()) + " lines";
	}
#endif
};
