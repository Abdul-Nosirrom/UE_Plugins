// Copyright 2023 CoC All rights reserved


#include "ActionSystem/Events/ActionEvent_CancelBehavior.h"

#include "TimerManager.h"
#include "ActionSystem/GameplayAction.h"
#include "Components/ActionSystemComponent.h"


void UActionEvent_CancelBehavior::Initialize(UGameplayAction* InOwnerAction)
{
	Super::Initialize(InOwnerAction);

	OwnerAction->OnGameplayActionStarted.Event.AddUObject(this, &UActionEvent_CancelBehavior::OnActionStarted);

	if (CancelBehavior.Behavior == EActionCancelBehavior::AfterTagsAdded)
	{
		GetCharacterInfo()->ActionSystemComponent->OnGameplayTagAddedDelegate.AddDynamic(this, &UActionEvent_CancelBehavior::OnCancelTagsMaybeAdded);
	}
}

void UActionEvent_CancelBehavior::Cleanup()
{
	Super::Cleanup();
	GetWorld()->GetTimerManager().ClearTimer(CancelTimerHandle);
}

void UActionEvent_CancelBehavior::OnActionStarted()
{
	OwnerAction->SetCanBeCanceled(false);

	if (CancelBehavior.Behavior == EActionCancelBehavior::AfterTime)
	{
		FTimerDelegate CancelTimerDelegate;
		CancelTimerDelegate.BindUObject(this, &UActionEvent_CancelBehavior::SetCanCancel);
		GetWorld()->GetTimerManager().SetTimer(CancelTimerHandle, CancelTimerDelegate, CancelBehavior.TimeUntilCanBeCancelled, false);
	}
}

void UActionEvent_CancelBehavior::OnCancelTagsMaybeAdded(FGameplayTagContainer AddedTags)
{
	if (CancelBehavior.AnyTagsAdded.HasAny(AddedTags)) SetCanCancel();
}

void UActionEvent_CancelBehavior::SetCanCancel()
{
	OwnerAction->SetCanBeCanceled(true);
}
