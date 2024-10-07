// Copyright 2023 CoC All rights reserved

#pragma once

#include "CoreMinimal.h"
#include "InteractionBehavior.h"
#include "InteractionBehaviors_InteractablesBasic.generated.h"

/**
 * 
 */
UCLASS(DisplayName="Start Other Interaction", MinimalAPI)
class UInteractionBehavior_StartOtherInteractable : public UInteractionBehavior
{
	GENERATED_BODY()

protected:

	UPROPERTY(EditAnywhere)
	AActor* InteractableActor;

	virtual void ExecuteInteraction(EInteractionDomain Domain, float DeltaTime) override;
	
#if WITH_EDITOR
	virtual TArray<AActor*> GetWorldReferences() const override;
	virtual EDataValidationResult IsBehaviorValid(EInteractionDomain InteractionDomain, FDataValidationContext& Context) override;
#endif 
};

enum class EInteractionLockStatus : uint8;

UENUM()
enum class EInteractionBehaviorLockRetrigger
{
	Nothing,
	FlipFlop
};


UCLASS(DisplayName="Lock/Unlock Interactable", MinimalAPI)
class UInteractionBehavior_SetInteractionLockStatus : public UInteractionBehavior
{
	GENERATED_BODY()

protected:
	UPROPERTY(EditAnywhere)
	EInteractionLockStatus TargetLockStatus;
	UPROPERTY(EditAnywhere)
	EInteractionBehaviorLockRetrigger RetriggerBehavior = EInteractionBehaviorLockRetrigger::Nothing;
	UPROPERTY(EditAnywhere)
	bool bInitActorToOppositeStatus = true;
	UPROPERTY(EditAnywhere)
	TObjectPtr<AActor> InteractableActor;

	virtual void Initialize(UInteractableComponent* OwnerInteractable) override;
	virtual void ExecuteInteraction(EInteractionDomain Domain, float DeltaTime) override;

#if WITH_EDITOR 
	virtual EDataValidationResult IsBehaviorValid(EInteractionDomain InteractionDomain, FDataValidationContext& Context) override;
	virtual TArray<AActor*> GetWorldReferences() const override { return {InteractableActor}; }
#endif
};