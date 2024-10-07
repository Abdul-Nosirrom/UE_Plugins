// Copyright 2023 CoC All rights reserved

#pragma once

#include "CoreMinimal.h"
#include "ActionSystem/Triggers/ActionTrigger_InputRequirement.h"
#include "ActionEvents_EventTriggers.generated.h"


/// @brief	Allows binding an event on an action to an input trigger.
UCLASS(DisplayName="On Input Released")
class ACTIONFRAMEWORK_API UActionEvent_InputReleaseTrigger : public UActionEvent
{
	GENERATED_BODY()
public:
	UActionEvent_InputReleaseTrigger()
	{ DECLARE_ACTION_EVENT("On Input Released", true); }

	UPROPERTY(EditAnywhere)
	FActionEventWrapper EventToInvoke;

	// BEGIN Input Delegates
	FButtonBinding ButtonInputDelegate;
	// END Input Delegates

	virtual void Initialize(UGameplayAction* InOwnerAction) override;
	virtual void ExecuteEvent() override;
	void CleanupEvent();

	UFUNCTION()
	void OnInputReleased(const FInputActionValue& Value, float ElapsedTime);


#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif 
};

/// @brief	Allows binding an event on an action to an input trigger.
UCLASS(DisplayName="On Input Pressed")
class ACTIONFRAMEWORK_API UActionEvent_InputPressedTrigger : public UActionEvent
{
	GENERATED_BODY()
public:
	UActionEvent_InputPressedTrigger()
	{ DECLARE_ACTION_EVENT("On Input Pressed", true); }

	UPROPERTY(EditAnywhere)
	FActionEventWrapper EventToInvoke;
	UPROPERTY(EditAnywhere, meta=(GameplayTagFilter="Input.Button"))
	FGameplayTag Button;
	UPROPERTY(EditAnywhere)
	bool bShouldAutoConsume = true;
	UPROPERTY(EditAnywhere, meta=(TitleProperty="Event", EditCondition="!bShouldAutoConsume", EditConditionHides))
	TArray<FActionEventWrapper> ConsumeInputWhenInvoked;
	
	// BEGIN Input Delegates
	FButtonBinding ButtonInputDelegate;
	// END Input Delegates

	virtual void Initialize(UGameplayAction* InOwnerAction) override;
	virtual void ExecuteEvent() override;
	void CleanupEvent();

	UFUNCTION()
	void OnInputPressed(const FInputActionValue& Value, float ElapsedTime);

	void TryConsumeInput();

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif 
};
