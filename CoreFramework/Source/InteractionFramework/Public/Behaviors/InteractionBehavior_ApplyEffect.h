// Copyright 2023 CoC All rights reserved

#pragma once

#include "CoreMinimal.h"
#include "InteractionBehavior.h"
#include "AttributeSystem/EntityEffectTypes.h"
#include "InteractionBehavior_ApplyEffect.generated.h"

/**
 * 
 */
UCLASS(DisplayName="Apply Effect", MinimalAPI)
class UInteractionBehavior_ApplyEffect : public UInteractionBehavior
{
	GENERATED_BODY()

protected:
	
	UPROPERTY(EditAnywhere)
	TSubclassOf<UEntityEffect> EffectToApply;

	UPROPERTY()
	FEntityEffectSpec EffectSpec;
	
	virtual void Initialize(UInteractableComponent* OwnerInteractable) override;
	virtual void ExecuteInteraction(EInteractionDomain Domain, float DeltaTime) override;
};
