// Copyright 2023 CoC All rights reserved


#include "Behaviors/InteractionBehavior_ApplyEffect.h"

#include "RadicalCharacter.h"
#include "Components/AttributeSystemComponent.h"
#include "Components/InteractableComponent.h"
#include "Helpers/ActionFrameworkStatics.h"


void UInteractionBehavior_ApplyEffect::Initialize(UInteractableComponent* OwnerInteractable)
{
	Super::Initialize(OwnerInteractable);

	const FEntityEffectContext Context(InteractableOwner->GetOwner(), InteractableOwner->GetOwner());
	EffectSpec = FEntityEffectSpec(EffectToApply->GetDefaultObject<UEntityEffect>(), Context);
}

void UInteractionBehavior_ApplyEffect::ExecuteInteraction(EInteractionDomain Domain, float DeltaTime)
{
	if (auto ASC = UActionFrameworkStatics::GetAttributeSystemFromActor(GetPlayerCharacter()))
	{
		ASC->ApplyEntityEffectSpecToSelf(EffectSpec);
	}
	CompleteBehavior();
}
