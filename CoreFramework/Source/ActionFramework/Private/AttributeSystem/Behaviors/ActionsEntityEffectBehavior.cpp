// Copyright 2023 CoC All rights reserved


#include "AttributeSystem/Behaviors/ActionsEntityEffectBehavior.h"

#include "AttributeSystem/EntityEffect.h"
#include "Components/ActionSystemComponent.h"
#include "Components/AttributeSystemComponent.h"
#include "Debug/AttributeLog.h"
#include "Misc/DataValidation.h"

#define LOCTEXT_NAMESPACE "ActionsEntityEffectBehavior"

void UActionsEntityEffectBehavior::OnEntityEffectApplied(UAttributeSystemComponent* AttributeSystem,
	FActiveEntityEffect& ActiveEffect) const
{
	if (!AttributeSystem->GetActorInfo().ActionSystemComponent.IsValid())
	{
		ATTRIBUTE_LOG(Warning, TEXT("Tried applying ActionsEffectBehavior on AttributeSystem whose owner has no ActionSystem [%s]"), *AttributeSystem->GetOwner()->GetName());
		return;
	}

	auto ActionSystem = AttributeSystem->GetActorInfo().ActionSystemComponent;

	const auto& AllActions = ActionSystem->GetActivatableActions();

	for (const FEffectActionGrantingSpec& ActionConfig : GrantActionsConfigs)
	{
		// If its already been given to us, then skip
		if (AllActions.Contains(ActionConfig.Action)) continue;

		auto ActionInstance = ActionSystem->GiveAction(ActionConfig.Action);
		if (ActionInstance && ActionConfig.RemovalPolicy == EEntityEffectGrantedActionRemovePolicy::RemoveActionOnEnd)
		{
			// Bind it to removal event for when its active
			FDelegateHandle Handle = ActionInstance->OnGameplayActionEnded.Event.AddLambda([ActionSystem, ActionInstance, ActionConfig, Handle]()
			{
				if (ActionInstance && ActionSystem.IsValid())
				{
					ActionSystem->RemoveAction(ActionConfig.Action);
					ActionInstance->OnGameplayActionEnded.Event.Remove(Handle);
				}
			});
		}

	}
}

void UActionsEntityEffectBehavior::OnEntityEffectRemoved(UAttributeSystemComponent* AttributeSystem,
	FActiveEntityEffect& ActiveEffect, bool bPrematureRemoval) const
{
	if (!AttributeSystem->GetActorInfo().ActionSystemComponent.IsValid())
	{
		return;
	}

	auto ActionSystem = AttributeSystem->GetActorInfo().ActionSystemComponent;
	
	for (const FEffectActionGrantingSpec& ActionConfig : GrantActionsConfigs)
	{
		// Other options are either do nothing, or remove 
		if (ActionConfig.RemovalPolicy == EEntityEffectGrantedActionRemovePolicy::CancelActionImmediately)
			ActionSystem->RemoveAction(ActionConfig.Action);
		else if (ActionConfig.RemovalPolicy == EEntityEffectGrantedActionRemovePolicy::RemoveActionOnEnd)
		{
			const auto ActionInstance = ActionSystem->GetRunningAction(ActionConfig.Action);
			if (ActionInstance && !ActionInstance->IsActive())
			{
				// NOTE: Removing deletes the action itself, so we dont have to worry about unbinding as it'll be marked as garbage
				ActionSystem->RemoveAction(ActionConfig.Action);
			}
		}
	}
}

#if WITH_EDITOR 
EDataValidationResult UActionsEntityEffectBehavior::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	if (GetOwner()->DurationPolicy == EEntityEffectDurationType::Instant)
	{
		if (GrantActionsConfigs.Num() > 0)
		{
			Context.AddError(FText::FormatOrdered(LOCTEXT("InstantDoesNotWorkWithGrantActions", "GrantActionsConfigs does not work with Instant Effects: {0}."), FText::FromString(GetClass()->GetName())));
			Result = EDataValidationResult::Invalid;
		}
	}

	for (int Index = 0; Index < GrantActionsConfigs.Num(); ++Index)
	{
		const UGameplayActionData* ActionData = GrantActionsConfigs[Index].Action;
		for (int CheckIndex = Index + 1; CheckIndex < GrantActionsConfigs.Num(); ++CheckIndex)
		{
			if (ActionData == GrantActionsConfigs[CheckIndex].Action)
			{
				Context.AddError(FText::FormatOrdered(LOCTEXT("GrantActionsMustBeUnique", "Multiple Actions of the same type cannot be granted by {0}."), FText::FromString(GetClass()->GetName())));
				Result = EDataValidationResult::Invalid;
			}
		}
	}

	return Result;
}
#endif

#undef LOCTEXT_NAMESPACE