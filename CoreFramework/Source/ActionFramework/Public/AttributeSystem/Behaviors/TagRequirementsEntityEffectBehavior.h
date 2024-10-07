// Copyright 2023 CoC All rights reserved

#pragma once

#include "CoreMinimal.h"
#include "AttributeSystem/EntityEffectBehavior.h"
#include "Data/GameplayTagData.h"
#include "TagRequirementsEntityEffectBehavior.generated.h"

/**
 * 
 */
UCLASS(CollapseCategories)
class ACTIONFRAMEWORK_API UTagRequirementsEntityEffectBehavior : public UEntityEffectBehavior
{
	GENERATED_BODY()

public:
	UTagRequirementsEntityEffectBehavior()
	{
#if WITH_EDITORONLY_DATA
		EditorFriendlyName = TEXT("Target Tag Reqs (While EE is Active)");
#endif 
	}

	virtual bool CanEntityEffectApply(const UAttributeSystemComponent* AttributeSystem, const FEntityEffectSpec& Spec) const override;

	
	/// @brief  Tag requirements for this effect to be applied to a target. This is pass/fail at the time of application/execution
	UPROPERTY(Category=Tags, EditDefaultsOnly)
	FGameplayTagRequirements ApplicationTagRequirements;
	
};
