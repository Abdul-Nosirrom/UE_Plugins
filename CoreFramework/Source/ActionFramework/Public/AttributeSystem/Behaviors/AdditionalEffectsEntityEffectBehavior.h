// Copyright 2023 CoC All rights reserved

#pragma once

#include "CoreMinimal.h"
#include "AttributeSystem/EntityEffectBehavior.h"
#include "AttributeSystem/EntityEffectTypes.h"
#include "AdditionalEffectsEntityEffectBehavior.generated.h"

/**
 * Struct for gameplay effects that apply only if another gameplay effect (or execution) was successfully applied.
 */
USTRUCT(BlueprintType)
struct ACTIONFRAMEWORK_API FConditionalEntityEffect
{
	GENERATED_USTRUCT_BODY()

	bool CanApply(const FGameplayTagContainer& SourceTags) const { return SourceTags.HasAll(RequiredSourceTags); }

	FEntityEffectSpec CreateSpec(FEntityEffectContext EffectContext) const;

	/// @brief	gameplay effect that will be applied to the target 
	UPROPERTY(Category = "Entity Effects", EditDefaultsOnly, BlueprintReadOnly)
	TSubclassOf<UEntityEffect> EffectClass;

	/// @brief	Tags that the source must have for this GE to apply.  If this is blank, then there are no requirements to apply the EffectClass. 
	UPROPERTY(Category = "Entity Effects", EditDefaultsOnly, BlueprintReadOnly)
	FGameplayTagContainer RequiredSourceTags;
};


UCLASS(CollapseCategories)
class ACTIONFRAMEWORK_API UAdditionalEffectsEntityEffectBehavior : public UEntityEffectBehavior
{
	GENERATED_BODY()

public:

	virtual void OnEntityEffectApplied(UAttributeSystemComponent* AttributeSystem, FActiveEntityEffect& ActiveEffect) const override
	{
		TryApplyAdditionalEffects(AttributeSystem, ActiveEffect.Spec);
	}
	virtual void OnEntityEffectExecuted(UAttributeSystemComponent* AttributeSystem, FEntityEffectSpec& Spec) const override;

	virtual void OnEntityEffectRemoved(UAttributeSystemComponent* AttributeSystem, FActiveEntityEffect& ActiveEffect, bool bPrematureRemoval) const override;

protected:
	void TryApplyAdditionalEffects(UAttributeSystemComponent* AttributeSystemComponent, FEntityEffectSpec& Spec) const;
	
public:

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
	
	/// @brief	Other gameplay effects that will be applied to the target of this effect if the owning effect applies 
	UPROPERTY(Category = OnApplication, EditDefaultsOnly)
	TArray<FConditionalEntityEffect> OnApplicationEntityEffects;

	/// @brief	Effects to apply when this effect completes, regardless of how it ends 
	UPROPERTY(Category = OnComplete, EditDefaultsOnly)
	TArray<TSubclassOf<UEntityEffect>> OnCompleteAlways;

	/// @brief	Effects to apply when this effect expires naturally via its duration 
	UPROPERTY(Category = OnComplete, EditDefaultsOnly)
	TArray<TSubclassOf<UEntityEffect>> OnCompleteNormal;

	/// @brief	Effects to apply when this effect is made to expire prematurely (e.g. via a forced removal, clear tags, etc.) 
	UPROPERTY(Category = OnComplete, EditDefaultsOnly)
	TArray<TSubclassOf<UEntityEffect>> OnCompletePrematurely;
};
