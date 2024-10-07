// Copyright 2023 CoC All rights reserved


#include "Conditions//InteractionCondition_DoOnce.h"

#include "Components/InteractableComponent.h"

void UInteractionCondition_DoOnce::Initialize(UInteractableComponent* OwnerInteractable)
{
	Super::Initialize(OwnerInteractable);

	bHasInteractedWith = false;

	InteractableOwner->OnInteractionStartedEvent.AddDynamic(this, &UInteractionCondition_DoOnce::InteractionConsumed);
}

bool UInteractionCondition_DoOnce::CanInteract(const UInteractableComponent* OwnerInteractable)
{
	return !bHasInteractedWith;
}
