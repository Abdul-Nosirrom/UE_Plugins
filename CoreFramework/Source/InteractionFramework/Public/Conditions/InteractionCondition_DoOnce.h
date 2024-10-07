// Copyright 2023 CoC All rights reserved

#pragma once

#include "CoreMinimal.h"
#include "Conditions/InteractionCondition.h"
#include "InteractionCondition_DoOnce.generated.h"

/**
 * 
 */
UCLASS(DisplayName="Do Once", MinimalAPI)
class UInteractionCondition_DoOnce : public UInteractionCondition
{
	GENERATED_BODY()
protected:

	bool bHasInteractedWith;
	
	virtual void Initialize(UInteractableComponent* OwnerInteractable) override;

	virtual bool CanInteract(const UInteractableComponent* OwnerInteractable) override;

	UFUNCTION()
	void InteractionConsumed() { bHasInteractedWith = true; }
};
