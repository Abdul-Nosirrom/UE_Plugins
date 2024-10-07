// Copyright 2023 CoC All rights reserved


#include "AttributeSystem/Behaviors/TagRequirementsEntityEffectBehavior.h"

#include "AttributeSystem/EntityEffectTypes.h"
#include "Components/AttributeSystemComponent.h"

bool UTagRequirementsEntityEffectBehavior::CanEntityEffectApply(const UAttributeSystemComponent* AttributeSystem,
                                                                const FEntityEffectSpec& Spec) const
{
	FGameplayTagContainer TargetTags;
	Spec.GetContext().GetTargetAttributeSystemComponent()->GetOwnedGameplayTags(TargetTags);
	
	if (!ApplicationTagRequirements.RequirementsMet(TargetTags)) return false;
	
	return true;
}
