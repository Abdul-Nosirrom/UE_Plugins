// Copyright 2023 CoC All rights reserved


#include "Behaviors/InteractionBehavior.h"

#include "Components/InteractableComponent.h"
#include "Misc/DataValidation.h"

void UInteractionBehavior::Initialize(UInteractableComponent* OwnerInteractable)
{
	Super::Initialize(OwnerInteractable);
	
	OwnerInteractable->Internal_StartEvent.AddDynamic(this, &UInteractionBehavior::InteractionStarted);
	OwnerInteractable->OnInteractionEndedEvent.AddDynamic(this, &UInteractionBehavior::InteractionEnded);
}

#if WITH_EDITOR 

EDataValidationResult UInteractionBehavior::IsBehaviorValid(EInteractionDomain InteractionDomain,
	FDataValidationContext& Context)
{
	return IsDataValid(Context);
}

#endif