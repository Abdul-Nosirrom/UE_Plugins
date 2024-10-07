// Copyright 2023 CoC All rights reserved

#pragma once

#include "CoreMinimal.h"
#include "ActionSystem/ActionScript.h"
#include "ActionEvent_ConditionalEvent.generated.h"

/**
 * 
 */
UCLASS(DisplayName="Conditional Event")
class ACTIONFRAMEWORK_API UActionEvent_ConditionalEvent : public UActionEvent
{
	GENERATED_BODY()
protected:
	UActionEvent_ConditionalEvent()
	{ DECLARE_ACTION_EVENT("Conditional Event", false) }

	UPROPERTY(EditDefaultsOnly, Instanced)
	UActionCondition* EventCondition;
	UPROPERTY(EditDefaultsOnly, Instanced)
	UActionEvent* EventToInvoke;
	
	virtual void Initialize(UGameplayAction* InOwnerAction) override;
	virtual void ExecuteEvent() override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif 
};
