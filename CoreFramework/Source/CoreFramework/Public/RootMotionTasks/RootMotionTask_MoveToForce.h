// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "RootMotionTask_Base.h"
#include "UObject/Object.h"
#include "RootMotionTask_MoveToForce.generated.h"

class URadicalMovementComponent;
class ARadicalCharacter;
class UCurveVector;
enum class ERootMotionFinishVelocityMode : uint8;
enum EMovementState;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FApplyRootMotionMoveToForceDelegate);


UCLASS()
class COREFRAMEWORK_API URootMotionTask_MoveToForce : public URootMotionTask_Base
{
	GENERATED_BODY()

	UPROPERTY(BlueprintAssignable)
	FApplyRootMotionMoveToForceDelegate OnTimedOut;

	UPROPERTY(BlueprintAssignable)
	FApplyRootMotionMoveToForceDelegate OnTimedOutAndDestinationReached;

	/** Apply force to character's movement */
	UFUNCTION(BlueprintCallable, Category = "Root Motion|Tasks", meta = (BlueprintInternalUseOnly = "TRUE"))
	static URootMotionTask_MoveToForce* ApplyRootMotionMoveToForce(ARadicalCharacter* Owner, FName TaskInstanceName, FVector TargetLocation, float Duration, bool bSetNewMovementMode, EMovementState MovementMode, bool bRestrictSpeedToExpected, UCurveVector* PathOffsetCurve, ERootMotionFinishVelocityMode VelocityOnFinishMode, FVector SetVelocityOnFinish, float ClampVelocityOnFinish);

	/** Tick function for this task, if bTickingTask == true */
	virtual void TickTask(float DeltaTime) override;

	virtual void OnDestroy() override;

protected:

	virtual void SharedInitAndApply() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override {}

protected:

	UPROPERTY(Replicated)
	FVector StartLocation;

	UPROPERTY(Replicated)
	FVector TargetLocation;

	UPROPERTY(Replicated)
	float Duration;

	UPROPERTY(Replicated)
	bool bSetNewMovementMode = false;

	UPROPERTY(Replicated)
	TEnumAsByte<EMovementState> NewMovementMode;

	/** If enabled, we limit velocity to the initial expected velocity to go distance to the target over Duration.
	 *  This prevents cases of getting really high velocity the last few frames of the root motion if you were being blocked by
	 *  collision. Disabled means we do everything we can to velocity during the move to get to the TargetLocation. */
	UPROPERTY(Replicated)
	bool bRestrictSpeedToExpected = false;

	UPROPERTY(Replicated)
	TObjectPtr<UCurveVector> PathOffsetCurve = nullptr;

	EMovementState PreviousMovementMode;
	uint8 PreviousCustomMovementMode = 0;};
