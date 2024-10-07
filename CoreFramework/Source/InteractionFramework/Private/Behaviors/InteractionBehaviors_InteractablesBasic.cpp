// Copyright 2023 CoC All rights reserved


#include "Behaviors/InteractionBehaviors_InteractablesBasic.h"

#include "Components/InteractableComponent.h"
#include "Misc/DataValidation.h"

void UInteractionBehavior_StartOtherInteractable::ExecuteInteraction(EInteractionDomain Domain, float DeltaTime)
{
	if (auto OtherInteractable = InteractableActor->FindComponentByClass<UInteractableComponent>())
	{
		OtherInteractable->InitializeInteraction();
	}

	CompleteBehavior();
}

void UInteractionBehavior_SetInteractionLockStatus::Initialize(UInteractableComponent* OwnerInteractable)
{
	Super::Initialize(OwnerInteractable);

	if (!bInitActorToOppositeStatus || !InteractableActor) return;
	
	if (auto Interactable = InteractableActor->FindComponentByClass<UInteractableComponent>())
	{
		const auto InitStatus = TargetLockStatus == EInteractionLockStatus::Locked ? EInteractionLockStatus::Unlocked : EInteractionLockStatus::Locked;
		Interactable->SetInteractionLockStatus(InitStatus);
	}
}

void UInteractionBehavior_SetInteractionLockStatus::ExecuteInteraction(EInteractionDomain Domain, float DeltaTime)
{
	if (!InteractableActor)
	{
		CompleteBehavior();
		return;
	}
	
	if (auto Interactable = InteractableActor->FindComponentByClass<UInteractableComponent>())
	{
		Interactable->SetInteractionLockStatus(TargetLockStatus);
	}

	if (RetriggerBehavior == EInteractionBehaviorLockRetrigger::FlipFlop)
	{
		// Setup for next time
		TargetLockStatus = TargetLockStatus == EInteractionLockStatus::Locked ? EInteractionLockStatus::Unlocked : EInteractionLockStatus::Locked;
	}

	CompleteBehavior();
}

#if WITH_EDITOR

TArray<AActor*> UInteractionBehavior_StartOtherInteractable::GetWorldReferences() const
{
	return {InteractableActor};
}

#define LOCTEXT_NAMESPACE "InteractionBehaviorValidation"

EDataValidationResult UInteractionBehavior_StartOtherInteractable::IsBehaviorValid(EInteractionDomain InteractionDomain, FDataValidationContext& Context)
{
	EDataValidationResult Result = Super::IsBehaviorValid(InteractionDomain, Context);

	
	if (!InteractableActor)
	{
		FText CurrentError = FText::FormatOrdered(LOCTEXT("IsNull", "[{0}] Interactable actor is null"), FText::FromString(StaticClass()->GetName()));
		Context.AddError(CurrentError);
		Result =  EDataValidationResult::Invalid;
		return Result;
	}

	if (!InteractableActor->FindComponentByClass<UInteractableComponent>())
	{
		FText CurrentError = FText::FormatOrdered(LOCTEXT("IsInteractable", "[{0}] Interactable actor does not have an interactable component"), FText::FromString(InteractableActor->GetName()));
		Context.AddError(CurrentError);
		Result =  EDataValidationResult::Invalid;
	} 
	
	return Result;
}

EDataValidationResult UInteractionBehavior_SetInteractionLockStatus::IsBehaviorValid(EInteractionDomain InteractionDomain, FDataValidationContext& Context)
{
	EDataValidationResult Result = Super::IsBehaviorValid(InteractionDomain, Context);

	
	if (!InteractableActor)
	{
		FText CurrentError = FText::FormatOrdered(LOCTEXT("IsNull", "[{0}] Interactable actor is null"), FText::FromString(StaticClass()->GetName()));
		Context.AddError(CurrentError);
		Result =  EDataValidationResult::Invalid;
		return Result;
	}

	if (!InteractableActor->FindComponentByClass<UInteractableComponent>())
	{
		FText CurrentError = FText::FormatOrdered(LOCTEXT("IsInteractable", "[{0}] Interactable actor does not have an interactable component"), FText::FromString(InteractableActor->GetName()));
		Context.AddError(CurrentError);
		Result =  EDataValidationResult::Invalid;
	} 
	
	return Result;
}

#undef LOCTEXT_NAMESPACE

#endif 