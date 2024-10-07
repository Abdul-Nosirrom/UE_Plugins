// Copyright 2023 CoC All rights reserved

#pragma once

#include "CoreMinimal.h"
#include "AttributeSystem/EntityEffectBehavior.h"
#include "ChanceToApplyEntityEffectBehavior.generated.h"

/**
 * 
 */
UCLASS()
class ACTIONFRAMEWORK_API UChanceToApplyEntityEffectBehavior : public UEntityEffectBehavior
{
	GENERATED_BODY()

public:

	virtual bool CanEntityEffectApply(const UAttributeSystemComponent* AttributeSystem, const FEntityEffectSpec& Spec) const override;
	
private:
	/// @brief	Probability that this entity effect will be applied to the target actor (0.0 for Never, 1.0 for Always)
	UPROPERTY(Category=Application, EditDefaultsOnly, meta=(ClampMin=0, UIMin=0, ClampMax=1, UIMax=1))
	float ChanceToApplyToTarget;
};
