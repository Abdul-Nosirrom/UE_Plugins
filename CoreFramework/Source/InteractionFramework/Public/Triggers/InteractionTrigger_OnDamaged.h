// Copyright 2023 CoC All rights reserved

#pragma once

#include "CoreMinimal.h"
#include "InteractionTrigger.h"
#include "InteractionTrigger_OnDamaged.generated.h"

/**
 * 
 */
UCLASS(Abstract, MinimalAPI) // TODO: Not done yet, getting the interface isn't working...
class UInteractionTrigger_OnDamaged : public UInteractionTrigger
{
	GENERATED_BODY()

protected:

	virtual void Initialize(UInteractableComponent* OwnerInteractable) override;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) override;
#endif 
};
