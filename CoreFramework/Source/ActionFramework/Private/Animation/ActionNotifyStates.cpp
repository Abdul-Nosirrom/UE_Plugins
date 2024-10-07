// Copyright 2023 CoC All rights reserved


#include "Animation/ActionNotifyStates.h"

#include "Animation/ActionNotifies.h"
#include "Components/SkeletalMeshComponent.h"


void UActionNotifyState_CanCancel::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
                                               float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	if (UAnimInstance* AnimInstance = MeshComp->GetAnimInstance())
	{
		AnimInstance->OnPlayMontageNotifyBegin.Broadcast(ActionNotifies::CanCancelNotify, FBranchingPointNotifyPayload());
	}
}

void UActionNotifyState_CanCancel::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	if (UAnimInstance* AnimInstance = MeshComp->GetAnimInstance())
	{
		AnimInstance->OnPlayMontageNotifyEnd.Broadcast(ActionNotifies::CanCancelNotify, FBranchingPointNotifyPayload());
	}
}
