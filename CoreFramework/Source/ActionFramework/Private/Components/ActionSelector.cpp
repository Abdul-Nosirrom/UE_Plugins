#include "RadicalCharacter.h"
#include "Components/ActionSystemComponent.h"
#include "Debug/ActionSystemLog.h"

DECLARE_CYCLE_STAT(TEXT("Select Action"), STAT_SelectAction, STATGROUP_ActionSystem)

void UActionSystemComponent::InitializeCharacterActionSet()
{
	int32 DefIdx = 0;
	for (auto Default : ActionSet->DefaultActions)
	{
		const auto GrantedAction = GiveAction(Default.Value.Action);
		if (DefIdx == 0)
		{
			AddTag(Default.Key);
			InternalTryActivateAction(GrantedAction);
		}
		
		RegisterGameplayTagEvent(Default.Key, EGameplayTagEventType::NewOrRemoved).AddUObject(this, &UActionSystemComponent::SwapDefaultAction);
		
		DefIdx++;
	}

	// NOTE: Here we might wanna setup the binding in GiveAction? In case they haven't been granted yet and we wanna hold off on granting them?
	// Bind base actions
	uint32 BasePriority = 0;
	for (auto Base : ActionSet->BaseActions)
	{
		if (!Base.bEnabled) continue;
		
		const auto GrantedAction = GiveAction(Base.Action);
		if (GrantedAction)
		{
			GrantedAction->SetupActionTrigger(BasePriority);
		}
		BasePriority++;
	}

	// NOTE: Have to be smarter with CancelAction functions when dealin with additives, if something like lock on is an additive action, then when getting hit and calling CancelAllActions, there may be additives we don't wanna cancel? How do we deal w/ it? 
	// Bind additive actions
	uint32 AdditivePriority = 0;
	for (auto Additive : ActionSet->GlobalAdditiveActions)
	{
		if (!Additive.bEnabled) continue;
		
		const auto GrantedAction = GiveAction(Additive.Action);
		if (GrantedAction)
		{
			GrantedAction->SetupActionTrigger(AdditivePriority);
		}
		AdditivePriority++;
	}
}

void UActionSystemComponent::SwapDefaultAction(FGameplayTag DefaultTag, int Count)
{
	if (Count <= 0)
	{
		// Cancel defaults if running
		for (const auto& Default : ActionSet->DefaultActions)
		{
			CancelAction(Default.Value.Action);
			RemoveCountOfTag(Default.Key, 9999);
		}

		return;
	}
	
	const auto DefaultToActivate = ActionSet->DefaultActions[DefaultTag];
	if (!PrimaryActionRunning) TryActivateAbilityByClass(DefaultToActivate.Action);
}

UGameplayActionData* UActionSystemComponent::GetCurrentDefaultAction() const
{
	for (const auto& Defaults : ActionSet->DefaultActions)
	{
		if (HasMatchingGameplayTag(Defaults.Key))
		{
			return Defaults.Value.Action;
		}
	}

	ACTIONSYSTEM_VLOG(CharacterOwner, Warning, "No compatible tags found on the character for any default action!");
	
	return nullptr;
}


void UActionSystemComponent::UpdateActionSelection()
{
	SCOPE_CYCLE_COUNTER(STAT_SelectAction)
	
	// Here we basically loop through all our registered non-input actions
	// Priority important here?
	for (int32 ActionIdx = ActionsAwaitingActivation.Num()-1; ActionIdx >= 0; ActionIdx--)
	{
		const auto Registered = ActionsAwaitingActivation[ActionIdx];
		if (!Registered) continue;
		
		if (InternalTryActivateAction(Registered)) break;
	}

	// No primary actions running ? Start the available default one
	if (!PrimaryActionRunning)
	{
		TryActivateAbilityByClass(GetCurrentDefaultAction());
	}
}

void UActionSystemComponent::RegisterActionExecution(UGameplayAction* InAction)
{
	ActionsAwaitingActivation.AddUnique(InAction);
}

void UActionSystemComponent::UnRegisterActionExecution(UGameplayAction* InAction)
{
	ActionsAwaitingActivation.Remove(InAction);
}

