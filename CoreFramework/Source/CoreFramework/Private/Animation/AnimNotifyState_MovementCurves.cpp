// Copyright 2023 CoC All rights reserved


#include "Animation/AnimNotifyState_MovementCurves.h"

#include "RadicalCharacter.h"
#include "RadicalMovementComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "StaticLibraries/CoreMathLibrary.h"

void UAnimNotifyState_MovementCurves::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
                                                  float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);
	// btw remember this is shared among anim instances, may be good for Init here to return an *instance* that does it, or take over the binding and manually do it here (?)
	Params.Init(PhysicsInfo[MeshComp].Character->GetCharacterMovement(), TotalDuration);

	MeshComp->GetAnimInstance()->OnMontageBlendingOut.AddDynamic(this, &UAnimNotifyState_MovementCurves::OnMontageBlendedOut);
}

void UAnimNotifyState_MovementCurves::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Params.DeInit(PhysicsInfo[MeshComp].Character->GetCharacterMovement());
	Super::NotifyEnd(MeshComp, Animation, EventReference);
}

void UAnimNotifyState_MovementCurves::OnMontageBlendedOut(UAnimMontage* Montage, bool bInterrupted)
{
	// NOTE: Temp hack to not stifle followup actions. E.g if we jump right after an action montage that uses this, it wont work normally since we unbind from physics after the montage blends out which could be after jump starts, so we instead unbind when we begin blending out which causes it to work
	for (auto Phys : PhysicsInfo)
	{
		Params.DeInit(Phys.Value.Character->GetCharacterMovement());
		Phys.Key->GetAnimInstance()->OnMontageBlendingOut.RemoveDynamic(this, &UAnimNotifyState_MovementCurves::OnMontageBlendedOut);
	}
}
