// Copyright 2023 CoC All rights reserved


#include "Conditions//InteractionCondition_CoolDown.h"

#include "Components/InteractableComponent.h"

void UInteractionCondition_CoolDown::Initialize(UInteractableComponent* OwnerInteractable)
{
	Super::Initialize(OwnerInteractable);

	if (bStartCooldownOnEnd)
	{
		InteractableOwner->OnInteractionEndedEvent.AddDynamic(this, &UInteractionCondition_CoolDown::StartCooldown);
	}
	else
	{
		InteractableOwner->OnInteractionStartedEvent.AddDynamic(this, &UInteractionCondition_CoolDown::StartCooldown);
	}

	bInCooldown = false;
}

void UInteractionCondition_CoolDown::StartCooldown()
{
	bInCooldown = true;
	
	if (CooldownHandle.IsValid())
	{
		CooldownHandle.Invalidate();
	}
	
	FTimerDelegate Delegate;
	Delegate.BindLambda([this]()
	{
		bInCooldown = false;
	});

	GetWorld()->GetTimerManager().SetTimer(CooldownHandle, Delegate, Cooldown, false);
}
