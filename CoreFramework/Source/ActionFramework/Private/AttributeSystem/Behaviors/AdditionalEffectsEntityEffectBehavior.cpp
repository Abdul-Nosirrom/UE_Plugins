// Copyright 2023 CoC All rights reserved


#include "AttributeSystem/Behaviors/AdditionalEffectsEntityEffectBehavior.h"

#include "AttributeSystem/EntityEffect.h"
#include "Components/AttributeSystemComponent.h"
#include "Misc/DataValidation.h"

#define LOCTEXT_NAMESPACE "AdditionalEntityEffectsBehavior"

FEntityEffectSpec FConditionalEntityEffect::CreateSpec(FEntityEffectContext EffectContext) const
{
	const UEntityEffect* EffectCDO = EffectClass ? EffectClass->GetDefaultObject<UEntityEffect>() : nullptr;
	return EffectCDO ? FEntityEffectSpec(EffectCDO, EffectContext) : FEntityEffectSpec();
}

void UAdditionalEffectsEntityEffectBehavior::OnEntityEffectExecuted(UAttributeSystemComponent* AttributeSystem,
                                                                    FEntityEffectSpec& Spec) const
{
	// Periodic might call this, but periodic also calls Applied, so let it just go through once through there and skip here
	if (Spec.Def->DurationPolicy != EEntityEffectDurationType::Instant) return;
	
	TryApplyAdditionalEffects(AttributeSystem, Spec);
}

void UAdditionalEffectsEntityEffectBehavior::TryApplyAdditionalEffects(
	UAttributeSystemComponent* AttributeSystemComponent, FEntityEffectSpec& Spec) const
{
	const FEntityEffectContext& EffectContext = Spec.GetContext();
	FGameplayTagContainer SourceTags;
	if (const auto SourceASC = EffectContext.GetInstigatorAttributeSystemComponent())
		SourceASC->GetOwnedGameplayTags(SourceTags);

	/** other effects that need to be applied to the target if this effect is successful */
	TArray<FEntityEffectSpec> TargetEffectSpecs;
	for (const FConditionalEntityEffect& ConditionalEffect : OnApplicationEntityEffects)
	{
		const UEntityEffect* EntityEffectDef = ConditionalEffect.EffectClass.GetDefaultObject();
		if (!EntityEffectDef)
		{
			continue;
		}

		if (ConditionalEffect.CanApply(SourceTags))
		{
			FEntityEffectSpec SpecHandle = ConditionalEffect.CreateSpec(EffectContext);
			TargetEffectSpecs.Add(SpecHandle);
		}
	}

	for (const FEntityEffectSpec& TargetSpec : TargetEffectSpecs)
	{
		AttributeSystemComponent->ApplyEntityEffectSpecToSelf(TargetSpec);
	}
}

void UAdditionalEffectsEntityEffectBehavior::OnEntityEffectRemoved(UAttributeSystemComponent* AttributeSystem,
                                                                   FActiveEntityEffect& ActiveEffect, bool bPrematureRemoval) const
{
	// Determine the appropriate type of effect to apply depending on whether the effect is being prematurely removed or not
	const TArray<TSubclassOf<UEntityEffect>>& ExpiryEffects = (bPrematureRemoval ? OnCompletePrematurely : OnCompleteNormal);

	// Mix-in the always-executing EntityEffects
	TArray<TSubclassOf<UEntityEffect>> AllEntityEffects{ ExpiryEffects };
	AllEntityEffects.Append(OnCompleteAlways);

	for (const TSubclassOf<UEntityEffect>& CurExpiryEffect : AllEntityEffects)
	{
		if (const UEntityEffect* CurExpiryCDO = CurExpiryEffect.GetDefaultObject())
		{
			FEntityEffectSpec NewSpec = ActiveEffect.Spec;
			NewSpec.Def = CurExpiryCDO;

			AttributeSystem->ApplyEntityEffectSpecToSelf(NewSpec);
		}
	}
}

#if WITH_EDITOR 
EDataValidationResult UAdditionalEffectsEntityEffectBehavior::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	if (GetOwner()->DurationPolicy == EEntityEffectDurationType::Instant)
	{
		const bool bHasOnCompleteEffects = (OnCompleteAlways.Num() + OnCompleteNormal.Num() + OnCompletePrematurely.Num() > 0);
		if (bHasOnCompleteEffects)
		{
			Context.AddError(FText::FormatOrdered(LOCTEXT("InstantDoesNotWorkWithOnComplete", "Instant EE will never receive OnComplete for {0}."), FText::FromString(GetClass()->GetName())));
			Result = EDataValidationResult::Invalid;
		}
	}
	else if (GetOwner()->Period > 0.0f)
	{
		if (OnApplicationEntityEffects.Num() > 0)
		{
			Context.AddWarning(LOCTEXT("IsPeriodicAndHasOnApplication", "Periodic EE has OnApplicationEntityEffects. Those EE's will only be applied once."));
		}
	}

	return Result;
}
#endif

#undef LOCTEXT_NAMESPACE