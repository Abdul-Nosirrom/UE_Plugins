// Copyright 2023 CoC All rights reserved

#pragma once

#include "CoreMinimal.h"
#include "ActionSystem/ActionScript.h"
#include "ActionEvent_TimerEvent.generated.h"

/**
 * 
 */
UCLASS(DisplayName="Timer Based Event")
class ACTIONFRAMEWORK_API UActionEvent_TimerEvent : public UActionEvent
{
	GENERATED_BODY()
protected:
	UActionEvent_TimerEvent()
	{ DECLARE_ACTION_EVENT("Timer Based Event", true) }

	UPROPERTY(EditDefaultsOnly)
	float Time;
	UPROPERTY(EditDefaultsOnly)
	bool bLoop;
	UPROPERTY(EditDefaultsOnly, Instanced)
	UActionEvent* EventToInvoke;

	FTimerHandle EventTimerHandle;

	virtual void Initialize(UGameplayAction* InOwnerAction) override;
	virtual void Cleanup() override;
	virtual void ExecuteEvent() override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif 
};
