// Copyright 2023 CoC All rights reserved

#pragma once

#include "CoreMinimal.h"
#include "Conditions/InteractionCondition.h"
#include "InteractionCondition_CoolDown.generated.h"


UCLASS(DisplayName="Cool Down", MinimalAPI)
class UInteractionCondition_CoolDown : public UInteractionCondition
{
	GENERATED_BODY()

	// TODO: Maybe have some stuff going on here to do some visual stuff when cooldown ends or something
protected:

	UPROPERTY(EditAnywhere)
	float Cooldown;
	/// @brief  Whether to start the cooldown once the interaction is over, if false, will start as soon as the interaction has been started
	UPROPERTY(EditAnywhere, DisplayName="Start Cooldown OnInteractionEnd")
	bool bStartCooldownOnEnd = true;
	
	bool bInCooldown;

	virtual void Initialize(UInteractableComponent* OwnerInteractable) override;

	virtual bool CanInteract(const UInteractableComponent* OwnerInteractable) override { return !bInCooldown; }
	
	UFUNCTION()
	void StartCooldown();

	FTimerHandle CooldownHandle;
};
