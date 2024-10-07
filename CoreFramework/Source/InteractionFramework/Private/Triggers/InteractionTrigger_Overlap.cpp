// Copyright 2023 CoC All rights reserved


#include "Triggers/InteractionTrigger_Overlap.h"

#include "Components/InteractableComponent.h"
#include "Actors/RadicalCharacter.h"
#include "Misc/DataValidation.h"
#include "Subsystems/InteractionManager.h"


void UInteractionTrigger_Overlap::Initialize(UInteractableComponent* OwnerInteractable)
{
	Super::Initialize(OwnerInteractable);

	InteractableOwner->GetOwner()->OnActorBeginOverlap.AddDynamic(this, &UInteractionTrigger_Overlap::OnBeginOverlap);
	InteractableOwner->GetOwner()->OnActorEndOverlap.AddDynamic(this, &UInteractionTrigger_Overlap::OnEndOverlap);
	bTriggeredWithoutRegistering = false;
}

void UInteractionTrigger_Overlap::OnBeginOverlap(AActor* OverlappedActor, AActor* OtherActor)
{
	if (OtherActor == GetPlayerCharacter())
	{
		if (InteractableOwner->IsInteractionValid())
		{
			bTriggeredWithoutRegistering = true;
			InteractableOwner->InitializeInteraction();
		}
		else
		{
			bTriggeredWithoutRegistering = false;
			GetWorld()->GetSubsystem<UInteractionManager>()->RegisterInteractable(InteractableOwner);
		}
	}
}

void UInteractionTrigger_Overlap::OnEndOverlap(AActor* OverlappedActor, AActor* OtherActor)
{
	if (bStopOnEndOverlap && InteractableOwner->GetInteractionState() == EInteractionState::InProgress)
	{
		InteractableOwner->StopInteraction();
	}
	
	if (OtherActor == GetPlayerCharacter() && !bTriggeredWithoutRegistering)
	{
		GetWorld()->GetSubsystem<UInteractionManager>()->UnRegisterInteractable(InteractableOwner);
	}
	bTriggeredWithoutRegistering = false;
}

#if WITH_EDITOR 
#define LOCTEXT_NAMESPACE "InteractionTriggerValidation"

EDataValidationResult UInteractionTrigger_Overlap::IsDataValid(FDataValidationContext& Context)
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	
	TArray<UActorComponent*> OwnerComponents;
	InteractableOwner = Cast<UInteractableComponent>(GetOuter());
	if (!InteractableOwner->GetOwner()) return Result;
	
	InteractableOwner->GetOwner()->GetComponents(UPrimitiveComponent::StaticClass(), OwnerComponents);
	
	if (OwnerComponents.IsEmpty())
	{
		FText CurrentError = FText::FormatOrdered(LOCTEXT("NoCollision", "[{0}] Owner needs a primitive component to overlap with, has none!"), FText::FromString(InteractableOwner->GetOwner()->GetName()));
		Context.AddError(CurrentError);
		Result =  EDataValidationResult::Invalid;
	}
	else
	{
		bool bAnyOverlapComps = false;
		for (auto Comp : OwnerComponents)
		{
			if (auto PrimComp = Cast<UPrimitiveComponent>(Comp))
			{
				auto CollisionResponse = PrimComp->GetCollisionResponseToChannel(ECC_Pawn);
				bAnyOverlapComps |= CollisionResponse != ECR_Overlap;
			}
		}

		if (!bAnyOverlapComps)
		{
			FText CurrentError = FText::FormatOrdered(LOCTEXT("CollisionSettings", "[{0}] Owner has no PrimitiveComp thats set to overlap with Pawn!"), FText::FromString(InteractableOwner->GetOwner()->GetName()));
			Context.AddError(CurrentError);
			Result =  EDataValidationResult::Invalid;
		}
	}

	
	return Result;
}
#endif