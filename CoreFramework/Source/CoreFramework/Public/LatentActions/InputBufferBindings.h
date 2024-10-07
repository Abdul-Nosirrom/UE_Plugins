// Copyright 2023 CoC All rights reserved

#pragma once

#include "CoreMinimal.h"
#include "InputData.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "InputBufferBindings.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FBPInputBufferBindingUnbound);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FBPInputBufferBindingExecuted);

// Input Action Registration
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FLatentBPInputActionEventSignature, const FInputActionValue&, Value, float, ElapsedTime);

// Input Action Sequence
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FLatentBPInputActionSequenceSignature, const FInputActionValue&, ValueOne, const FInputActionValue&, ValueTwo);

// Directional Action Registration
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FLatentBPDirectionalActionSignature);

// Directional Action & Input Action Serquence
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FLatentBPDirectionalAndActionSequenceSignature, const FInputActionValue&, ActionValue, float, ActionElapsedTime);


UENUM(BlueprintType)
enum class EUnbindBehavior : uint8
{
	Never,
	AfterXConsume,
	AfterTime
};

UCLASS()
class COREFRAMEWORK_API UInputBufferBindings : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable)
	FBPInputBufferBindingUnbound Unbound;

	UPROPERTY()
	APlayerController* Controller;
	UPROPERTY()
	FGameplayTag InputOne;
	UPROPERTY()
	FGameplayTag InputTwo;
	UPROPERTY()
	FGameplayTag DirectionalInput;
	
	EUnbindBehavior UnbindBehavior;
	
	/* Info */
	int ConsumeCount;
	int CurrentConsumeAmount;
	float UnbindAfterTime;
	int Priority;

protected:
	
	virtual void Activate() override;

	UInputBufferSubsystem* GetInputBuffer() const;
	
	/* Bindings */
	void BindInternal();
	void UnBindInternal();

	virtual void BindToBuffer() {}
	virtual void UnbindFromBuffer() {}
	
	/* Input Buffer Events*/

	FDirectionalBinding DirectionalInputDelegate;
	UFUNCTION()
	void DirectionalInputBinding();

	FDirectionalSequenceBinding DirectionalAndButtonInputDelegate;
	UFUNCTION()
	void DirectionalAndButtonInputBinding(const FInputActionValue& ActionValue, float ActionElapsedTime);

	FButtonSequenceBinding ButtonSequenceInputDelegate;
	UFUNCTION()
	void ButtonSequenceInputBinding(const FInputActionValue& ValueOne, const FInputActionValue& ValueTwo);

	FButtonBinding ButtonInputDelegate;
	UFUNCTION()
	void ButtonInputBinding(const FInputActionValue& Value, float ElapsedTime);

	virtual void InputFound(const FInputActionValue& ValueOne, float ElapsedTime, const FInputActionValue& ValueTwo);
};

UCLASS()
class COREFRAMEWORK_API UButtonBufferBindings : public UInputBufferBindings
{
	GENERATED_BODY()

public:

	UPROPERTY(BlueprintAssignable)
	FLatentBPInputActionEventSignature InputEvent;

	UFUNCTION(Category=Input, BlueprintCallable, meta=(BlueprintInternalUseOnly="true", GameplayTagFilter="Input"))
	static UButtonBufferBindings* ListenForButtonInput(
		APlayerController* PlayerController,
		const FGameplayTag& Input,
		EBufferTriggerEvent TriggerEvent,
		float HoldTimeRequired,
		EUnbindBehavior UnbindBehavior,
		int ConsumeCount = 1,
		float UnbindAfterTime = 1.f,
		int Priority = 0);
	
protected:
	
	/* Button Binding */
	float HoldTime;
	EBufferTriggerEvent TriggerEvent;

	virtual void InputFound(const FInputActionValue& ValueOne, float ElapsedTime, const FInputActionValue& ValueTwo) override;

	virtual void BindToBuffer() override;
	virtual void UnbindFromBuffer() override;
};

UCLASS()
class COREFRAMEWORK_API UButtonSequenceBufferBindings : public UInputBufferBindings
{
	GENERATED_BODY()
public:

	UPROPERTY(BlueprintAssignable)
	FLatentBPInputActionSequenceSignature InputEvent;

	UFUNCTION(Category=Input, BlueprintCallable, meta=(BlueprintInternalUseOnly="true", GameplayTagFilter="Input"))
	static UButtonSequenceBufferBindings* ListenForButtonSequenceInput(
		APlayerController* PlayerController,
		const FGameplayTag& FirstInput,
		const FGameplayTag& SecondInput,
		bool bFirstInputIsHold,
		bool bConsiderOrder,
		EUnbindBehavior UnbindBehavior,
		int ConsumeCount = 1,
		float UnbindAfterTime = 1.f,
		int Priority = 0);
	
protected:
	
	/* Button Sequence Binding */
	bool bFirstInputIsHold;
	bool bConsiderOrder;

	virtual void InputFound(const FInputActionValue& ValueOne, float ElapsedTime, const FInputActionValue& ValueTwo) override;

	virtual void BindToBuffer() override;
	virtual void UnbindFromBuffer() override;
};

UCLASS()
class COREFRAMEWORK_API UDirectionalBufferBindings : public UInputBufferBindings
{
	GENERATED_BODY()

public:

	UPROPERTY(BlueprintAssignable)
	FLatentBPDirectionalActionSignature InputEvent;

	UFUNCTION(Category=Input, BlueprintCallable, meta=(BlueprintInternalUseOnly="true", GameplayTagFilter="Input"))
	static UDirectionalBufferBindings* ListenForDirectionalInput(
		APlayerController* PlayerController,
		const FGameplayTag& DirectionInput,
		EUnbindBehavior UnbindBehavior,
		int ConsumeCount = 1,
		float UnbindAfterTime = 1.f,
		int Priority = 0);
	
protected:
	
	virtual void InputFound(const FInputActionValue& ValueOne, float ElapsedTime, const FInputActionValue& ValueTwo) override;

	virtual void BindToBuffer() override;
	virtual void UnbindFromBuffer() override;
};

UCLASS()
class COREFRAMEWORK_API UDirectionalAndButtonBufferBindings : public UInputBufferBindings
{
	GENERATED_BODY()

public:

	UPROPERTY(BlueprintAssignable)
	FLatentBPDirectionalAndActionSequenceSignature InputEvent;

	UFUNCTION(Category=Input, BlueprintCallable, meta=(BlueprintInternalUseOnly="true", GameplayTagFilter="Input"))
	static UDirectionalAndButtonBufferBindings* ListenForDirectionalAndButtonInput(
		APlayerController* PlayerController,
		const FGameplayTag& DirectionInput,
		const FGameplayTag& ButtonInput,
		EBufferTriggerEvent TriggerEvent,
		float HoldTimeRequired,
		EDirectionalSequenceOrder SequenceOrder,
		EUnbindBehavior UnbindBehavior,
		int ConsumeCount = 1,
		float UnbindAfterTime = 1.f,
		int Priority = 0);
	
protected:
	
	float HoldTime;
	EBufferTriggerEvent TriggerEvent;
	EDirectionalSequenceOrder SequenceOrder;

	virtual void InputFound(const FInputActionValue& ValueOne, float ElapsedTime, const FInputActionValue& ValueTwo) override;

	virtual void BindToBuffer() override;
	virtual void UnbindFromBuffer() override;
};