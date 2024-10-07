// Copyright 2023 CoC All rights reserved


#include "Behaviors/InteractionBehaviors_ActorsBasic.h"

#include "InteractionSystemDebug.h"
#include "Components/InteractableComponent.h"
#include "Misc/DataValidation.h"

void UInteractionBehavior_SpawnActor::ExecuteInteraction(EInteractionDomain Domain, float DeltaTime)
{
	if (!SpawnTransform)
	{
		INTERACTABLE_VLOG(InteractableOwner->GetOwner(), Warning, "Spawn Transform Marker Was Null!");
		CompleteBehavior();
		return;
	}
	
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	auto SpawnedActor = GetWorld()->SpawnActor<AActor>(ActorToSpawn, SpawnTransform->GetActorTransform(), SpawnParams);
	if (!SpawnedActor)
	{
		INTERACTABLE_VLOG(InteractableOwner->GetOwner(), Warning, "Tried spawning an actor but failed");
	}


	CompleteBehavior();
}

void UInteractionBehavior_DestroyActor::ExecuteInteraction(EInteractionDomain Domain, float DeltaTime)
{
	if (ActorToDestroy)
	{
		ActorToDestroy->Destroy();
	}
}

void UInteractionBehavior_SetActorEnable::Initialize(UInteractableComponent* OwnerInteractable)
{
	Super::Initialize(OwnerInteractable);

	if (!TargetActor) return;

	bActorCachedTickState = TargetActor->IsActorTickEnabled();

	SetActorEnabled(!bInitToDisabled);
	bTargetEnableState = bInitToDisabled;
}

void UInteractionBehavior_SetActorEnable::SetActorEnabled(bool bEnabled)
{
	TargetActor->SetActorHiddenInGame(!bEnabled);
	TargetActor->SetActorTickEnabled(bEnabled ? bActorCachedTickState : false);
	TargetActor->SetActorEnableCollision(bEnabled);

	if (bFlipFlopOnRetrigger)
	{
		bTargetEnableState = !bEnabled; // Opposite to whatever was input
	}
}

void UInteractionBehavior_SetActorEnable::ExecuteInteraction(EInteractionDomain Domain, float DeltaTime)
{
	if (!TargetActor)
	{
		CompleteBehavior();
		return;
	}
	
	SetActorEnabled(bTargetEnableState);
	CompleteBehavior();
}

