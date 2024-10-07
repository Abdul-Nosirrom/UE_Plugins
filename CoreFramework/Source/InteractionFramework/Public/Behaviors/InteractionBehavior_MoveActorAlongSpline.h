// Copyright 2023 CoC All rights reserved

#pragma once

#include "CoreMinimal.h"
#include "InteractionBehavior.h"
#include "InteractionBehavior_MoveActorAlongSpline.generated.h"

/**
 * 
 */
UCLASS(DisplayName="Move Actor Along Spline", MinimalAPI)
class UInteractionBehavior_MoveActorAlongSpline : public UInteractionBehavior
{
	GENERATED_BODY()
protected:

	UPROPERTY(Category=Physics, EditAnywhere)
	FRuntimeFloatCurve SpeedCurve;
	UPROPERTY(Category=Physics, EditAnywhere)
	bool bFollowRotation;
	UPROPERTY(Category=References, EditAnywhere)
	AActor* TargetSplineActor;
	UPROPERTY(Category=References, EditAnywhere)
	AActor* ActorToMove;

	UPROPERTY(Transient)
	class USplineComponent* SplineComponent;

	float CurrentDistance;
	bool bGoingForward;

	virtual void Initialize(UInteractableComponent* OwnerInteractable) override;
	virtual void ExecuteInteraction(EInteractionDomain Domain, float DeltaTime) override;

	UFUNCTION()
	void OnInteractionStarted() { bGoingForward = !bGoingForward; }
#if WITH_EDITOR
	virtual TArray<AActor*> GetWorldReferences() const override;
	virtual EDataValidationResult IsBehaviorValid(EInteractionDomain InteractionDomain, FDataValidationContext& Context) override;
#endif 
};
