// Copyright 2023 CoC All rights reserved


#include "AttributeSystem/Behaviors/ChanceToApplyEntityEffectBehavior.h"

bool UChanceToApplyEntityEffectBehavior::CanEntityEffectApply(const UAttributeSystemComponent* AttributeSystem,
	const FEntityEffectSpec& Spec) const
{
	if ((ChanceToApplyToTarget < 1.f - SMALL_NUMBER) && (FMath::FRand() > ChanceToApplyToTarget))
	{
		return false;
	}

	return true;
}
