// Copyright 2023 CoC All rights reserved


#include "ActionSystem/Events/ActionEvent_ExitBehavior.h"

#include "InputBufferSubsystem.h"
#include "RadicalCharacter.h"
#include "ActionSystem/GameplayAction.h"
#include "ActionSystem/Triggers/ActionTrigger_InputRequirement.h"
#include "Components/ActionSystemComponent.h"
#include "Misc/DataValidation.h"

void UActionEvent_ExitBehavior::Initialize(UGameplayAction* InOwnerAction)
{
	Super::Initialize(InOwnerAction);

	OwnerAction->OnGameplayActionStarted.Event.AddUObject(this, &UActionEvent_ExitBehavior::Setup);
	OwnerAction->OnGameplayActionEnded.Event.AddUObject(this, &UActionEvent_ExitBehavior::Cleanup);

	ReleasedInputDelegate.Delegate.BindDynamic(this, &UActionEvent_ExitBehavior::MaybeExitActionFromInput);
}

void UActionEvent_ExitBehavior::Setup()
{
	if (Reason == EActionAutoExitReason::TagsAdded)
	{
		GetCharacterInfo()->ActionSystemComponent->OnGameplayTagAddedDelegate.AddUniqueDynamic(this, &UActionEvent_ExitBehavior::MaybeExitAction);
	}
	else
	{
		auto InputReq = OwnerAction->GetInputRequirement();
		GetCharacterInfo()->CharacterOwner->GetInputBuffer()->BindAction(ReleasedInputDelegate, InputReq->ButtonInput, EBufferTriggerEvent::TRIGGER_Release, true);
	}
}

void UActionEvent_ExitBehavior::Cleanup()
{
	if (Reason == EActionAutoExitReason::TagsAdded)
	{
		GetCharacterInfo()->ActionSystemComponent->OnGameplayTagAddedDelegate.RemoveDynamic(this, &UActionEvent_ExitBehavior::MaybeExitAction);
	}
	else if (GetCharacterInfo()->CharacterOwner->GetInputBuffer())
	{
		GetCharacterInfo()->CharacterOwner->GetInputBuffer()->UnbindAction(ReleasedInputDelegate);
	}
}

void UActionEvent_ExitBehavior::MaybeExitAction(FGameplayTagContainer AddedTags)
{
	if (AddedTags.HasAny(ExitWhenAnyTagsAdded))
	{
		if (OwnerAction->IsActive()) GetCharacterInfo()->ActionSystemComponent->CancelAction(OwnerAction->GetActionData());
	}
}

void UActionEvent_ExitBehavior::MaybeExitActionFromInput(const FInputActionValue& Value, float ElapsedTime)
{
	if (OwnerAction->IsActive())
	{
		OwnerAction->SetCanBeCanceled(true);
		OwnerAction->CancelAction();
	}
}

#define LOCTEXT_NAMESPACE "ExitBehaviorValidation"
#if WITH_EDITOR 
EDataValidationResult UActionEvent_ExitBehavior::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	if (Reason == EActionAutoExitReason::TagsAdded)
		return Result;

	// Else check if the owner has an input requirement trigger, and if its a button
	if (auto Action = GetTypedOuter<UGameplayAction>())
	{
		if (auto Trigger = Cast<UActionTrigger_InputRequirement>(Action->GetActionTrigger()))
		{
			if (Trigger->InputRequirement.InputType != EActionInputType::Button)
			{
				Context.AddError(LOCTEXT("BadInput", "Input release exit behavior requires a button input as the trigger"));
				Result = EDataValidationResult::Invalid;
			}
		}
		else
		{
			Context.AddError(LOCTEXT("NoInput", "Input release exit behavior requires the trigger to be an InputReq"));
			Result = EDataValidationResult::Invalid;
		}
	}
	else
	{
		// Our outer isnt an action? Thats bad...
		Context.AddError(LOCTEXT("BadOuter", "Outer is not a GameplayAction, have no access to InputReq"));
		Result = EDataValidationResult::Invalid;
	}

	return Result;
}
#endif
#undef LOCTEXT_NAMESPACE