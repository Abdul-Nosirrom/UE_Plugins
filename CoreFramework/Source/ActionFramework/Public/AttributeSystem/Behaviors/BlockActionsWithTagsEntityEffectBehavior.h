// Copyright 2023 CoC All rights reserved

#pragma once

#include "CoreMinimal.h"
#include "AttributeSystem/EntityEffectBehavior.h"
#include "Data/GameplayTagData.h"
#include "BlockActionsWithTagsEntityEffectBehavior.generated.h"

/// @brief	Handles blocking actions for the duration the effect is applied
UCLASS()
class ACTIONFRAMEWORK_API UBlockActionsWithTagsEntityEffectBehavior : public UEntityEffectBehavior
{
	GENERATED_BODY()

	/// @brief Setup editor friendly name since thats what the UPROPERTY of UEntityEffect is gonna call us
	virtual void PostInitProperties() override
	{
		Super::PostInitProperties();
#if WITH_EDITORONLY_DATA
		EditorFriendlyName = TEXT("Block Actions W/ Tags");
#endif
	}

	virtual void OnEntityEffectApplied(UAttributeSystemComponent* AttributeSystem, FActiveEntityEffect& ActiveEffect) const override;
	virtual void OnEntityEffectRemoved(UAttributeSystemComponent* AttributeSystem, FActiveEntityEffect& ActiveEffect, bool bPrematureRemoval) const override;

private:
	/// @brief	These tags are applied to the target actor of the Gameplay Effect.  Blocked Ability Tags prevent Gameplay Abilities with these tags from executing. 
	UPROPERTY(EditDefaultsOnly, Category = None, meta = (DisplayName = "Block Abilities w/ Tags"))
	FGameplayTagContainer InheritableBlockedAbilityTagsContainer;

};
