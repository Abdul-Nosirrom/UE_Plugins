// Copyright 2023 CoC All rights reserved
#include "AttributeSystem/EntityEffect.h"
#include "Components/AttributeSystemComponent.h"
#include "AttributeSystem/EntityEffectBehavior.h"
#include "Debug/AttributeLog.h"
#include "Misc/DataValidation.h"

#define LOCTEXT_NAMESPACE "EntityEffect"

/*--------------------------------------------------------------------------------------------------------------
* Entity Effects
*--------------------------------------------------------------------------------------------------------------*/


bool UEntityEffect::CanApply(UAttributeSystemComponent* AttributeSystem, const FEntityEffectSpec& EESpec) const
{
	for (const UEntityEffectBehavior* Behavior : EffectBehaviors)
	{
		if (Behavior && !Behavior->CanEntityEffectApply(AttributeSystem, EESpec))
		{
			//ATTRIBUTE_VLOG(AttributeSystem->GetOwnerActor(), Verbose, TEXT("%s Could not apply. Blocked by %s")); TODO: This
			return false;
		}
	}

	return true;
}


void UEntityEffect::OnExecuted(UAttributeSystemComponent* AttributeSystem, FEntityEffectSpec& EESpec) const
{
	ATTRIBUTE_VLOG(AttributeSystem->GetOwnerActor(), Verbose, TEXT("EntityEffect %s executed"), *GetNameSafe(EESpec.Def));
	if (EffectBehaviors.IsEmpty()) return;
	
	for (const UEntityEffectBehavior* Behavior : EffectBehaviors)
	{
		if (Behavior)
		{
			Behavior->OnEntityEffectExecuted(AttributeSystem, EESpec);
		}
	}
}

void UEntityEffect::OnApplied(UAttributeSystemComponent* AttributeSystem, FActiveEntityEffect& ActiveEE) const
{
	ATTRIBUTE_VLOG(AttributeSystem->GetOwnerActor(), Verbose, TEXT("EntityEffect %s applied"), *GetNameSafe(ActiveEE.Spec.Def));
	if (EffectBehaviors.IsEmpty()) return;

	for (const UEntityEffectBehavior* Behavior : EffectBehaviors)
	{
		if (Behavior)
		{
			Behavior->OnEntityEffectApplied(AttributeSystem, ActiveEE);
		}
	}
}

void UEntityEffect::OnRemoved(UAttributeSystemComponent* AttributeSystem, FActiveEntityEffect& ActiveEE, bool bPrematureRemoval) const
{
	ATTRIBUTE_VLOG(AttributeSystem->GetOwnerActor(), Verbose, TEXT("EntityEffect %s removed"), *GetNameSafe(ActiveEE.Spec.Def));
	if (EffectBehaviors.IsEmpty()) return;

	for (const UEntityEffectBehavior* Behavior : EffectBehaviors)
	{
		if (Behavior)
		{
			Behavior->OnEntityEffectRemoved(AttributeSystem, ActiveEE, bPrematureRemoval);
		}
	}
}

const UEntityEffectBehavior* UEntityEffect::FindBehavior(TSubclassOf<UEntityEffectBehavior> ClassToFind) const
{
	for (const TObjectPtr<UEntityEffectBehavior>& Behavior : EffectBehaviors)
	{
		if (Behavior && Behavior->IsA(ClassToFind))
		{
			return Behavior;
		}
	}

	return nullptr;
}

#if WITH_EDITOR

void UEntityEffect::PostLoad()
{
	UObject::PostLoad();

	switch (DurationPolicy) {
		case EEntityEffectDurationType::Instant:
			DurationMagnitude.MagnitudeCalculationPolicy = EAttributeModifierCalculationPolicy::FloatBased;
			DurationMagnitude.FloatBasedMagnitude = 0.f;
			Period = 0.f;
			break;
		case EEntityEffectDurationType::Infinite:
			DurationMagnitude.MagnitudeCalculationPolicy = EAttributeModifierCalculationPolicy::FloatBased;
			DurationMagnitude.FloatBasedMagnitude = -1.f;
			break;
		case EEntityEffectDurationType::HasDuration:
			break;
		default: ;
	}
	
	// Update status text
	FDataValidationContext Context;
	IsDataValid(Context);
}

/** [Copied from UE GAS since it seems important]
 * We now support GameplayEffectComponents which are Subobjects.
 * 
 * When we're loading a Blueprint (including Data Only Blueprints), we go through a bunch of steps:  Preload, Serialize (which does the instancing), Init, CDO Compile, PostLoad.
 * However, we are not guaranteed that our Parent Class (Archetype) is fully loaded until PostLoad.  So during load (or cooking, where we see this most) is that
 * a Child will request to load, then it's possible the Parent has not fully loaded, so they may also request its Parent load and they go through these steps *together*.
 * When we get to PostCDOCompiled, the Parent may create some Subobjects (for us, this happens during the Monolithic -> Modular upgrade) and yet the Child is also at the same step,
 * so it hasn't actually seen those Subobjects and instanced them, but it *would have* instanced them if Parent Class was loaded first through some other means.
 * So our job here is to ensure the Subobjects that exist in the Parent also exist in the Child, so that the order of loading the classes doesn't matter.
 *  
 * There are other issues that will pop-up:
 *	1. You cannot remove a Parent's Subobject in the Child (this code will just recreate it).
 *	2. If you remove a Subobject from the Parent, the Child will continue to own it and it will be delta serialized *from the Grandparent*
 */
void UEntityEffect::PostCDOCompiled(const FPostCDOCompiledContext& Context)
{
	UObject::PostCDOCompiled(Context);

	const UEntityEffect* Archetype = Cast<UEntityEffect>(GetArchetype());
	if (!Archetype)
	{
		return;
	}

	for (const UEntityEffectBehavior* ParentComponent : Archetype->EffectBehaviors)
	{
		if (!ParentComponent)
		{
			continue;
		}

		bool bFound = false;
		const FName ParentComponentName = ParentComponent->GetFName();
		for (const UEntityEffectBehavior* ChildComponent : EffectBehaviors)
		{
			// When the SubObject code decides how to delta serialize from its Archetype
			// The Archetype is determined by name. Let's match-up specifically by name rather than type alone.
			if (ChildComponent && ChildComponent->GetFName() == ParentComponentName)
			{
				bFound = true;
				break;
			}
		}

		// We already have the Component that matches the Archetype's Component
		if (bFound)
		{
			continue;
		}

		// We don't already have the Archetype's Component, so add it here using the exact same name so we link up.
		ATTRIBUTE_LOG(Verbose, TEXT("%s is manually duplicating Archetype %s because they were not inherited through automatic instancing"), *GetFullNameSafe(this), *GetFullNameSafe(ParentComponent));
		UEntityEffectBehavior* ChildComponent = DuplicateObject(ParentComponent, this, ParentComponentName);
		EffectBehaviors.Add(ChildComponent);
	}
}


void UEntityEffect::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	UObject::PostEditChangeProperty(PropertyChangedEvent);

	switch (DurationPolicy) {
		case EEntityEffectDurationType::Instant:
			DurationMagnitude.MagnitudeCalculationPolicy = EAttributeModifierCalculationPolicy::FloatBased;
			DurationMagnitude.FloatBasedMagnitude = 0.f;
			Period = 0.f;
			break;
		case EEntityEffectDurationType::Infinite:
			DurationMagnitude.MagnitudeCalculationPolicy = EAttributeModifierCalculationPolicy::FloatBased;
			DurationMagnitude.FloatBasedMagnitude = -1.f;
			break;
		case EEntityEffectDurationType::HasDuration:
			break;
		default: ;
	}

	// Update the status text
	FDataValidationContext DataValidationContext;
	IsDataValid(DataValidationContext);
}

EDataValidationResult UEntityEffect::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult ValidationResult = Super::IsDataValid(Context);

	if (ValidationResult != EDataValidationResult::Invalid)
	{
		for (UEntityEffectBehavior* EEBehavior : EffectBehaviors)
		{
			if (EEBehavior)
			{
				ValidationResult = EEBehavior->IsDataValid(Context);
				if (ValidationResult == EDataValidationResult::Invalid)
				{
					break;
				}
			}
			else
			{
				Context.AddWarning(LOCTEXT("EEIsNull", "Null entry in EEBehaviors"));
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