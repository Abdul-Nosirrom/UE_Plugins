// Copyright 2023 CoC All rights reserved

#pragma once

#include "CoreMinimal.h"
#include "Nodes/World/FlowNode_PlayLevelSequence.h"
#include "FlowNode_DialogueSequencer.generated.h"

/**
 * 
 */
UCLASS(NotBlueprintable, meta = (DisplayName = "Play Dialogue Sequence"))
class DIALOGUEFRAMEWORK_API UFlowNode_DialogueSequencer : public UFlowNode_PlayLevelSequence
{
	GENERATED_BODY()

protected:
	UFlowNode_DialogueSequencer()
	{
#if WITH_EDITOR
		Category = TEXT("Dialogue");
#endif 
	}
	
	virtual void ExecuteInput(const FName& PinName) override;
};
