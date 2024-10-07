// Fill out your copyright notice in the Description page of Project Settings.


#include "Visualizers/DialogueComponentVisualizer.h"

#include "Components/DialogueInteractableComponent.h"


void FDialogueComponentVisualizer::DrawVisualization(const UActorComponent* Component, const FSceneView* View,
                                                    FPrimitiveDrawInterface* PDI)
{
	FInteractableComponentVisualizer::DrawVisualization(Component, View, PDI);
	
	const UDialogueInteractableComponent* DialogueComponent = Cast<UDialogueInteractableComponent>(Component);
	if (!DialogueComponent) return;

	// Retrieve events and draw lines from event actors to component
	const TArray<AActor*> RelevantActors = DialogueComponent->RetrieveEventActors();

	for (auto RelevantActor : RelevantActors)
	{
		if (!RelevantActor) continue;
		PDI->DrawLine(DialogueComponent->GetOwner()->GetActorLocation(), RelevantActor->GetActorLocation(), FColor::Purple, ESceneDepthPriorityGroup::SDPG_MAX, 5, 1, false);
	}
}
