// 


#include "StateMachineDefinitions/Action_CoreStateMachineInstance.h"

#include "ImageUtils.h"
#include "Actors/RadicalPlayerCharacter.h"
#include "Components/ActionManagerComponent.h"

#include "Interfaces/IPluginManager.h"

#include "StateMachineDefinitions/Action_CoreStateInstance.h"
#include "StateMachineDefinitions/Action_MoveSetSMInstance.h"

UAction_CoreStateMachineInstance::UAction_CoreStateMachineInstance()
{
#if WITH_EDITOR
	/* Establish Connection Rules */

	////////////////////////////////
	/// 1.) AnyState->This (We do this implicitly by not setting anything in AllowedInboundStates)
	/// 2.) This->This
	/// 3.) This->CoreState
	////////////////////////////////
	
	/* Define Base Data */
	FSMStateClassRule CoreStateRule;
	FSMStateClassRule CoreStateMachineRule;
	FSMStateMachineClassRule MoveSetClassRule;

	CoreStateRule.StateClass = TSoftClassPtr<USMStateInstance_Base>(UAction_CoreStateInstance::StaticClass());
	CoreStateRule.bIncludeChildren = true;

	CoreStateMachineRule.StateClass = TSoftClassPtr<USMStateInstance_Base>(StaticClass());
	CoreStateMachineRule.bIncludeChildren = true;

	MoveSetClassRule.StateMachineClass = TSoftClassPtr<USMStateMachineInstance>(UAction_MoveSetSMInstance::StaticClass());
	MoveSetClassRule.bIncludeChildren = true;

	/* Set the rules */
	ConnectionRules.AllowedOutboundStates = {CoreStateRule, CoreStateMachineRule};
	ConnectionRules.AllowedInStateMachines = {MoveSetClassRule};
	bHideFromContextMenuIfRulesFail = true;

	/* Don't wait for EndState reached as that doesn't make sense for this type of State Machine */
	SetWaitForEndState(false);

#endif
}

/* Retrieve valid references */
void UAction_CoreStateMachineInstance::OnRootStateMachineStart_Implementation()
{
	/* Cache references for the state to use */
	if (!ActionManager)
	{
		const auto CharacterOwner = Cast<ARadicalPlayerCharacter>(GetContext());
		ActionManager = CharacterOwner->GetActionManager();
	}

	// NOTE: Can't retrieve action instance here as it has never been activated at this point and therefore, never instanced
	
	Super::OnRootStateMachineStart_Implementation();
}

/* Action was activated during CanEnter */
void UAction_CoreStateMachineInstance::OnStateBegin_Implementation()
{
	Super::OnStateBegin_Implementation();

	// We use this to retrieve the action instance, we assume it exists otherwise we wouldn't have entered the state to begin with
	if (!ActionInstance)
	{
		ActionInstance = ActionManager->FindActionInstanceFromClass(ActionClass);
		ActionInstance->OnGameplayActionEnded.AddUObject(this, &UAction_CoreStateMachineInstance::OnActionEnded);
	}
}

/* In some cases, we're here because the action ended itself. Otherwise, we check if its active and force cancel it if its not already cancellable. This is only the case for Interrupt transitions */
void UAction_CoreStateMachineInstance::OnStateEnd_Implementation()
{
	Super::OnStateEnd_Implementation();

	// Check if instance is active, if so we're cancelling it. Otherwise we were ended because the action itself was ended (or cancelled externally)
	if (ActionInstance->IsActive())
	{
		if (!ActionInstance->CanBeCanceled())
		{
			// This is specifically for interrupt transitions which don't care about the outgoing state, we force it to be cancellable (We can't avoid the transition in here)
			ActionInstance->SetCanBeCanceled(true);
		}
		ActionInstance->CancelAction(); 
	}
	// else we assume the action ended naturally, hence why it's no longer active by the time we get here
}

/* This binds to the action. If the ending of the action was not due to cancellation, we rever to entry state. */
void UAction_CoreStateMachineInstance::OnActionEnded(UGameplayAction* Action, bool bWasCancelled) const
{
	// To revert us back to the entry state
	if (!bWasCancelled)
		ForceExitDelegate.ExecuteIfBound();
}

/* We check if we can activate the action through the actionmanager. If we can, the action manager will auto-activate it. */
bool UAction_CoreStateMachineInstance::CanEnter()
{
	return ActionManager->TryActivateAbilityByClass(ActionClass);
}

/* This is checked for normal transitions, we take them only if we can cancel the current action */
bool UAction_CoreStateMachineInstance::CanExit()
{
	return ActionInstance->CanBeCanceled(); // need to store action instance safely, noting that it can be "removed" at any time.
}



void UAction_CoreStateMachineInstance::ConstructionScript_Implementation()
{
	/* Set Visuals */
	if (!NodeIcon)
	{
		static FString ContentDir = IPluginManager::Get().FindPlugin(TEXT("CoreFramework"))->GetBaseDir() / TEXT("Resources");
		static FString IconDir = (ContentDir / TEXT("corestatemachineicon")) + TEXT(".png");
		const auto Icon = FImageUtils::ImportFileAsTexture2D(IconDir);
		SetUseCustomIcon(true);
		NodeIcon = Icon;
		NodeIconSize = FVector2D(100,100);
	}
#pragma region Input Construction
	
	bool bHideFirstButton = false, bHideSecondButton = false, bHideDirectional = false;
	bool bHideSequenceOrder = false, bHideTriggerEvent = false, bHideConsiderOrder = false;
	
	switch (InputType)
	{
		case None:
			bHideFirstButton = true;
		bHideSecondButton = true;
		bHideDirectional = true;
		bHideSequenceOrder = true;
		bHideTriggerEvent = true;
		bHideConsiderOrder = true;
		break;
		case Button:
			bHideSecondButton = true;
		bHideDirectional = true;
		bHideSequenceOrder = true;
		bHideConsiderOrder = true;
		break;
		case ButtonSequence:
			bHideDirectional = true;
		bHideSequenceOrder = true;
		bHideTriggerEvent = true;
		break;
		case Directional:
			bHideFirstButton = true;
		bHideSecondButton = true;
		bHideSequenceOrder = true;
		bHideConsiderOrder = true;
		bHideTriggerEvent = true;
		break;
		case DirectionButtonSequence:
			bHideSecondButton = true;
		bHideConsiderOrder = true;
		bHideTriggerEvent = true;
		break;
	}
		
	SetVariableHidden(GET_MEMBER_NAME_CHECKED(UAction_CoreStateMachineInstance, ButtonInput), bHideFirstButton);
	SetVariableHidden(GET_MEMBER_NAME_CHECKED(UAction_CoreStateMachineInstance, SecondButtonInput), bHideSecondButton);
	SetVariableHidden(GET_MEMBER_NAME_CHECKED(UAction_CoreStateMachineInstance, DirectionalInput), bHideDirectional);

	SetVariableHidden(GET_MEMBER_NAME_CHECKED(UAction_CoreStateMachineInstance, bConsiderOrder), bHideConsiderOrder);
	SetVariableHidden(GET_MEMBER_NAME_CHECKED(UAction_CoreStateMachineInstance, SequenceOrder), bHideSequenceOrder);
	SetVariableHidden(GET_MEMBER_NAME_CHECKED(UAction_CoreStateMachineInstance, TriggerEvent), bHideTriggerEvent);

#pragma endregion Input Construction
	
	Super::ConstructionScript_Implementation();
}
