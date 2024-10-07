// Copyright 2023 CoC All rights reserved

#pragma once

#include "CoreMinimal.h"
#include "InteractionBehavior.h"
#include "Engine/TargetPoint.h"
#include "InteractionBehaviors_ActorsBasic.generated.h"

/**
 * 
 */
UCLASS(DisplayName="Spawn Actor", MinimalAPI)
class UInteractionBehavior_SpawnActor : public UInteractionBehavior
{
	GENERATED_BODY()

protected:
	UPROPERTY(EditAnywhere)
	TSubclassOf<AActor> ActorToSpawn;
	UPROPERTY(EditAnywhere)
	ATargetPoint* SpawnTransform;

	virtual void ExecuteInteraction(EInteractionDomain Domain, float DeltaTime) override;

#if WITH_EDITOR 
	virtual TArray<AActor*> GetWorldReferences() const override { return {SpawnTransform}; }
#endif
};

UCLASS(DisplayName="Destroy Actor", MinimalAPI)
class UInteractionBehavior_DestroyActor : public UInteractionBehavior
{
	GENERATED_BODY()

protected:
	UPROPERTY(EditAnywhere)
	TObjectPtr<AActor> ActorToDestroy;

	virtual void ExecuteInteraction(EInteractionDomain Domain, float DeltaTime) override;

#if WITH_EDITOR 
	virtual TArray<AActor*> GetWorldReferences() const override { return {ActorToDestroy}; }
#endif
};

UCLASS(DisplayName="Enable/Disable Actor", MinimalAPI)
class UInteractionBehavior_SetActorEnable : public UInteractionBehavior
{
	GENERATED_BODY()

protected:
	UPROPERTY(EditAnywhere)
	bool bInitToDisabled = true;
	UPROPERTY(EditAnywhere)
	bool bFlipFlopOnRetrigger = false;
	UPROPERTY(EditAnywhere)
	TObjectPtr<AActor> TargetActor;

	virtual void Initialize(UInteractableComponent* OwnerInteractable) override;
	virtual void ExecuteInteraction(EInteractionDomain Domain, float DeltaTime) override;

	void SetActorEnabled(bool bEnabled);
	
	bool bTargetEnableState;
	bool bActorCachedTickState;
	
#if WITH_EDITOR 
	virtual TArray<AActor*> GetWorldReferences() const override { return {TargetActor}; }
#endif
};