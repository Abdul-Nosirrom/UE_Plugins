// 


#include "ActionSystem/GameplayAction.h"

#include "InputBufferSubsystem.h"
#include "RadicalCharacter.h"
#include "TimerManager.h"
#include "ActionSystem/Triggers/ActionTrigger_InputRequirement.h"
#include "Camera/CameraModifier.h"
#include "Components/ActionSystemComponent.h"
#include "Components/AttributeSystemComponent.h"
#include "Debug/ActionSystemLog.h"
#include "Misc/DataValidation.h"

DECLARE_CYCLE_STAT(TEXT("Activating Action Internal"), STAT_ActivateActionInternal, STATGROUP_ActionSystem)
DECLARE_CYCLE_STAT(TEXT("End Action Internal"), STAT_EndActionInternal, STATGROUP_ActionSystem)
DECLARE_CYCLE_STAT(TEXT("Followup Init"), STAT_FollowupInit, STATGROUP_ActionSystem)
DECLARE_CYCLE_STAT(TEXT("Followup Cleanup"), STAT_FollowupCleanup, STATGROUP_ActionSystem)

UE_DEFINE_GAMEPLAY_TAG(TAG_Action_Base, "Action")
UE_DEFINE_GAMEPLAY_TAG(TAG_Action_Movement, "Action.Movement")

UWorld* UGameplayAction::GetWorld() const
{
	//Return null if called from the CDO, or if the outer is being destroyed
	if (!HasAnyFlags(RF_ClassDefaultObject) &&  !GetOuter()->HasAnyFlags(RF_BeginDestroyed) && !GetOuter()->IsUnreachable())
	{
		//Try to get the world from the owning actor if we have one
		AActor* Outer = GetTypedOuter<AActor>();
		if (Outer != nullptr)
		{
			return Outer->GetWorld();
		}
	}
	
	//Else return null - the latent action will fail to initialize
	return nullptr;
}

void UGameplayAction::Cleanup()
{
	// Cleanup all triggers and events
	if (ActionTrigger) ActionTrigger->Cleanup();
	for (auto Condition : ActionConditions) Condition->Cleanup();
	for (auto Event : ActionEvents) Event->Cleanup();
}

void UGameplayAction::OnActionGranted(const FActionActorInfo* ActorInfo)
{
	// Check if input requirement trigger exists, if so, store the input requirement
	if (auto InputTrigger = Cast<UActionTrigger_InputRequirement>(ActionTrigger))
	{
		InputRequirement = &InputTrigger->InputRequirement;
	}
	
	// Set actor and spec info
	CurrentActorInfo = ActorInfo;
	bIsCancelable = true;
	K2_OnActionGranted();
	
	ActionRuntimeSetup();

	// Setup all triggers and events
	if (ActionTrigger) ActionTrigger->Initialize(this);
	for (auto Condition : ActionConditions) Condition->Initialize(this);
	for (auto Event : ActionEvents) Event->Initialize(this);
}

void UGameplayAction::OnActionRemoved(const FActionActorInfo* ActorInfo)
{
	// Unbind default events
	// NOTE: Should we care to unbind the input delegates? We're gonna be destroyed when removed and they're apart of us so who cares. Maybe if there are issues, unbinding it helps by letting the input buffer know if it was still currently bound

	
	K2_OnActionRemoved();
}

bool UGameplayAction::CanActivateAction()
{
	// Ensure we hae a valid ActionSystemComponent reference
	if (!CurrentActorInfo->ActionSystemComponent.IsValid())
	{
		return false;
	}

	// Are we active and allowed to retrigger?
	if (IsActive() && !GetActionData()->bRetriggerAbility)
	{
		return false;
	}
	
	// Ensure valid tag requirements satisfied
	if (!DoesSatisfyTagRequirements())
	{
		ACTIONSYSTEM_VLOG(CurrentActorInfo->CharacterOwner.Get(), Log, "(ACTIVATION FAILED) [%s] Action Tag Requirements not satisfied", *GetActionData()->GetName())
		return false;
	}

	// Check condition scripts
	for (auto Condition : ActionConditions)
	{
		if (!Condition)
		{
			ACTIONSYSTEM_VLOG(CurrentActorInfo->CharacterOwner.Get(), Warning, "[%s] Encountered null condition script, skipping", *GetActionData()->GetName())
			continue;
		}

		if (!Condition->DoesConditionPass())
		{
			ACTIONSYSTEM_VLOG(CurrentActorInfo->CharacterOwner.Get(), Log, "(ACTIVATION FAILED) [%s] Condition script evaluted to false [%s]", *GetActionData()->GetName(), *Condition->GetName());
			return false;
		}
	}

	return EnterCondition();
}

bool UGameplayAction::DoesSatisfyTagRequirements() const
{
	const bool bIsBlocked =  CurrentActorInfo->ActionSystemComponent->AreActionTagsBlocked(GetActionData()->ActionTags);
	const bool bHasRequiredTags = CurrentActorInfo->ActionSystemComponent->HasAllMatchingGameplayTags(GetActionData()->OwnerRequiredTags);
	if (bIsBlocked || !bHasRequiredTags)
	{
		return false;
	}
	
	// Requirement satisfied if we've depleted at least 1 of the tags for reset, we do this implicitly by resetting NumTimesActivated when one of those tags are added
	return true;
}

void UGameplayAction::ActivateAction()
{
	SCOPE_CYCLE_COUNTER(STAT_ActivateActionInternal);

	if (!CurrentActorInfo || !CurrentActorInfo->ActionSystemComponent.IsValid())
	{
		ACTIONSYSTEM_LOG(Error, "[%s] Failed activation, ActorInfo or ActionSystem references are null", *GetName());
		return;
	}

	// Are we being retriggered? If so end the current one running in case it wants to do cleanup
	if (IsActive() && GetActionData()->bRetriggerAbility) EndAction(true);

	TimeActivated = GetWorld()->GetTimeSeconds();
	bIsActive = true;

	// Tags are added in ASC
	CurrentActorInfo->ActionSystemComponent->NotifyActionActivated(this);


	// Set default cancel behavior to true. Cancel behavior events are called after this so if setup, they'll override this but this is default for actions without auto-cancel behaviors
	SetCanBeCanceled(true);
	
	// Notify listeners that we've started
	OnGameplayActionStarted.Event.Broadcast();

	// Notify followups & additive actions to setup their triggers (NOTE: We call it after events, since events can change our CanCancel state)
	InitializeFollowupsAndAdditives();
	
	OnActionActivated();
}

void UGameplayAction::EndAction(bool bWasCancelled)
{
	SCOPE_CYCLE_COUNTER(STAT_EndActionInternal);
	
	if (IsActive())
	{
		// Stop timers or latent actions
		UWorld* MyWorld = GetWorld();
		if (MyWorld)
		{
			MyWorld->GetLatentActionManager().RemoveActionsForObject(this);
			MyWorld->GetTimerManager().ClearAllTimersForObject(this);
		}
		
		// Execute events then unbind it since we're no longer active
		OnGameplayActionEnded.Event.Broadcast();

		// Notify followups & Additive actions to cleanup their triggers
		DeinitializeFollowupsAndAdditives();
		
		// Overridden action behavior
		OnActionEnd();

		// Notify action manager
		if (CurrentActorInfo->ActionSystemComponent.IsValid())
		{
			// Tags are removed in ASC
			CurrentActorInfo->ActionSystemComponent->NotifyActionEnded(this);
		}

		// We'd have already used this variable, so lets null it so it doesn't hang around aimlessly in case we're not cancelled out next time
		ActionThatCancelledUs = nullptr;

		bIsActive = false;
	}
}

void UGameplayAction::CancelAction()
{
	if (CanBeCanceled())
	{
		EndAction(true);
	}
}

void UGameplayAction::InitializeFollowupsAndAdditives()
{
	SCOPE_CYCLE_COUNTER(STAT_FollowupInit)
	
	auto& ActivatableActions = CurrentActorInfo->ActionSystemComponent->GetActivatableActions();

	uint32 CurrentPriority = 1;
	// Setup followups that are enabled (NOTE: We should prolly just combine the two lists, no reason to keep them seperate?)
	for (auto& FollowUp : GetActionData()->GetFollowups())
	{
		if (!FollowUp.Action || !FollowUp.bEnabled) continue;

		if (!ActivatableActions.Contains(FollowUp.Action) && FollowUp.Action->bGrantOnActivation)
		{
			auto FollowUpInstance = CurrentActorInfo->ActionSystemComponent->GiveAction(FollowUp.Action);
			if (FollowUpInstance) FollowUpInstance->SetupActionTrigger(Priority + CurrentPriority);
		}
		else if (ActivatableActions.Contains(FollowUp.Action)) ActivatableActions[FollowUp.Action]->SetupActionTrigger(Priority + CurrentPriority);

		CurrentPriority++;
	}

	// Setup additives that are enabled
	for (auto& Additive : GetActionData()->GetAdditives())
	{
		if (!Additive.Action || !Additive.bEnabled) continue;

		if (!ActivatableActions.Contains(Additive.Action) && Additive.Action->bGrantOnActivation)
		{
			auto AdditiveInstance = CurrentActorInfo->ActionSystemComponent->GiveAction(Additive.Action);
			if (AdditiveInstance) AdditiveInstance->SetupActionTrigger(Priority + CurrentPriority);
		}
		else if (ActivatableActions.Contains(Additive.Action)) ActivatableActions[Additive.Action]->SetupActionTrigger(Priority + CurrentPriority);

		CurrentPriority++;
	}
}

void UGameplayAction::DeinitializeFollowupsAndAdditives()
{
	SCOPE_CYCLE_COUNTER(STAT_FollowupCleanup)
	
	auto& ActivatableActions = CurrentActorInfo->ActionSystemComponent->GetActivatableActions();
	
	// Setup followups that are enabled (NOTE: We should prolly just combine the two lists, no reason to keep them seperate?)
	for (auto& FollowUp : GetActionData()->GetFollowups())
	{
		if (!FollowUp.Action || !FollowUp.bEnabled) continue;

		if (ActivatableActions.Contains(FollowUp.Action))
		{
			ActivatableActions[FollowUp.Action]->CleanupActionTrigger();
		}
	}

	// Setup additives that are enabled
	for (auto& Additive : GetActionData()->GetAdditives())
	{
		if (!Additive.Action || !Additive.bEnabled) continue;

		if (ActivatableActions.Contains(Additive.Action))
		{
			if (ActivatableActions[Additive.Action]->IsActive()) ActivatableActions[Additive.Action]->EndAction(true);
			ActivatableActions[Additive.Action]->CleanupActionTrigger();
		}
	}
}

void UGameplayAction::SetupActionTrigger(uint32 InPriority)
{
	Priority = InPriority;
	
	// No trigger? Tick this actions activation
	if (!ActionTrigger) 
	{
		CurrentActorInfo->ActionSystemComponent->RegisterActionExecution(this);
	} // Else setup trigger
	else
	{
		ActionTrigger->SetupTrigger(Priority);
	}
}

void UGameplayAction::CleanupActionTrigger()
{
	// No trigger? Remove this action from ticking activation
	if (!ActionTrigger) 
	{
		CurrentActorInfo->ActionSystemComponent->UnRegisterActionExecution(this);
	} // Else setup trigger
	else
	{
		ActionTrigger->CleanupTrigger();
	}
}

#define LOCTEXT_NAMESPACE "GameplayActionValidation"
#if WITH_EDITOR 

void UGameplayAction::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	UObject::PostEditChangeProperty(PropertyChangedEvent);
	FDataValidationContext Context;
	IsDataValid(Context);
}

void UGameplayAction::PostLoad()
{
	UObject::PostLoad();
	FDataValidationContext Context;
	IsDataValid(Context);
}

EDataValidationResult UGameplayAction::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult ValidationResult = EDataValidationResult::NotValidated;

	auto ValidateScript = [this](const UActionScript* Script, FDataValidationContext& Context, EDataValidationResult& Result)
	{
		if (Script)
		{
			Result = Script->IsDataValid(Context);
			if (Result == EDataValidationResult::Invalid)
			{
				return false;
			}
		}
		else
		{
			Context.AddWarning(LOCTEXT("ScriptNull", "Null entry in ActionScripts"));
		}
		return true; 
	};

	if (ValidateScript(ActionTrigger, Context, ValidationResult))
	{
		for (const UActionScript* Condi : ActionConditions)
		{
			if (!ValidateScript(Condi, Context, ValidationResult)) break;
		}
		if (ValidationResult != EDataValidationResult::Invalid)
		{
			for (const UActionScript* Event : ActionEvents)
			{
				if (!ValidateScript(Event, Context, ValidationResult)) break;
			}
		}
	}

	TArray<FText> Warnings, Errors;
	Context.SplitIssues(Warnings, Errors);

	if (Errors.Num() > 0)
	{
		EditorStatusText = FText::FormatOrdered(LOCTEXT("ErrorsFmt", "Error: {0}"), FText::Join(FText::FromStringView(TEXT(", ")), Errors));
	}
	else if (Warnings.Num() > 0)
	{
		EditorStatusText = FText::FormatOrdered(LOCTEXT("WarningsFmt", "Warning: {0}"), FText::Join(FText::FromStringView(TEXT(", ")), Warnings));
	}
	else
	{
		EditorStatusText = LOCTEXT("AllOk", "All Ok");
	}
	
	return ValidationResult;
}

#endif
#undef LOCTEXT_NAMESPACE