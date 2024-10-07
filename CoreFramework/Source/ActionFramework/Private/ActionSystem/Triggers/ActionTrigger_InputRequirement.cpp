// Copyright 2023 CoC All rights reserved


#include "ActionSystem/Triggers/ActionTrigger_InputRequirement.h"

#include "InputBufferSubsystem.h"
#include "RadicalCharacter.h"
#include "TimerManager.h"
#include "ActionSystem/GameplayAction.h"
#include "Components/ActionSystemComponent.h"
#include "Debug/ActionSystemLog.h"

/*--------------------------------------------------------------------------------------------------------------
* Data
*--------------------------------------------------------------------------------------------------------------*/

void FActionInputRequirement::BindInput(UInputBufferSubsystem* InputBuffer, UActionTrigger_InputRequirement* Action, int32 Priority) const
{
	switch (InputType)
	{
		case EActionInputType::Button:
			InputBuffer->BindAction(Action->ButtonInputDelegate, ButtonInput, TriggerEvent, false, Priority); break;
		case EActionInputType::Directional:
			InputBuffer->BindDirectionalAction(Action->DirectionalInputDelegate, DirectionalInput, false, Priority); break;
		case EActionInputType::ButtonSequence:
			InputBuffer->BindActionSequence(Action->ButtonSequenceInputDelegate, ButtonInput, SecondButtonInput, false, bFirstInputIsHold, bConsiderOrder, Priority); break;
		case EActionInputType::DirectionButtonSequence:
			InputBuffer->BindDirectionalActionSequence(Action->DirectionalAndButtonInputDelegate, ButtonInput, DirectionalInput, SequenceOrder, false, Priority);break;
	}
}

void FActionInputRequirement::UnbindInput(UInputBufferSubsystem* InputBuffer, UActionTrigger_InputRequirement* Action) const
{
	switch (InputType)
	{
		case EActionInputType::Button:					InputBuffer->UnbindAction(Action->ButtonInputDelegate); break;
		case EActionInputType::Directional:				InputBuffer->UnbindDirectionalAction(Action->DirectionalInputDelegate); break;
		case EActionInputType::ButtonSequence:			InputBuffer->UnbindActionSequence(Action->ButtonSequenceInputDelegate); break;
		case EActionInputType::DirectionButtonSequence: InputBuffer->UnbindDirectionalActionSequence(Action->DirectionalAndButtonInputDelegate);break;
	}
}

void FActionInputRequirement::ConsumeInput(UInputBufferSubsystem* InputBuffer) const
{
	switch (InputType)
	{
		case EActionInputType::Button:
		{
			InputBuffer->ConsumeInput(ButtonInput);
			break;
		}
		case EActionInputType::Directional:
		{
			InputBuffer->ConsumeInput(DirectionalInput);
			break;
		}
		case EActionInputType::ButtonSequence:
		{
			if (!bFirstInputIsHold)	
				InputBuffer->ConsumeInput(ButtonInput);
			InputBuffer->ConsumeInput(SecondButtonInput);
			break;
		}
		case EActionInputType::DirectionButtonSequence:
		{
			InputBuffer->ConsumeInput(DirectionalInput);
			InputBuffer->ConsumeInput(ButtonInput);
			break;
		}
	}
}

/*--------------------------------------------------------------------------------------------------------------
* Trigger
*--------------------------------------------------------------------------------------------------------------*/


void UActionTrigger_InputRequirement::Initialize(UGameplayAction* InOwnerAction)
{
	Super::Initialize(InOwnerAction);

	// Bind default events
	ButtonInputDelegate.Delegate.BindDynamic(this, &UActionTrigger_InputRequirement::ActivationButtonInput);
	ButtonSequenceInputDelegate.Delegate.BindDynamic(this, &UActionTrigger_InputRequirement::ActivationButtonSequence);
	DirectionalInputDelegate.Delegate.BindDynamic(this, &UActionTrigger_InputRequirement::ActivationDirectionalInput);
	DirectionalAndButtonInputDelegate.Delegate.BindDynamic(this, &UActionTrigger_InputRequirement::ActivationDirectionalActionInput);

	OwnerAction->OnGameplayActionStarted.Event.AddUObject(this, &UActionTrigger_InputRequirement::ParentActionStarted);

	NumInputBindingAttempts = 0;
}

void UActionTrigger_InputRequirement::Cleanup()
{
	Super::Cleanup();
	GetWorld()->GetTimerManager().ClearTimer(ReattemptHandle);
}

void UActionTrigger_InputRequirement::SetupTrigger(int32 Priority)
{
	if (!GetCharacterInfo()->CharacterOwner->GetInputBuffer())
	{
		ACTIONSYSTEM_VLOG(GetCharacterInfo()->CharacterOwner.Get(), Error, "Action requires input binding but owner is not player controlled, skipping...");
		// Try redoing this after some time
		if (NumInputBindingAttempts < 4)
		{
			FTimerDelegate ReattemptDelegate;
			ReattemptDelegate.BindUObject(this, &UActionTrigger_InputRequirement::SetupTrigger, Priority);
			GetCharacterInfo()->CharacterOwner->GetWorld()->GetTimerManager().SetTimer(ReattemptHandle, ReattemptDelegate, 0.5f, false);
			NumInputBindingAttempts++;
		}
		return;
	}

	UInputBufferSubsystem* InputBuffer = GetCharacterInfo()->CharacterOwner->GetInputBuffer();
	if (InputBuffer)
		InputRequirement.BindInput(InputBuffer, this, Priority);
}

void UActionTrigger_InputRequirement::CleanupTrigger()
{
	if (!GetCharacterInfo()->CharacterOwner->GetInputBuffer())
	{
		ACTIONSYSTEM_VLOG(GetCharacterInfo()->CharacterOwner.Get(), Error, "Action requires input binding but owner is not player controlled, skipping...");
		return;
	}

	UInputBufferSubsystem* InputBuffer = GetCharacterInfo()->CharacterOwner->GetInputBuffer();

	if (InputBuffer)
		InputRequirement.UnbindInput(InputBuffer, this);
}

void UActionTrigger_InputRequirement::ActivationInputReceived()
{
	if (GetCharacterInfo()->ActionSystemComponent->TryActivateAbilityByClass(OwnerAction->GetActionData()))
	{
		if (!GetCharacterInfo()->CharacterOwner->GetInputBuffer())
		{
			ACTIONSYSTEM_VLOG(GetCharacterInfo()->CharacterOwner.Get(), Error, "Action requires input binding but owner is not player controlled, skipping...");
			return;
		}
		
		UInputBufferSubsystem* InputBuffer = GetCharacterInfo()->CharacterOwner->GetInputBuffer();
		
		if (InputBuffer)
			InputRequirement.ConsumeInput(InputBuffer);
	}
}

void UActionTrigger_DelayedInputRequirement::Cleanup()
{
	Super::Cleanup();
	GetWorld()->GetTimerManager().ClearTimer(UnbindTimerHandle);
}

void UActionTrigger_DelayedInputRequirement::SetupTrigger(int32 Priority)
{
	GetWorld()->GetTimerManager().ClearTimer(UnbindTimerHandle);
	
	Super::SetupTrigger(Priority);
}

void UActionTrigger_DelayedInputRequirement::CleanupTrigger()
{
	FTimerDelegate UnbindTimerDelegate;
	UnbindTimerDelegate.BindLambda([this] { Super::CleanupTrigger(); });
	GetWorld()->GetTimerManager().SetTimer(UnbindTimerHandle, UnbindTimerDelegate, DelayBeforeUnbind, false);
}

void UActionTrigger_DelayedInputRequirement::ParentActionStarted()
{
	// Ensure that we unbind our inputs
	if (UnbindTimerHandle.IsValid() && GetWorld()->GetTimerManager().GetTimerRemaining(UnbindTimerHandle) > 0.f)
		Super::CleanupTrigger();
	// Clear up the timer since if it was still pending, we manually called it
	GetWorld()->GetTimerManager().ClearTimer(UnbindTimerHandle);
}
