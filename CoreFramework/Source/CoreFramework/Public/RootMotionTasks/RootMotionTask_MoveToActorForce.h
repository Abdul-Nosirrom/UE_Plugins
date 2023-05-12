// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "RootMotionTask_Base.h"
#include "UObject/Object.h"
#include "RootMotionTask_MoveToActorForce.generated.h"

class ARadicalCharacter;
class URadicalMovementComponent;
class UCurveFloat;
class UCurveVector;
enum EMovementState;
enum class ERootMotionFinishVelocityMode : uint8;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FRootMotionTaskMoveToActorForceDelegate, bool, DestinationReached, bool, TimedOut, FVector, FinalTargetLocation);

class AActor;

UENUM()
enum class ERootMotionTaskMoveToActorTargetOffsetType : uint8
{
	// Align target offset vector from target to source, ignoring height difference
	AlignFromTargetToSource = 0,
	// Align from target actor location to target actor forward
	AlignToTargetForward,
	// Align in world space
	AlignToWorldSpace
};

UCLASS()
class COREFRAMEWORK_API URootMotionTask_MoveToActorForce : public URootMotionTask_Base
{
	GENERATED_BODY()

	URootMotionTask_MoveToActorForce();
	
	UPROPERTY(BlueprintAssignable)
	FRootMotionTaskMoveToActorForceDelegate OnFinished;

	/* Apply force to character's movement */
	UFUNCTION(BlueprintCallable, Category = "Root Motion|Tasks", meta = (BlueprintInternalUseOnly = "TRUE"))
	static URootMotionTask_MoveToActorForce* ApplyRootMotionMoveToActorForce(ARadicalCharacter* Owner, FName TaskInstanceName, AActor* TargetActor, FVector TargetLocationOffset, ERootMotionTaskMoveToActorTargetOffsetType OffsetAlignment, float Duration, UCurveFloat* TargetLerpSpeedHorizontal, UCurveFloat* TargetLerpSpeedVertical, bool bSetNewMovementMode, EMovementState MovementMode, bool bRestrictSpeedToExpected, bool bApplyCurveInLocalSpace, UCurveVector* PathOffsetCurve, UCurveFloat* TimeMappingCurve, ERootMotionFinishVelocityMode VelocityOnFinishMode, FVector SetVelocityOnFinish, float ClampVelocityOnFinish, bool bDisableDestinationReachedInterrupt);
	
	virtual void TickTask(float DeltaTime) override;

	virtual void OnDestroy() override;

protected:

	virtual void SharedInitAndApply() override;

	bool UpdateTargetLocation(float DeltaTime);

	void SetRootMotionTargetLocation(FVector NewTargetLocation);

	FVector CalculateTargetOffset() const;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override {}

protected:

	FDelegateHandle TargetActorSwapHandle;

	UPROPERTY()
	FVector StartLocation;

	UPROPERTY()
	FVector TargetLocation;

	UPROPERTY()
	TObjectPtr<AActor> TargetActor;

	UPROPERTY()
	FVector TargetLocationOffset;

	UPROPERTY()
	ERootMotionTaskMoveToActorTargetOffsetType OffsetAlignment;

	UPROPERTY()
	float Duration;

	/** By default, this force ends when the destination is reached. Using this parameter you can disable it so it will not 
	 *  "early out" and get interrupted by reaching the destination and instead go to its full duration. */
	UPROPERTY()
	bool bDisableDestinationReachedInterrupt;

	UPROPERTY()
	bool bSetNewMovementMode;

	UPROPERTY()
	TEnumAsByte<EMovementState> NewMovementMode;

	/** If enabled, we limit velocity to the initial expected velocity to go distance to the target over Duration.
	 *  This prevents cases of getting really high velocity the last few frames of the root motion if you were being blocked by
	 *  collision. Disabled means we do everything we can to velocity during the move to get to the TargetLocation. */
	UPROPERTY()
	bool bRestrictSpeedToExpected;

	UPROPERTY()
	bool bApplyCurveInLocalSpace;

	UPROPERTY()
	TObjectPtr<UCurveVector> PathOffsetCurve;

	/** 
	 *  Maps real time to movement fraction curve to affect the speed of the
	 *  movement through the path
	 *  Curve X is 0 to 1 normalized real time (a fraction of the duration)
	 *  Curve Y is 0 to 1 is what percent of the move should be at a given X
	 *  Default if unset is a 1:1 correspondence
	 */
	UPROPERTY()
	TObjectPtr<UCurveFloat> TimeMappingCurve;

	UPROPERTY()
	TObjectPtr<UCurveFloat> TargetLerpSpeedHorizontalCurve;

	UPROPERTY()
	TObjectPtr<UCurveFloat> TargetLerpSpeedVerticalCurve;

	EMovementState PreviousMovementMode;
	uint8 PreviousCustomMode;
};
