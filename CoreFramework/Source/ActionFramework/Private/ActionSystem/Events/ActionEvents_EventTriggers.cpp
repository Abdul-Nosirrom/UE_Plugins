// Copyright 2023 CoC All rights reserved


#include "ActionSystem/Events/ActionEvents_EventTriggers.h"

#include "InputBufferSubsystem.h"
#include "RadicalCharacter.h"
#include "Debug/ActionSystemLog.h"
#include "Misc/DataValidation.h"

void UActionEvent_InputReleaseTrigger::Initialize(UGameplayAction* InOwnerAction)
{
	Super::Initialize(InOwnerAction);

	OwnerAction->OnGameplayActionStarted.Event.AddUObject(this, &UActionEvent_InputReleaseTrigger::ExecuteEvent);
	OwnerAction->OnGameplayActionEnded.Event.AddUObject(this, &UActionEvent_InputReleaseTrigger::CleanupEvent);
	ButtonInputDelegate.Delegate.BindDynamic(this, &UActionEvent_InputReleaseTrigger::OnInputReleased);
}

void UActionEvent_InputReleaseTrigger::ExecuteEvent()
{
	if (auto IB = GetCharacterInfo()->CharacterOwner->GetInputBuffer())
	{
		if (auto Input = OwnerAction->GetInputRequirement())
		{
			IB->BindAction(ButtonInputDelegate, Input->ButtonInput, EBufferTriggerEvent::TRIGGER_Release, true, 255);
		}
	}
}

void UActionEvent_InputReleaseTrigger::CleanupEvent()
{
	if (auto IB = GetCharacterInfo()->CharacterOwner->GetInputBuffer())
	{
		IB->UnbindAction(ButtonInputDelegate);
	}
}

void UActionEvent_InputReleaseTrigger::OnInputReleased(const FInputActionValue& Value, float ElapsedTime)
{
	if (EventToInvoke.IsValid())
	{
		EventToInvoke.GetEvent(OwnerAction.Get())->Execute();
	}
	else
	{
		ACTIONSYSTEM_VLOG(GetCharacterInfo()->CharacterOwner.Get(), Error, "Invalid event to invoke on action event!")
	}
}

#define LOCTEXT_NAMESPACE "EventValidation"
#if WITH_EDITOR 
EDataValidationResult UActionEvent_InputReleaseTrigger::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	if (Result == EDataValidationResult::Invalid) return Result;
	if (!EventToInvoke.IsValid())
	{
		Context.AddError(LOCTEXT("NoEvent", "No event to invoke is set!"));
		Result = EDataValidationResult::Invalid;
	}
	else if (auto Action = GetTypedOuter<UGameplayAction>())
	{
		if (auto Input = Action->GetInputRequirement())
		{
			if (Input->InputType != EActionInputType::Button || !Input->ButtonInput.IsValid())
			{
				Context.AddError(LOCTEXT("BadInput", "Button input is null or not set!"));
				Result = EDataValidationResult::Invalid;
			}
		}
		else
		{
			Context.AddError(LOCTEXT("NoInput", "Owner action must have an input requirement!"));
			Result = EDataValidationResult::Invalid;
		}
	}
	else
	{
		Context.AddError(LOCTEXT("WrongOuter", "Outer must be a gameplay action!"));
		Result = EDataValidationResult::Invalid;
	}

	return Result;
}
#endif
#undef LOCTEXT_NAMESPACE


void UActionEvent_InputPressedTrigger::Initialize(UGameplayAction* InOwnerAction)
{
	Super::Initialize(InOwnerAction);
	OwnerAction->OnGameplayActionStarted.Event.AddUObject(this, &UActionEvent_InputPressedTrigger::ExecuteEvent);
	OwnerAction->OnGameplayActionEnded.Event.AddUObject(this, &UActionEvent_InputPressedTrigger::CleanupEvent);
	ButtonInputDelegate.Delegate.BindDynamic(this, &UActionEvent_InputPressedTrigger::OnInputPressed);

	for (auto ConsumeEvent : ConsumeInputWhenInvoked)
	{
		if (!bShouldAutoConsume && ConsumeEvent.IsValid())
		{
			ConsumeEvent.GetEvent(InOwnerAction)->Event.AddUObject(this, &UActionEvent_InputPressedTrigger::TryConsumeInput);
		}
	}
}

void UActionEvent_InputPressedTrigger::ExecuteEvent()
{
	if (auto IB = GetCharacterInfo()->CharacterOwner->GetInputBuffer())
	{
		IB->BindAction(ButtonInputDelegate, Button, EBufferTriggerEvent::TRIGGER_Press, bShouldAutoConsume, 255);
	}
}

void UActionEvent_InputPressedTrigger::CleanupEvent()
{
	if (auto IB = GetCharacterInfo()->CharacterOwner->GetInputBuffer())
	{
		IB->UnbindAction(ButtonInputDelegate);
	}
}

void UActionEvent_InputPressedTrigger::OnInputPressed(const FInputActionValue& Value, float ElapsedTime)
{
	if (EventToInvoke.IsValid())
	{
		EventToInvoke.GetEvent(OwnerAction.Get())->Execute();
	}
	else
	{
		ACTIONSYSTEM_VLOG(GetCharacterInfo()->CharacterOwner.Get(), Error, "Invalid event to invoke on action event!")
	}
}

void UActionEvent_InputPressedTrigger::TryConsumeInput()
{
	if (auto IB = GetCharacterInfo()->CharacterOwner->GetInputBuffer())
	{
		IB->ConsumeInput(Button);
	}
}

#define LOCTEXT_NAMESPACE "EventValidation"
#if WITH_EDITOR 
EDataValidationResult UActionEvent_InputPressedTrigger::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	if (Result == EDataValidationResult::Invalid) return Result;
	if (!EventToInvoke.IsValid())
	{
		Context.AddError(LOCTEXT("NoEvent", "No event to invoke is set!"));
		Result = EDataValidationResult::Invalid;
	}
	else if (!Button.IsValid())
	{
		Context.AddError(LOCTEXT("BadInput", "Button input is null or not set!"));
		Result = EDataValidationResult::Invalid;
	}
	else
	{
		Context.AddError(LOCTEXT("WrongOuter", "Outer must be a gameplay action!"));
		Result = EDataValidationResult::Invalid;
	}

	return Result;
}
#endif
#undef LOCTEXT_NAMESPACE