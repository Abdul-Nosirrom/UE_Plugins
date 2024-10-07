// Copyright 2023 CoC All rights reserved


#include "Animation/CombatNotifies.h"

#include "RadicalCharacter.h"
#include "Data/CombatData.h"



void UAnimNotify_ClearHitList::BranchingPointNotify(FBranchingPointNotifyPayload& BranchingPointPayload)
{
	if (USkeletalMeshComponent* MeshComp = BranchingPointPayload.SkelMeshComponent)
	{
		if (UAnimInstance* AnimInstance = MeshComp->GetAnimInstance())
		{
			AnimInstance->OnPlayMontageNotifyBegin.Broadcast(CombatNotifies::ClearHitListNotify, BranchingPointPayload);
		}
	}
}

FString UAnimNotify_ClearHitList::GetNotifyName_Implementation() const
{
	return CombatNotifies::ClearHitListNotify.ToString();
}

void UAnimNotify_LaunchCharacter::BranchingPointNotify(FBranchingPointNotifyPayload& BranchingPointPayload)
{
	if (const auto Character = Cast<ARadicalCharacter>(BranchingPointPayload.SkelMeshComponent->GetOwner()))
	{
		const FVector ProperLaunch = Character->GetActorForwardVector() * LaunchVel.X + Character->GetActorRightVector() * LaunchVel.Y + FVector::UpVector * LaunchVel.Z;
		Character->LaunchCharacter(ProperLaunch, bPlanarOverride, bVerticalOverride);
	}
}
