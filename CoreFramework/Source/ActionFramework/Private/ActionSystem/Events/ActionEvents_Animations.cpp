// Copyright 2023 CoC All rights reserved


#include "ActionSystem/Events/ActionEvents_Animations.h"

#include "RadicalMovementComponent.h"
#include "ActionSystem/GameplayAction.h"
#include "Components/SkeletalMeshComponent.h"
#include "Actors/RadicalCharacter.h"
#include "Animation/ActionNotifies.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "Debug/ActionSystemLog.h"

void UActionEvent_PlayMontage::Initialize(UGameplayAction* InOwnerAction)
{
	Super::Initialize(InOwnerAction);

	if (!Montage)
	{
		return;
	}

	if (bEndMontageWhenActionEnds)
		OwnerAction->OnGameplayActionEnded.Event.AddUObject(this, &UActionEvent_PlayMontage::OnActionEnd);
}

void UActionEvent_PlayMontage::ExecuteEvent()
{
	if (!Montage)
	{
		ACTIONSYSTEM_VLOG(GetCharacterInfo()->CharacterOwner.Get(), Warning, "Montage event failed, animation is null");
		return;
	}

	if (auto Mesh = GetCharacterInfo()->SkeletalMeshComponent.Get())
	{
		if (Mesh->GetAnimInstance()->Montage_Play(Montage, PlayRate) == 0.f)
		{
			ACTIONSYSTEM_VLOG(GetCharacterInfo()->CharacterOwner.Get(), Warning, "Failed to play montage in MontageEvent");
		}
	}
}

void UActionEvent_PlayMontage::OnActionEnd()
{
	if (auto Mesh = GetCharacterInfo()->SkeletalMeshComponent.Get())
	{
		if (Mesh->GetAnimInstance()->Montage_IsActive(Montage))
		{
			Mesh->GetAnimInstance()->Montage_Stop(Montage->GetBlendOutArgs().BlendTime, Montage);
		}
	}
}

/*--------------------------------------------------------------------------------------------------------------
* Advanced Montage
*--------------------------------------------------------------------------------------------------------------*/


void UActionEvent_ActionAnimation::Initialize(UGameplayAction* InOwnerAction)
{
	Super::Initialize(InOwnerAction);

	OwnerAction->OnGameplayActionStarted.Event.AddUObject(this, &UActionEvent_ActionAnimation::ExecuteEvent);
	OwnerAction->OnGameplayActionEnded.Event.AddUObject(this, &UActionEvent_ActionAnimation::OnActionEnd);
}

void UActionEvent_ActionAnimation::ExecuteEvent()
{
	// Play Montage
	if (!Montage)
	{
		ACTIONSYSTEM_VLOG(GetCharacterInfo()->CharacterOwner.Get(), Warning, "Montage event failed, animation is null");
		OwnerAction->CancelAction();
		return;
	}

	// Bind to montage notify events
	if (auto Mesh = GetCharacterInfo()->SkeletalMeshComponent.Get())
	{
		Mesh->GetAnimInstance()->OnMontageBlendingOut.AddDynamic(this, &UActionEvent_ActionAnimation::OnMontageEnded);
		Mesh->GetAnimInstance()->OnPlayMontageNotifyBegin.AddDynamic(this, &UActionEvent_ActionAnimation::OnNotifyBeginReceived);
		Mesh->GetAnimInstance()->OnPlayMontageNotifyEnd.AddDynamic(this, &UActionEvent_ActionAnimation::OnNotifyEndReceived);
	}
	
	if (auto Mesh = GetCharacterInfo()->SkeletalMeshComponent.Get())
	{
		if (Mesh->GetAnimInstance()->Montage_Play(Montage, PlayRate) == 0.f)
		{
			OwnerAction->CancelAction();
			ACTIONSYSTEM_VLOG(GetCharacterInfo()->CharacterOwner.Get(), Warning, "Failed to play montage in MontageEvent");
			return;
		}
	}

	// Cancel windows controlled via animation notifies
	OwnerAction->SetCanBeCanceled(false);
}

void UActionEvent_ActionAnimation::OnActionEnd()
{
	// Unbind from montage notify events
	if (auto Mesh = GetCharacterInfo()->SkeletalMeshComponent.Get())
	{
		// Ensure if its bound to movement events to stop it now
		if (GetCharacterInfo()->MovementComponent.IsValid() && GetCharacterInfo()->MovementComponent->RequestSecondaryVelocityBinding().IsBound())
		{
			if (Cast<UAnimNotifyState>(GetCharacterInfo()->MovementComponent->RequestSecondaryVelocityBinding().GetUObject()))
			{
				GetCharacterInfo()->MovementComponent->RequestSecondaryVelocityBinding().Unbind();
			}
			if (Cast<UAnimNotifyState>(GetCharacterInfo()->MovementComponent->RequestSecondaryRotationBinding().GetUObject()))
			{
				GetCharacterInfo()->MovementComponent->RequestSecondaryRotationBinding().Unbind();
			}
		}
		
		// Is the animation still not over? If so lets end it here
		if (Mesh->GetAnimInstance()->Montage_IsActive(Montage))
		{
			Mesh->GetAnimInstance()->Montage_Stop(Montage->GetBlendOutArgs().BlendTime, Montage);
		}
		
		Mesh->GetAnimInstance()->OnMontageBlendingOut.RemoveDynamic(this, &UActionEvent_ActionAnimation::OnMontageEnded);
		Mesh->GetAnimInstance()->OnPlayMontageNotifyBegin.RemoveDynamic(this, &UActionEvent_ActionAnimation::OnNotifyBeginReceived);
		Mesh->GetAnimInstance()->OnPlayMontageNotifyEnd.RemoveDynamic(this, &UActionEvent_ActionAnimation::OnNotifyEndReceived);
	}
}

void UActionEvent_ActionAnimation::OnNotifyBeginReceived(FName NotifyName, const FBranchingPointNotifyPayload& Payload)
{
	if (NotifyName == ActionNotifies::CanCancelNotify)
	{
		OwnerAction->SetCanBeCanceled(true);
	}
}

void UActionEvent_ActionAnimation::OnNotifyEndReceived(FName NotifyName, const FBranchingPointNotifyPayload& Payload)
{
	if (NotifyName == ActionNotifies::CanCancelNotify)
	{
		OwnerAction->SetCanBeCanceled(false);
	}
}

void UActionEvent_ActionAnimation::OnMontageEnded(UAnimMontage* EndedMontage, bool bInterrupted)
{
	// Is the action active? If so lets manually end it
	if (OwnerAction->IsActive())
	{
		OwnerAction->SetCanBeCanceled(true);
		OwnerAction->CancelAction();
	}
}
