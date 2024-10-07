// Copyright 2023 CoC All rights reserved


#include "ActionSystem/Events/ActionEvent_ConditionalEvent.h"

void UActionEvent_ConditionalEvent::Initialize(UGameplayAction* InOwnerAction)
{
	Super::Initialize(InOwnerAction);

	if (EventCondition) EventCondition->Initialize(InOwnerAction);
	if (EventToInvoke) EventToInvoke->Initialize(InOwnerAction);
}

void UActionEvent_ConditionalEvent::ExecuteEvent()
{
	if (EventCondition && EventCondition->DoesConditionPass())
	{
		EventToInvoke->ExecuteEvent();
	}
}

#if WITH_EDITOR 
void UActionEvent_ConditionalEvent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (EventToInvoke) EventToInvoke->bIgnoreEvent = true;
}
#endif
