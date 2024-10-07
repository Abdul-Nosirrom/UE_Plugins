// Copyright 2023 CoC All rights reserved

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "DialogueControllerInterface.generated.h"


struct DIALOGUEFRAMEWORK_API FDialogueControllerPayload
{

public:
	enum class EPayloadType : int
	{
		InitDialogue,
		DialogueLine,
		DialogueChoices,
		DeinitDialogue
	};

	FDialogueControllerPayload() {}

	FDialogueControllerPayload(const FText& InSpeakerName, const FText& InDialogueLine)
		: PayloadType(EPayloadType::DialogueLine), SpeakerName(InSpeakerName), DialogueLine(InDialogueLine) {}
	FDialogueControllerPayload(const TArray<FText>& InChoices)
		: PayloadType(EPayloadType::DialogueChoices), PopulatedChoices(InChoices) {}
	
	bool operator==(const EPayloadType& Other) const { return PayloadType == Other; }
	bool operator!=(const EPayloadType& Other) const { return PayloadType != Other; }

private:
	FDialogueControllerPayload(EPayloadType InPayload) : PayloadType(InPayload) {}

public:
	EPayloadType PayloadType;

	FText SpeakerName;
	FText DialogueLine;
	TArray<FText> PopulatedChoices;

	static const FDialogueControllerPayload Init;
	static const FDialogueControllerPayload Deinit;
};

// This class does not need to be modified.
UINTERFACE()
class UDialogueControllerInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class DIALOGUEFRAMEWORK_API IDialogueControllerInterface
{
	GENERATED_BODY()

	friend class UDialogueInteractableComponent;
	// Add interface functions to this class. This is the class that will be inherited to implement this interface.
protected:
	
	virtual bool OnDialoguePayloadReceived(class UDialogueInteractableComponent* DialogueOwner, const FDialogueControllerPayload& Payload) = 0;
};
