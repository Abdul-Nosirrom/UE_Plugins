// Copyright 2023 CoC All rights reserved


#include "Animation/ActionNotifies.h"

#include "Components/SkeletalMeshComponent.h"


void UActionNotify_CanCancel::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	if (UAnimInstance* AnimInstance = MeshComp->GetAnimInstance())
	{
		AnimInstance->OnPlayMontageNotifyBegin.Broadcast(ActionNotifies::CanCancelNotify, FBranchingPointNotifyPayload());
	}
}
