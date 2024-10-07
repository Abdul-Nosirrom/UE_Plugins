// Copyright 2023 CoC All rights reserved


#include "Triggers/InteractionTrigger_OnOtherOverlaps.h"

#include "InteractionSystemDebug.h"
#include "Components/InteractableComponent.h"
#include "Subsystems/InteractionManager.h"

void UInteractionTrigger_OnOtherOverlaps::Initialize(UInteractableComponent* OwnerInteractable)
{
	Super::Initialize(OwnerInteractable);

	for (auto Volume : OverlapVolumes)
	{
		if (!Volume)
		{
			INTERACTABLE_VLOG(InteractableOwner->GetOwner(), Warning, "One or more overlap volumes are null!");
			continue;
		}

		Volume->OnActorBeginOverlap.AddDynamic(this, &UInteractionTrigger_OnOtherOverlaps::OnBeginOverlap);
		Volume->OnActorEndOverlap.AddDynamic(this, &UInteractionTrigger_OnOtherOverlaps::OnEndOverlap);
	}
}

void UInteractionTrigger_OnOtherOverlaps::OnBeginOverlap(AActor* OverlappedActor, AActor* OtherActor)
{
	NumOverlaps++;
	if (NumOverlaps == OverlapVolumes.Num())
	{
		if (InteractableOwner->IsInteractionValid())
		{
			bInitializedWithoutRegistering = true;
			InteractableOwner->InitializeInteraction();
		}
		else
		{
			bInitializedWithoutRegistering = false;
			GetWorld()->GetSubsystem<UInteractionManager>()->RegisterInteractable(InteractableOwner);
		}
	}
}

void UInteractionTrigger_OnOtherOverlaps::OnEndOverlap(AActor* OverlappedActor, AActor* OtherActor)
{
	if (NumOverlaps == OverlapVolumes.Num() && bInitializedWithoutRegistering)
	{
		GetWorld()->GetSubsystem<UInteractionManager>()->UnRegisterInteractable(InteractableOwner);
	}
	NumOverlaps--;
}

