// Copyright 2023 CoC All rights reserved


#include "ActionSystem/Events/ActionEvent_OnCancelledBy.h"
#include "Components/ActionSystemComponent.h"

void UActionEvent_OnCancelledBy::Initialize(UGameplayAction* InOwnerAction)
{
	Super::Initialize(InOwnerAction);

	if (EventToTrigger) EventToTrigger->Initialize(InOwnerAction);

	// If we were cancelled, then the other action starting ends us, and would fill out PrimaryActionRunning first
	OwnerAction->OnGameplayActionEnded.Event.AddUObject(this, &UActionEvent_OnCancelledBy::ExecuteEvent);
}

void UActionEvent_OnCancelledBy::ExecuteEvent()
{
	if (OwnerAction->GetActionThatCancelledUs() == CancelledBy)
	{
		EventToTrigger->ExecuteEvent();
	}
}

void UActionEvent_PreviousActionCheck::Initialize(UGameplayAction* InOwnerAction)
{
	Super::Initialize(InOwnerAction);

	if (EventToTrigger) EventToTrigger->Initialize(InOwnerAction);
}

void UActionEvent_PreviousActionCheck::ExecuteEvent()
{
	if (!EventToTrigger) return;
	
	if (auto PA = CurrentActorInfo->ActionSystemComponent->GetPreviousPrimaryAction())
	{
		if (PA->GetActionData() == PreviousAction) EventToTrigger->ExecuteEvent();
	}
}

#if WITH_EDITOR
void UActionEvent_OnCancelledBy::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (EventToTrigger) EventToTrigger->bIgnoreEvent = true;
}

FString UActionEvent_OnCancelledBy::GetEditorFriendlyName() const
{
	FString Name = "";
	if (EventToTrigger) Name.Append(EventToTrigger->EditorFriendlyName);
	Name.Append(" On Cancelled By ");
	if (CancelledBy) Name.Append(CancelledBy.GetName());

	return Name;
}

void UActionEvent_PreviousActionCheck::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	
	if (EventToTrigger) EventToTrigger->bIgnoreEvent = true;
}

FString UActionEvent_PreviousActionCheck::GetEditorFriendlyName() const
{
	FString Name = "";
	if (EventToTrigger) Name.Append(EventToTrigger->EditorFriendlyName);
	Name.Append(" If Previous Action Is ");
	if (PreviousAction) Name.Append(PreviousAction.GetName());

	return Name;
}
#endif
