// Copyright 2023 CoC All rights reserved


#include "ActionSystem/Events/ActionEvent_TimerEvent.h"

#include "TimerManager.h"
#include "ActionSystem/GameplayAction.h"

void UActionEvent_TimerEvent::Initialize(UGameplayAction* InOwnerAction)
{
	Super::Initialize(InOwnerAction);

	if (EventToInvoke) EventToInvoke->Initialize(InOwnerAction);

	// If the outer isnt a gameplay action, then we must've been placed in an event container that'll invoke Execute itself
	if (GetOuter()->IsA(UGameplayAction::StaticClass()))
		OwnerAction->OnGameplayActionStarted.Event.AddUObject(this, &UActionEvent_TimerEvent::ExecuteEvent);
	OwnerAction->OnGameplayActionEnded.Event.AddLambda([this]
	{
		GetWorld()->GetTimerManager().ClearTimer(EventTimerHandle);
	});
}

void UActionEvent_TimerEvent::Cleanup()
{
	Super::Cleanup();
	GetWorld()->GetTimerManager().ClearTimer(EventTimerHandle);
}

void UActionEvent_TimerEvent::ExecuteEvent()
{
	FTimerDelegate EventDelegate;
	EventDelegate.BindLambda([this]
	{
		if (OwnerAction->IsActive() && EventToInvoke) EventToInvoke->ExecuteEvent();
	});

	GetWorld()->GetTimerManager().SetTimer(EventTimerHandle, EventDelegate, Time, bLoop);
}

#if WITH_EDITOR 
void UActionEvent_TimerEvent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (EventToInvoke) EventToInvoke->bIgnoreEvent = true;
}
#endif
