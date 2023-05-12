// Fill out your copyright notice in the Description page of Project Settings.


#include "StateMachineDefinitions/Action_TransitionStateInstance.h"

#include "InputBufferSubsystem.h"
#include "Actors/RadicalCharacter.h"
#include "Components/ActionSystemComponent.h"
#include "Engine/LocalPlayer.h"
#include "StateMachineDefinitions/Action_CoreStateInstance.h"
#include "StateMachineDefinitions/Action_CoreStateMachineInstance.h"
#include "StateMachineDefinitions/Action_MoveSetSMInstance.h"
#include "StateMachineDefinitions/GameplayStateMachine_Base.h"

#define PRINT(Color, Text) GEngine->AddOnScreenDebugMessage(-1, 2.f, Color, Text);

DECLARE_CYCLE_STAT(TEXT("On Transition Initialized"), STAT_TransitionInitialized, STATGROUP_ActionSystemComp)
DECLARE_CYCLE_STAT(TEXT("Setup Bindings"), STAT_SetupBindings, STATGROUP_ActionSystemComp)
DECLARE_CYCLE_STAT(TEXT("Bind Inputs"), STAT_BindInputs, STATGROUP_ActionSystemComp)

UAction_TransitionStateInstance::UAction_TransitionStateInstance()
{
#if WITH_EDITOR
	/* Establish connection rules */
	/////////////////////////////////////
	/// 1.) CoreState -> CoreState
	/// 2.) CoreState -> CoreStateMachine
	/// 3.) CoreStateMachine -> CoreState
	/// 4.) SubState -> SubState
	/// 6.) MoveSet -> MoveSet
	/// 6.) AnyState -> CoreState
	/// 7.) AnyState -> CoreStateMachine
	/// 8.) AnyState -> SubState
	////////////////////////////////////

	/* Define Base Data */
	FSMStateClassRule CoreStateRule;
	FSMStateClassRule CoreStateMachineRule;
	FSMStateClassRule MoveSetRule;
	
	FSMStateClassRule AnyStateRule;

	// Setup Core State
	CoreStateRule.StateClass = TSoftClassPtr<USMStateInstance_Base>(UAction_CoreStateInstance::StaticClass());
	CoreStateRule.bIncludeChildren = true;

	// Setup Core State Machine
	CoreStateMachineRule.StateClass = TSoftClassPtr<USMStateInstance_Base>(UAction_CoreStateMachineInstance::StaticClass());
	CoreStateMachineRule.bIncludeChildren = true;

	// Setup Move-set
	MoveSetRule.StateClass = TSoftClassPtr<USMStateInstance_Base>(UAction_MoveSetSMInstance::StaticClass());
	MoveSetRule.bIncludeChildren = true;
	
	// Setup Any
	AnyStateRule.StateClass = TSoftClassPtr<USMStateInstance_Base>();
	AnyStateRule.bIncludeChildren = true;
	
	
	/* Setup Core->Core Rule */
	FSMNodeConnectionRule CoreCoreStateRule;
	CoreCoreStateRule.FromState = CoreStateRule;
	CoreCoreStateRule.ToState = CoreStateRule;

	/* Setup CoreState->CoreStateMachine Rule */
	FSMNodeConnectionRule CoreCoreSMRule;
	CoreCoreSMRule.FromState = CoreStateRule;
	CoreCoreSMRule.ToState = CoreStateMachineRule;

	/* Setup CoreStateMachine->CoreState Rule */
	FSMNodeConnectionRule CoreSMCoreRule;
	CoreSMCoreRule.FromState = CoreStateMachineRule;
	CoreSMCoreRule.ToState = CoreStateRule;

	/* Setup MoveSet->MoveSet Rule */
	FSMNodeConnectionRule MoveSetMoveSetRule;
	MoveSetMoveSetRule.FromState = MoveSetRule;
	MoveSetMoveSetRule.ToState = MoveSetRule;

	/* Setup Any->CoreState Rule */
	FSMNodeConnectionRule AnyCoreStateRule;
	AnyCoreStateRule.FromState = AnyStateRule;
	AnyCoreStateRule.ToState = CoreStateRule;

	/* Setup Any->CoreStateMachine Rule */
	FSMNodeConnectionRule AnyCoreStateMachineRule;
	AnyCoreStateMachineRule.FromState = AnyStateRule;
	AnyCoreStateMachineRule.ToState = CoreStateMachineRule;
	

	/* Set the rule */
	ConnectionRules.AllowedConnections = {CoreCoreStateRule, CoreCoreSMRule, CoreSMCoreRule, MoveSetMoveSetRule, AnyCoreStateRule, AnyCoreStateMachineRule};

	/* Prevent transition from ticking */
	
	bUseCustomColors = true;
	NodeColor = FColor::Purple;
#endif
}


void UAction_TransitionStateInstance::ConstructionScript_Implementation()
{
	if (bIsInterrupt)
	{
		SetNodeColor(FColor::Red);
	}
	else
	{
		SetNodeColor(FColor::Purple);
	}
	Super::ConstructionScript_Implementation();
}

void UAction_TransitionStateInstance::OnRootStateMachineStart_Implementation()
{
	Super::OnRootStateMachineStart_Implementation();

	/* Bind To Events To Store References Early Since They're Invariant During Runtime */
	if (!Cast<IActionStateFlow>(GetPreviousStateInstance()) || !Cast<IActionStateFlow>(GetNextStateInstance()))
	{
		//UE_LOG(LogTemp, Fatal, TEXT("State Setup Not Valid. Transition Not Connected To States Implementing Appropriate Interface [%s -> %s]"), *GetPreviousStateInstance()->GetNodeDisplayName(), *GetNextStateInstance()->GetNodeDisplayName());
	}
	
	PrevState = TScriptInterface<IActionStateFlow>(GetPreviousStateInstance());
	NextState = TScriptInterface<IActionStateFlow>(GetNextStateInstance());
	CharacterOwner = Cast<ARadicalCharacter>(GetContext());

	// Bind them from the start, and just register them with the input buffer when necessary
	ButtonInputDelegate.BindDynamic(this, &UAction_TransitionStateInstance::ButtonInputBinding);
	DirectionalInputDelegate.BindDynamic(this, &UAction_TransitionStateInstance::DirectionalInputBinding);
	DirectionalAndButtonInputDelegate.BindDynamic(this, &UAction_TransitionStateInstance::DirectionalAndButtonInputBinding);
	ButtonSequenceInputDelegate.BindDynamic(this, &UAction_TransitionStateInstance::ButtonSequenceInputBinding);
}

void UAction_TransitionStateInstance::OnRootStateMachineStop_Implementation()
{
	Super::OnRootStateMachineStop_Implementation();

	// Unbind them, to prevent attempting to rebind them OnRootStart
	ButtonInputDelegate.Unbind();
	DirectionalInputDelegate.Unbind();
	DirectionalAndButtonInputDelegate.Unbind();
	ButtonSequenceInputDelegate.Unbind();
}

UInputBufferSubsystem* UAction_TransitionStateInstance::GetInputBuffer() const
{
	if (APlayerController* PlayerController = Cast<APlayerController>(CharacterOwner->Controller))
	{
		return ULocalPlayer::GetSubsystem<UInputBufferSubsystem>(PlayerController->GetLocalPlayer());
	}
	return nullptr;
}

void UAction_TransitionStateInstance::EvaluateTransition()
{
	if (PrevState == NextState && IsTransitionFromAnyState()) return; // Don't self-transition if from any state and not a manual self-transition
	
	// CanExit calls the action instance, CanEnter calls the actionmanager. Both should be valid here.
	if ((PrevState->CanExit() || bIsInterrupt) && NextState->CanEnter())
	{
		SetCanEvaluate(true);

		ConsumeTransitionInput();
	}
}



void UAction_TransitionStateInstance::ForceExitBinding() // Bound to a states ShouldEnd which is bound to OnActionEnded in an action
{
	SetCanEvaluate(true);
}


void UAction_TransitionStateInstance::OnTransitionEntered_Implementation()
{
	if (!NextState.GetInterface()) return;
	
	if (!CharacterOwner.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("Context is not a radical character"));
		return;
	}

	// Initially used this to consume input, but now doing it earlier so the input buffer is up to date rather than waiting on the state machine to tick
}


void UAction_TransitionStateInstance::OnTransitionInitialized_Implementation()
{
	SCOPE_CYCLE_COUNTER(STAT_TransitionInitialized)
	
	// Disable transition evaluation, manually bound events determine this
	SetCanEvaluate(false);
	
	SetupBindings();

	Super::OnTransitionInitialized_Implementation();
}

void UAction_TransitionStateInstance::OnTransitionShutdown_Implementation()
{
	if (IsTransitionFromAnyState() && GetNextStateInstance()->IsEntryState())
	{
		if (PrevState.GetInterface())
		{
			PrevState->ForceExitDelegate.Unbind();
		}
	}

	UnbindInputsIfAny();

	GetPreviousStateInstance()->OnStateUpdateEvent.RemoveDynamic(this, &UAction_TransitionStateInstance::TickBinding);
	
	Super::OnTransitionShutdown_Implementation();
}

void UAction_TransitionStateInstance::SetupBindings()
{
	SCOPE_CYCLE_COUNTER(STAT_SetupBindings);
	
	/* Transition to entry state, implying a natural exit of the current state [State-State] */
	if (IsTransitionFromAnyState() && GetNextStateInstance()->IsEntryState())
	{
		PrevState->ForceExitDelegate.BindUObject(this, &UAction_TransitionStateInstance::ForceExitBinding); // TODO: This, binds to OnEndAction if not cancelled
	}
	/* Ordinary Gameplay State Transition, bind to input [State-State] */
	else
	{
		BindInputsIfAny();
	}
}


void UAction_TransitionStateInstance::BindInputsIfAny()
{
	/* Get Input Buffer Subsystem */
	SCOPE_CYCLE_COUNTER(STAT_BindInputs);
	const auto InputBuffer = GetInputBuffer();

	if (!InputBuffer)
	{
		UE_LOG(LogTemp, Error, TEXT("Buffer null"))
		return;
	}
	
	switch (NextState->GetStateInputType())
	{
		case None:
			GetPreviousStateInstance()->OnStateUpdateEvent.AddDynamic(this, &UAction_TransitionStateInstance::TickBinding); // NOTE: This was causing a stutter because it triggered an ensure due to me forgetting to unbind it and trying to bind it again
			//SetCanEvaluate(true);
		break;
		case Button:
			InputBuffer->BindAction(ButtonInputDelegate, NextState->GetFirstButtonAction(), NextState->GetTriggerType(), false);
		break;
		case ButtonSequence:
			InputBuffer->BindActionSequence(ButtonSequenceInputDelegate, NextState->GetFirstButtonAction(), NextState->GetSecondButtonAction(), false, NextState->GetConsiderOrder());
		break;
		case Directional:
			InputBuffer->BindDirectionalAction(DirectionalInputDelegate, NextState->GetDirectionalAction(), false);
		break;
		case DirectionButtonSequence:
			InputBuffer->BindDirectionalActionSequence(DirectionalAndButtonInputDelegate, NextState->GetFirstButtonAction(), NextState->GetDirectionalAction(), NextState->GetSequenceOrder(), false);
		break;
	}
}

void UAction_TransitionStateInstance::UnbindInputsIfAny() const
{
	UInputBufferSubsystem* InputBuffer = GetInputBuffer();
	
	if (!InputBuffer)
	{
		UE_LOG(LogTemp, Error, TEXT("Buffer null"))
		return;
	}
	
	switch (NextState->GetStateInputType())
	{
		case None:
		break;
		case Button:
			InputBuffer->UnbindAction(ButtonInputDelegate);
		break;
		case ButtonSequence:
			InputBuffer->UnbindActionSequence(ButtonSequenceInputDelegate);
		break;
		case Directional:
			InputBuffer->UnbindDirectionalAction(DirectionalInputDelegate);
		break;
		case DirectionButtonSequence:
			InputBuffer->UnbindDirectionalActionSequence(DirectionalAndButtonInputDelegate);
		break;
	}
}

void UAction_TransitionStateInstance::ConsumeTransitionInput() const
{
	UInputBufferSubsystem* InputBuffer = GetInputBuffer();

	if (!InputBuffer)
	{
		UE_LOG(LogTemp, Error, TEXT("Buffer null"))
		return;
	}
	
	switch (NextState->GetStateInputType())
	{
		case None:
			break;
		case Button:
			if (NextState->GetTriggerType() == TRIGGER_Press)
				InputBuffer->ConsumeButtonInput(NextState->GetFirstButtonAction());
		break;
		case ButtonSequence:
			InputBuffer->ConsumeButtonInput(NextState->GetFirstButtonAction());
			InputBuffer->ConsumeButtonInput(NextState->GetSecondButtonAction());
		break;
		case Directional:
			InputBuffer->ConsumeDirectionalInput(NextState->GetDirectionalAction());
		break;
		case DirectionButtonSequence:
			InputBuffer->ConsumeButtonInput(NextState->GetFirstButtonAction());
			InputBuffer->ConsumeDirectionalInput(NextState->GetDirectionalAction());
		break;
	}
}



