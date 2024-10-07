// Copyright 2023 CoC All rights reserved


#include "Triggers/InteractionTrigger_InputRequirement.h"

#include "Components/InteractableComponent.h"
#include "Actors/RadicalCharacter.h"
#include "Subsystems/InteractionManager.h"

void UInteractionTrigger_InputRequirement::OnBeginOverlap(AActor* OverlappedActor, AActor* OtherActor)
{
	if (OtherActor == GetPlayerCharacter())
	{
		GetWorld()->GetSubsystem<UInteractionManager>()->RegisterInputInteractable(InteractableOwner);
	}
}

void UInteractionTrigger_InputRequirement::OnEndOverlap(AActor* OverlappedActor, AActor* OtherActor)
{
	if (OtherActor == GetPlayerCharacter())
	{
		GetWorld()->GetSubsystem<UInteractionManager>()->UnRegisterInputInteractable(InteractableOwner);
	}
}