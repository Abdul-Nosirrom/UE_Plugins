// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "RootMotionTask_Base.h"
#include "UObject/Object.h"
#include "RootMotionTask_JumpForce.generated.h"

class UCustomMovementComponent;
class UCurveFloat;
class UCurveVector;
class AActor;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FApplyRootMotionJumpForceDelegate);


UCLASS()
class COREFRAMEWORK_API URootMotionTask_JumpForce : public URootMotionTask_Base
{
	GENERATED_BODY()

	URootMotionTask_JumpForce();
	
	UPROPERTY(BlueprintAssignable)
	FApplyRootMotionJumpForceDelegate OnFinish;

	UPROPERTY(BlueprintAssignable)
	FApplyRootMotionJumpForceDelegate OnLanded;

	UFUNCTION(BlueprintCallable, Category="Ability|Tasks")
	void Finish();

	UFUNCTION()
	void OnLandedCallback(const FHitResult& Hit);

	/** Apply force to character's movement */
	UFUNCTION(BlueprintCallable, Category = "Root Motion|Tasks", meta = (BlueprintInternalUseOnly = "TRUE"))
	static URootMotionTask_JumpForce* ApplyRootMotionJumpForce(AOPCharacter* Owner, FName TaskInstanceName, FRotator Rotation, float Distance, float Height, float Duration, float MinimumLandedTriggerTime,
		bool bFinishOnLanded, ERootMotionFinishVelocityMode VelocityOnFinishMode, FVector SetVelocityOnFinish, float ClampVelocityOnFinish, UCurveVector* PathOffsetCurve, UCurveFloat* TimeMappingCurve);

	virtual void Activate() override;

	/** Tick function for this task, if bTickingTask == true */
	virtual void TickTask(float DeltaTime) override;

	virtual void PreDestroyFromReplication() override;
	virtual void OnDestroy() override;

protected:

	virtual void SharedInitAndApply() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override {}

	/**
	* Work-around for OnLanded being called during bClientUpdating in movement replay code
	* Don't want to trigger our Landed logic during a replay, so we wait until next frame
	* If we don't, we end up removing root motion from a replay root motion set instead
	* of the real one
	*/
	void TriggerLanded();

protected:

	UPROPERTY()
	FRotator Rotation;

	UPROPERTY()
	float Distance;

	UPROPERTY()
	float Height;

	UPROPERTY()
	float Duration;

	UPROPERTY()
	float MinimumLandedTriggerTime;

	UPROPERTY()
	bool bFinishOnLanded;

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

	bool bHasLanded;
};
