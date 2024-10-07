// Copyright 2023 CoC All rights reserved


#include "FlowDialogue/DialogueNode_MultiEncounter.h"

UDialogueNode_MultiEncounter::UDialogueNode_MultiEncounter()
{
#if WITH_EDITOR
	NodeStyle = EFlowNodeStyle::Logic;
#endif
	
	SetNumberedOutputPins(0, 1);
	AllowedSignalModes = {EFlowSignalMode::Enabled, EFlowSignalMode::Disabled};
}

void UDialogueNode_MultiEncounter::ExecuteInput(const FName& PinName)
{
	const int32 CurrentOutput = NextOutput;
	NextOutput++;
	NextOutput = FMath::Clamp(NextOutput, 0, OutputPins.Num()-1);
	TriggerOutput(OutputPins[CurrentOutput].PinName, false);
}

void UDialogueNode_MultiEncounter::Cleanup()
{
	NextOutput = 0;
	Super::Cleanup();
}

