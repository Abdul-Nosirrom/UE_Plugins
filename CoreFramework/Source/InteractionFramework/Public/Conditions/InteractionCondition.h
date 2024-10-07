// Copyright 2023 CoC All rights reserved

#pragma once

#include "CoreMinimal.h"
#include "Components/InteractionScript.h"
#include "UObject/Object.h"
#include "InteractionCondition.generated.h"

/**
 * 
 */
UCLASS(EditInlineNew, DefaultToInstanced, Abstract)
class INTERACTIONFRAMEWORK_API UInteractionCondition : public UInteractionScript
{
	GENERATED_BODY()

	friend class UInteractableComponent;
	
protected:

	virtual bool CanInteract(const UInteractableComponent* OwnerInteractable) { return true; }
	
};
