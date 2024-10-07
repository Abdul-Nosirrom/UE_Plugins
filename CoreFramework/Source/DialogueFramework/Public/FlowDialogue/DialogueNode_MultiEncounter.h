// Copyright 2023 CoC All rights reserved

#pragma once

#include "CoreMinimal.h"
#include "DialogueNodeBase.h"
#include "DialogueNode_MultiEncounter.generated.h"

/**
 * 
 */
UCLASS()
class DIALOGUEFRAMEWORK_API UDialogueNode_MultiEncounter : public UDialogueNodeBase
{
	GENERATED_BODY()

	UDialogueNode_MultiEncounter();

private:
	UPROPERTY(SaveGame)
	int32 NextOutput;

public:
#if WITH_EDITOR
	virtual bool CanUserAddOutput() const override { return true; }
#endif

protected:
	virtual void ExecuteInput(const FName& PinName) override;
	virtual void Cleanup() override;
};
