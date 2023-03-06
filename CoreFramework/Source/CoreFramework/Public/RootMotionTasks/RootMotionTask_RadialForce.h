// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "RootMotionTask_Base.h"
#include "UObject/Object.h"
#include "RootMotionTask_RadialForce.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FApplyRootMotionRadialForceDelegate);

UCLASS()
class COREFRAMEWORK_API URootMotionTask_RadialForce : public URootMotionTask_Base
{
	GENERATED_BODY()

	UPROPERTY(BlueprintAssignable)
	FApplyRootMotionRadialForceDelegate OnFinish;

	/** Apply force to character's movement */
	UFUNCTION(BlueprintCallable, Category = "Root Motion|Tasks", meta = (BlueprintInternalUseOnly = "TRUE"))
	static URootMotionTask_RadialForce* ApplyRootMotionRadialForce(ARadicalCharacter* Owner, FName TaskInstanceName, FVector Location, AActor* LocationActor, float Strength, float Duration, float Radius, bool bIsPush, bool bIsAdditive, bool bNoZForce, UCurveFloat* StrengthDistanceFalloff, UCurveFloat* StrengthOverTime, bool bUseFixedWorldDirection, FRotator FixedWorldDirection, ERootMotionFinishVelocityMode VelocityOnFinishMode, FVector SetVelocityOnFinish, float ClampVelocityOnFinish);

	/** Tick function for this task, if bTickingTask == true */
	virtual void TickTask(float DeltaTime) override;

	virtual void OnDestroy() override;

protected:

	virtual void SharedInitAndApply() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override {}

protected:

	UPROPERTY()
	FVector Location;

	UPROPERTY()
	TObjectPtr<AActor> LocationActor;

	UPROPERTY()
	float Strength;

	UPROPERTY()
	float Duration;

	UPROPERTY()
	float Radius;

	UPROPERTY()
	bool bIsPush;

	UPROPERTY()
	bool bIsAdditive;

	UPROPERTY()
	bool bNoZForce;

	/** 
	 *  Strength of the force over distance to Location
	 *  Curve Y is 0 to 1 which is percent of full Strength parameter to apply
	 *  Curve X is 0 to 1 normalized distance (0 = 0cm, 1 = what Strength % at Radius units out)
	 */
	UPROPERTY(Replicated)
	TObjectPtr<UCurveFloat> StrengthDistanceFalloff;

	/** 
	 *  Strength of the force over time
	 *  Curve Y is 0 to 1 which is percent of full Strength parameter to apply
	 *  Curve X is 0 to 1 normalized time if this force has a limited duration (Duration > 0), or
	 *          is in units of seconds if this force has unlimited duration (Duration < 0)
	 */
	UPROPERTY(Replicated)
	TObjectPtr<UCurveFloat> StrengthOverTime;

	UPROPERTY(Replicated)
	bool bUseFixedWorldDirection;

	UPROPERTY(Replicated)
	FRotator FixedWorldDirection;
};
