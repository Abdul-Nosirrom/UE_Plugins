// Copyright 2023 CoC All rights reserved


#include "Visualizers/InteractableComponentVisualizer.h"

#include "Components/InteractableComponent.h"


void FInteractableComponentVisualizer::DrawVisualization(const UActorComponent* Component, const FSceneView* View,
                                                        FPrimitiveDrawInterface* PDI)
{
	const UInteractableComponent* InteractableComponent = Cast<UInteractableComponent>(Component);
	if (!InteractableComponent) return;
	const FVector OwnerLoc = InteractableComponent->GetOwner()->GetActorLocation();
	
	// Get actors referencing us by iterating through the interactables in the world
	TArray<AActor*> InteractablesReferencingUs;
	for (TObjectIterator<UInteractableComponent> InteractableIterator; InteractableIterator; ++InteractableIterator)
	{
		// Skip us
		if (*InteractableIterator == InteractableComponent) continue;

		// If object doesnt reference us, skip
		if (!InteractableIterator->IsActorReferenced(InteractableComponent->GetOwner())) continue;
		
		if (const auto Actor = InteractableIterator->GetOwner())
		{
			InteractablesReferencingUs.Add(Actor);
		}
	}

	// Draw purple lines from stuff referencing us
	{
		for (auto LinkedInteractables : InteractablesReferencingUs)
		{
			const FVector LinkLoc = LinkedInteractables->GetActorLocation();
			PDI->DrawLine(LinkLoc, OwnerLoc, FColor::Purple, SDPG_MAX, 4.f);
		}
	}

	// Draw the stuff we reference
	{
		for (auto ReferencedObjects : InteractableComponent->GetReferences())
		{
			// Could be null if not set in a behavior
			if (!ReferencedObjects) continue;
			
			const FVector ReferenceLoc = ReferencedObjects->GetActorLocation();
			PDI->DrawLine(OwnerLoc, ReferenceLoc, FColor::Green, SDPG_MAX, 4.f);
		}
	}
}
