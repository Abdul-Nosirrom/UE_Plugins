// Copyright 2023 CoC All rights reserved

#pragma once

#include "CoreMinimal.h"
#include "AttributeSystem/EntityEffectBehavior.h"
#include "AttributeSystem/EntityEffectTypes.h"
#include "ImmunityEntityEffectBehavior.generated.h"

/**
 * 
 */
UCLASS()
class ACTIONFRAMEWORK_API UImmunityEntityEffectBehavior : public UEntityEffectBehavior
{
	GENERATED_BODY()

	virtual void PostInitProperties() override
	{
		Super::PostInitProperties();
#if WITH_EDITORONLY_DATA
		EditorFriendlyName = TEXT("Immunity (Prevent Other EEs)");
#endif 
	}

#if WITH_EDITOR
	/// @brief  Simply checks if effect we're on is INSTANT and gives a warning since we can't apply immunity on instant effects
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif 
	
	virtual void OnEntityEffectApplied(UAttributeSystemComponent* AttributeSystem, FActiveEntityEffect& ActiveEffect) const override;

	bool QueryEffectBeingApplied(const UAttributeSystemComponent* AttributeSystem, const FEntityEffectSpec& Spec) const;

public:
	///	@brief	Grants immunity to EntityEffects that match these queries
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = None)
	TArray<FEntityEffectQuery> ImmunityQueries;
};
