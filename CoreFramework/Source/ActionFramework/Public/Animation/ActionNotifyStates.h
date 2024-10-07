// Copyright 2023 CoC All rights reserved

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "ActionNotifyStates.generated.h"


/// @brief	Sets CanCancel to true when this notify begins and false again when it ends. Note, only works with animation events on an action
UCLASS(DisplayName="Cancel Window")
class ACTIONFRAMEWORK_API UActionNotifyState_CanCancel : public UAnimNotifyState
{
	GENERATED_BODY()

	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;

	virtual FString GetNotifyName_Implementation() const override { return "Cancel Window"; }
};
