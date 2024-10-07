// Copyright 2023 CoC All rights reserved

#pragma once

#include "CoreMinimal.h"
#include "ActionSystem/ActionScript.h"
#include "ActionSystem/GameplayActionTypes.h"
#include "ActionTrigger_InputRequirement.generated.h"

// NOTE: For input now that we do it this way, can be nice to setup different input configs. For example "Modifier Held + Button" for L2 held + Square for example

class UMotionAction;
enum EBufferTriggerEvent : int;
enum EDirectionalSequenceOrder : int;

UENUM(BlueprintType)
enum class EActionInputType : uint8 
{
	Button,
	ButtonSequence,
	Directional,
	DirectionButtonSequence
};

USTRUCT(BlueprintType)
struct FActionInputRequirement
{
	GENERATED_BODY()

public:
	
	// BEGIN Inputs
	UPROPERTY(Category=Input, EditAnywhere, BlueprintReadWrite)
	EActionInputType InputType = EActionInputType::Button;
	
	UPROPERTY(Category=Input, EditAnywhere, BlueprintReadWrite, meta=(EditCondition="InputType==EActionInputType::Button"))
	TEnumAsByte<EBufferTriggerEvent> TriggerEvent;
	UPROPERTY(Category=Input, EditAnywhere, BlueprintReadWrite, meta=(EditCondition="TriggerEvent==EBufferTriggerEvent::TRIGGER_Hold", EditConditionHides))
	float RequiredElapsedTime;
	UPROPERTY(Category=Input, EditAnywhere, BlueprintReadWrite, meta=(EditCondition="InputType==EActionInputType::DirectionButtonSequence", EditConditionHides))
	TEnumAsByte<EDirectionalSequenceOrder> SequenceOrder;
	UPROPERTY(Category=Input, EditAnywhere, BlueprintReadWrite, meta=(EditCondition="InputType==EActionInputType::ButtonSequence", EditConditionHides))
	bool bFirstInputIsHold;
	UPROPERTY(Category=Input, EditAnywhere, BlueprintReadWrite, meta=(EditCondition="InputType==EActionInputType::ButtonSequence && !bFirstInputIsHold", EditConditionHides))
	bool bConsiderOrder;
	
	UPROPERTY(Category=Input, EditAnywhere, BlueprintReadWrite, meta=(EditCondition="InputType!=EActionInputType::Directional", EditConditionHides, GameplayTagFilter="Input.Button"))
	FGameplayTag ButtonInput;
	UPROPERTY(Category=Input, EditAnywhere, BlueprintReadWrite, meta=(EditCondition="InputType==EActionInputType::ButtonSequence", EditConditionHides, GameplayTagFilter="Input.Button"))
	FGameplayTag SecondButtonInput;
	UPROPERTY(Category=Input, EditAnywhere, BlueprintReadWrite, meta=(EditCondition="InputType==EActionInputType::Directional || InputType==EActionInputType::DirectionButtonSequence", EditConditionHides, GameplayTagFilter="Input.Directional"))
	FGameplayTag DirectionalInput;
	// END Inputs

	void BindInput(UInputBufferSubsystem* InputBuffer, class UActionTrigger_InputRequirement* Action, int32 Priority) const;
	void UnbindInput(UInputBufferSubsystem* InputBuffer, UActionTrigger_InputRequirement* Action) const;
	void ConsumeInput(UInputBufferSubsystem* InputBuffer) const;
};

UCLASS(DisplayName="Input Requirement")
class ACTIONFRAMEWORK_API UActionTrigger_InputRequirement : public UActionTrigger
{
	GENERATED_BODY()
             
	friend class UGameplayAction;
	friend class FActionSetEntryDetails;
	friend struct FActionInputRequirement;

public:
	UActionTrigger_InputRequirement()
	DECLARE_ACTION_SCRIPT("Input Requirement");

	UPROPERTY(EditAnywhere, meta=(ShowOnlyInnerProperties))
	FActionInputRequirement InputRequirement;

	uint8 NumInputBindingAttempts = 0;
	FTimerHandle ReattemptHandle;

	virtual void Initialize(UGameplayAction* InOwnerAction) override;
	virtual void Cleanup() override;
	virtual void SetupTrigger(int32 Priority) override;
	virtual void CleanupTrigger() override;

	// BEGIN Input Delegates
	FDirectionalBinding DirectionalInputDelegate;
	FDirectionalSequenceBinding DirectionalAndButtonInputDelegate;
	FButtonSequenceBinding ButtonSequenceInputDelegate;
	FButtonBinding ButtonInputDelegate;
	// END Input Delegates

	void ActivationInputReceived();

	// Used to cleanup bindings since theyre no longer needed
	virtual void ParentActionStarted() {  }

#pragma region Input Binding Response

	UFUNCTION()
	void ActivationButtonInput(const FInputActionValue& Value, float ElapsedTime)
	{
		if (InputRequirement.TriggerEvent == TRIGGER_Hold && ElapsedTime <= InputRequirement.RequiredElapsedTime) return;
		ActivationInputReceived();
	}
	UFUNCTION()
	void ActivationButtonSequence(const FInputActionValue& Value1, const FInputActionValue& Value2)
	{
		ActivationInputReceived();
	}
	UFUNCTION()
	void ActivationDirectionalInput()
	{
		ActivationInputReceived();
	}
	UFUNCTION()
	void ActivationDirectionalActionInput(const FInputActionValue& Value, float ElapsedTime)
	{
		ActivationInputReceived();
	}
	
#pragma endregion

};

/// @brief	Functions just like regular input requirements, except we add a delay before we unbind during cleanup
UCLASS(DisplayName="Delayed Input Requirement")
class UActionTrigger_DelayedInputRequirement : public UActionTrigger_InputRequirement
{
	GENERATED_BODY()

protected:
	UActionTrigger_DelayedInputRequirement()
	{ DECLARE_ACTION_SCRIPT("Input Requirement W/ Delayed Unbind") }


	UPROPERTY(EditDefaultsOnly, meta=(UIMin=0, ClampMin=0))
	float DelayBeforeUnbind = 0.5f;

	FTimerHandle UnbindTimerHandle;

	virtual void Cleanup() override;
	virtual void SetupTrigger(int32 Priority) override;
	virtual void CleanupTrigger() override;

	virtual void ParentActionStarted() override;
};
