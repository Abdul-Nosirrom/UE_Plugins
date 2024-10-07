// Copyright 2023 CoC All rights reserved

#pragma once

#include "CoreMinimal.h"
#include "FlowAsset.h"
#include "DialogueAsset.generated.h"

DECLARE_MULTICAST_DELEGATE(FOnDialogueGraphCompleted)


UCLASS()
class DIALOGUEFRAMEWORK_API UDialogueAsset : public UFlowAsset
{
	GENERATED_BODY()

	friend class UDialogueLine;
	friend class UDialogueInteractableComponent;
	
protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TArray<FText> AvailableSpeakers;
	FOnDialogueGraphCompleted OnDialogueGraphCompleted;

	FFlowAssetSaveData SaveData;

	UDialogueAsset();

	virtual void StartFlow() override;
	virtual void FinishFlow(const EFlowFinishPolicy InFinishPolicy, const bool bRemoveInstance) override;

};
