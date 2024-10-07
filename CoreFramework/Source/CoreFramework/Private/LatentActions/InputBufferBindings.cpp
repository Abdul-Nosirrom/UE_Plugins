// Copyright 2023 CoC All rights reserved


#include "LatentActions/InputBufferBindings.h"

#include "InputBufferSubsystem.h"

/*--------------------------------------------------------------------------------------------------------------
* Base
*--------------------------------------------------------------------------------------------------------------*/

void UInputBufferBindings::Activate()
{
	Super::Activate();

	if (!Controller || !GetInputBuffer()) return;
	
	BindInternal();

	CurrentConsumeAmount = 0;

	if (UnbindBehavior == EUnbindBehavior::AfterTime)
	{
		FTimerDelegate UnbindTimer;
		FTimerHandle UnbindTimerHandler;
		UnbindTimer.BindLambda([this]
		{
			UnBindInternal();
		});
		Controller->GetWorld()->GetTimerManager().SetTimer(UnbindTimerHandler, UnbindTimer, UnbindAfterTime, false);
	}
}

UInputBufferSubsystem* UInputBufferBindings::GetInputBuffer() const
{
	if (Controller)
	{
		return Controller->GetLocalPlayer()->GetSubsystem<UInputBufferSubsystem>();
	}
	return nullptr;
}

void UInputBufferBindings::BindInternal()
{
	ButtonInputDelegate.Delegate.BindDynamic(this, &UInputBufferBindings::ButtonInputBinding);
	DirectionalInputDelegate.Delegate.BindDynamic(this, &UInputBufferBindings::DirectionalInputBinding);
	DirectionalAndButtonInputDelegate.Delegate.BindDynamic(this, &UInputBufferBindings::DirectionalAndButtonInputBinding);
	ButtonSequenceInputDelegate.Delegate.BindDynamic(this, &UInputBufferBindings::ButtonSequenceInputBinding);

	BindToBuffer();
}

void UInputBufferBindings::UnBindInternal()
{
	// Unbind them, to prevent attempting to rebind them OnRootStart
	ButtonInputDelegate.Delegate.Unbind();
	DirectionalInputDelegate.Delegate.Unbind();
	DirectionalAndButtonInputDelegate.Delegate.Unbind();
	ButtonSequenceInputDelegate.Delegate.Unbind();

	UnbindFromBuffer();

	// We're done
	Unbound.Broadcast();
}

void UInputBufferBindings::DirectionalInputBinding()
{
	InputFound(FInputActionValue(), 0.f, FInputActionValue());
}

void UInputBufferBindings::DirectionalAndButtonInputBinding(const FInputActionValue& ActionValue,
	float ActionElapsedTime)
{
	InputFound(ActionValue, ActionElapsedTime, FInputActionValue());
}

void UInputBufferBindings::ButtonSequenceInputBinding(const FInputActionValue& ValueOne,
	const FInputActionValue& ValueTwo)
{
	InputFound(ValueOne, 0.f, ValueTwo);
}

void UInputBufferBindings::ButtonInputBinding(const FInputActionValue& Value, float ElapsedTime)
{
	InputFound(Value, ElapsedTime, FInputActionValue());
}

void UInputBufferBindings::InputFound(const FInputActionValue& ValueOne, float ElapsedTime,
	const FInputActionValue& ValueTwo)
{
	CurrentConsumeAmount++;
	if (UnbindBehavior == EUnbindBehavior::AfterXConsume && CurrentConsumeAmount >= ConsumeCount)
	{
		UnBindInternal();
	}
}

/*--------------------------------------------------------------------------------------------------------------
* Button Bindings
*--------------------------------------------------------------------------------------------------------------*/


UButtonBufferBindings* UButtonBufferBindings::ListenForButtonInput(APlayerController* PlayerController,
	const FGameplayTag& Input, EBufferTriggerEvent TriggerEvent, float HoldTimeRequired, EUnbindBehavior UnbindBehavior,
	int ConsumeCount, float UnbindAfterTime, int Priority)
{
	auto Node = NewObject<UButtonBufferBindings>();
	if (Node)
	{
		Node->Controller = PlayerController;
		Node->UnbindBehavior = UnbindBehavior;
		Node->ConsumeCount = ConsumeCount;
		Node->UnbindAfterTime = UnbindAfterTime;
		Node->Priority = Priority;

		Node->InputOne = Input;
		
		Node->HoldTime = HoldTimeRequired;
		Node->TriggerEvent = TriggerEvent;
	}
	return Node;
}

void UButtonBufferBindings::InputFound(const FInputActionValue& ValueOne, float ElapsedTime,
	const FInputActionValue& ValueTwo)
{
	InputEvent.Broadcast(ValueOne, ElapsedTime);
	Super::InputFound(ValueOne, ElapsedTime, ValueTwo);
}

void UButtonBufferBindings::BindToBuffer()
{
	if (auto IB = GetInputBuffer())
	{
		IB->BindAction(ButtonInputDelegate, InputOne, TriggerEvent, true, Priority);
	}
}

void UButtonBufferBindings::UnbindFromBuffer()
{
	if (auto IB = GetInputBuffer())
	{
		IB->UnbindAction(ButtonInputDelegate);
	}
}

/*--------------------------------------------------------------------------------------------------------------
* Button Sequence Bindings
*--------------------------------------------------------------------------------------------------------------*/

UButtonSequenceBufferBindings* UButtonSequenceBufferBindings::ListenForButtonSequenceInput(APlayerController* PlayerController,
	const FGameplayTag& InputOne, const FGameplayTag& InputTwo, bool bFirstInputIsHold, bool bConsiderOrder, EUnbindBehavior UnbindBehavior,
	int ConsumeCount, float UnbindAfterTime, int Priority)
{
	auto Node = NewObject<UButtonSequenceBufferBindings>();
	if (Node)
	{
		Node->Controller = PlayerController;
		Node->UnbindBehavior = UnbindBehavior;
		Node->ConsumeCount = ConsumeCount;
		Node->UnbindAfterTime = UnbindAfterTime;
		Node->Priority = Priority;

		Node->InputOne = InputOne;
		Node->InputTwo = InputTwo;

		Node->bFirstInputIsHold = bFirstInputIsHold;
		Node->bConsiderOrder = bConsiderOrder;
	}
	return Node;
}

void UButtonSequenceBufferBindings::InputFound(const FInputActionValue& ValueOne, float ElapsedTime,
	const FInputActionValue& ValueTwo)
{
	InputEvent.Broadcast(ValueOne, ValueTwo);
	Super::InputFound(ValueOne, ElapsedTime, ValueTwo);
}

void UButtonSequenceBufferBindings::BindToBuffer()
{
	if (auto IB = GetInputBuffer())
	{
		IB->BindActionSequence(ButtonSequenceInputDelegate, InputOne, InputTwo, true, bFirstInputIsHold, bConsiderOrder, Priority);
	}
}

void UButtonSequenceBufferBindings::UnbindFromBuffer()
{
	if (auto IB = GetInputBuffer())
	{
		IB->UnbindActionSequence(ButtonSequenceInputDelegate);
	}
}

/*--------------------------------------------------------------------------------------------------------------
* Directional Bindings
*--------------------------------------------------------------------------------------------------------------*/

UDirectionalBufferBindings* UDirectionalBufferBindings::ListenForDirectionalInput(APlayerController* PlayerController,
	const FGameplayTag& DirectionalInput, EUnbindBehavior UnbindBehavior,
	int ConsumeCount, float UnbindAfterTime, int Priority)
{
	auto Node = NewObject<UDirectionalBufferBindings>();
	if (Node)
	{
		Node->Controller = PlayerController;
		Node->UnbindBehavior = UnbindBehavior;
		Node->ConsumeCount = ConsumeCount;
		Node->UnbindAfterTime = UnbindAfterTime;
		Node->Priority = Priority;

		Node->DirectionalInput = DirectionalInput;
	}
	return Node;
}

void UDirectionalBufferBindings::InputFound(const FInputActionValue& ValueOne, float ElapsedTime,
	const FInputActionValue& ValueTwo)
{
	InputEvent.Broadcast();
	Super::InputFound(ValueOne, ElapsedTime, ValueTwo);
}

void UDirectionalBufferBindings::BindToBuffer()
{
	if (auto IB = GetInputBuffer())
	{
		IB->BindDirectionalAction(DirectionalInputDelegate, DirectionalInput, true, Priority);
	}
}

void UDirectionalBufferBindings::UnbindFromBuffer()
{
	if (auto IB = GetInputBuffer())
	{
		IB->UnbindDirectionalAction(DirectionalInputDelegate);
	}
}

/*--------------------------------------------------------------------------------------------------------------
* Directional And Button Bindings
*--------------------------------------------------------------------------------------------------------------*/

UDirectionalAndButtonBufferBindings* UDirectionalAndButtonBufferBindings::ListenForDirectionalAndButtonInput(
	APlayerController* PlayerController, const FGameplayTag& DirectionalInput, const FGameplayTag& ButtonInput,
	EBufferTriggerEvent TriggerEvent, float HoldTimeRequired, EDirectionalSequenceOrder SequenceOrder,
	EUnbindBehavior UnbindBehavior,
	int ConsumeCount, float UnbindAfterTime, int Priority)

{
	auto Node = NewObject<UDirectionalAndButtonBufferBindings>();
	if (Node)
	{
		Node->Controller = PlayerController;
		Node->UnbindBehavior = UnbindBehavior;
		Node->ConsumeCount = ConsumeCount;
		Node->UnbindAfterTime = UnbindAfterTime;
		Node->Priority = Priority;

		Node->InputOne = ButtonInput;
		Node->DirectionalInput = DirectionalInput;
		
		Node->HoldTime = HoldTimeRequired;
		Node->TriggerEvent = TriggerEvent;
		Node->SequenceOrder = SequenceOrder;
	}
	return Node;
}

void UDirectionalAndButtonBufferBindings::InputFound(const FInputActionValue& ValueOne, float ElapsedTime,
	const FInputActionValue& ValueTwo)
{
	InputEvent.Broadcast(ValueOne, ElapsedTime);
	Super::InputFound(ValueOne, ElapsedTime, ValueTwo);
}

void UDirectionalAndButtonBufferBindings::BindToBuffer()
{
	if (auto IB = GetInputBuffer())
	{
		IB->BindDirectionalActionSequence(DirectionalAndButtonInputDelegate, InputOne, DirectionalInput, SequenceOrder, true, Priority);
	}
}

void UDirectionalAndButtonBufferBindings::UnbindFromBuffer()
{
	if (auto IB = GetInputBuffer())
	{
		IB->UnbindDirectionalActionSequence(DirectionalAndButtonInputDelegate);
	}
}
