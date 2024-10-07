// Copyright 2023 CoC All rights reserved

#pragma once

#include "CoreMinimal.h"
#include "Behaviors/InteractionBehavior.h"
#include "InteractionBehaviors_MoveAndRotation.generated.h"


UENUM()
enum class EInteractRotateRetriggerBehavior
{
	Stop,
	Reverse
};

UCLASS(DisplayName="Rotate Actor [Advanced]")
class INTERACTIONFRAMEWORK_API UInteractionBehavior_RotateActorAdvanced : public UInteractionBehavior
{
	GENERATED_BODY()

protected:
	/// @brief	Duration of the rotation. If <= 0, then infinite
	UPROPERTY(EditAnywhere)
	float Duration = -1.f;
	UPROPERTY(EditAnywhere)
	EInteractRotateRetriggerBehavior RetriggerBehavior;
	UPROPERTY(EditAnywhere)
	AActor* ActorToRotate;
	UPROPERTY(EditAnywhere)
	FRotator RotationRate;
	UPROPERTY(EditAnywhere)
	FVector LocalPivotLocation;
	UPROPERTY(EditAnywhere)
	bool bRotationInLocalSpace = true;

	uint32 ActivationCount;
	
	virtual void Initialize(UInteractableComponent* OwnerInteractable) override;
	virtual void InteractionStarted() override { Super::InteractionStarted(); ActivationCount++; }
	virtual void ExecuteInteraction(EInteractionDomain Domain, float DeltaTime) override;

#if WITH_EDITOR
	virtual TArray<AActor*> GetWorldReferences() const override { return {ActorToRotate}; }
	virtual EDataValidationResult IsBehaviorValid(EInteractionDomain InteractionDomain, FDataValidationContext& Context) override;
#endif 
};


UCLASS(DisplayName="Rotate Actor [Simple]")
class UInteractionBehavior_RotateActorSimple : public UInteractionBehavior
{
	GENERATED_BODY()

protected:
	UPROPERTY(EditAnywhere)
	float RotationTime = 1.f;
	UPROPERTY(EditAnywhere)
	AActor* ActorToRotate;
	UPROPERTY(EditAnywhere)
	FRotator FromRotation;
	UPROPERTY(EditAnywhere)
	FRotator ToRotation;

	FRotator TargetRotation;
	float CurrentTime;

	virtual void Initialize(UInteractableComponent* OwnerInteractable) override;
	virtual void InteractionStarted() override;
	virtual void ExecuteInteraction(EInteractionDomain Domain, float DeltaTime) override;

#if WITH_EDITOR
	virtual TArray<AActor*> GetWorldReferences() const override { return { ActorToRotate }; }
	virtual EDataValidationResult IsBehaviorValid(EInteractionDomain InteractionDomain, FDataValidationContext& Context) override;
#endif 
};

USTRUCT()
struct FInteractionMoveSimpleParams
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	class ATargetPoint* TargetLocation;
	UPROPERTY(EditAnywhere, meta=(UIMin=0, ClampMin=0))
	float MoveTime;
	UPROPERTY(EditAnywhere)
	bool bEaseOut;
	UPROPERTY(EditAnywhere)
	bool bEaseIn;
};

UENUM()
enum class EInteractionMoveSimpleRetrigger
{
	Reverse,
	IteratePointsAndReverse,
	IteratePointsButOnce,
	DoOnce
};

UCLASS(DisplayName="Move Actor [Simple]")
class UInteractionBehavior_MoveActorSimple : public UInteractionBehavior
{
	GENERATED_BODY()

protected:
	UPROPERTY(EditAnywhere)
	EInteractionMoveSimpleRetrigger RetriggerBehavior;
	UPROPERTY(EditAnywhere)
	AActor* ActorToMove;
	UPROPERTY(EditAnywhere)
	bool bFollowRotation;
	UPROPERTY(EditAnywhere, meta=(ShowOnlyInnerProperties))
	TArray<FInteractionMoveSimpleParams> MoveToPoints;
	
	int CurrentPointIdx;
	bool bGoingForward;
	bool bCompletedMovingToPoint;
	bool bCompletedFullMove;

	virtual void Initialize(UInteractableComponent* OwnerInteractable) override;
	virtual void InteractionStarted() override;
	virtual void ExecuteInteraction(EInteractionDomain Domain, float DeltaTime) override;

	void OnCompletedMoveToPoint();
	
#if WITH_EDITOR
	virtual TArray<AActor*> GetWorldReferences() const override { return { ActorToMove }; }
	virtual EDataValidationResult IsBehaviorValid(EInteractionDomain InteractionDomain, FDataValidationContext& Context) override;
#endif 
};