// Copyright 2023 CoC All rights reserved


#include "Interfaces/DialogueControllerInterface.h"


// Add default functionality here for any IDialogueControllerInterface functions that are not pure virtual.
const FDialogueControllerPayload FDialogueControllerPayload::Init = FDialogueControllerPayload(EPayloadType::InitDialogue);
const FDialogueControllerPayload FDialogueControllerPayload::Deinit = FDialogueControllerPayload(EPayloadType::DeinitDialogue);
