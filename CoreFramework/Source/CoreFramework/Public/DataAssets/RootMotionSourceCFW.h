// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/RootMotionSource.h"
#include "RootMotionSourceCFW.generated.h"

/* FORWARD DECLARATIONS */
class ARadicalCharacter;
class URadicalMovementComponent;
class URootMotionTask_Base;

#define ROOT_MOTION_DEBUG (1 && !(UE_BUILD_SHIPPING || UE_BUILD_TEST))

#if ROOT_MOTION_DEBUG
struct COREFRAMEWORK_API RootMotionSourceCFWDebug
{
	static TAutoConsoleVariable<int32> CVarDebugRootMotionSources;
	static void PrintOnScreen(const ARadicalCharacter& InCharacter, const FString& InString);
	static void PrintOnScreenServerMsg(const FString& InString);
};
#endif

USTRUCT()
struct COREFRAMEWORK_API FRootMotionSourceCFW : public FRootMotionSource
{
	GENERATED_BODY()

	virtual void PrepareCustomRootMotion(float SimulationTime, float MovementTickTime, const ARadicalCharacter& Character, const URadicalMovementComponent& MoveComponent);

	virtual void AddReferencedObjects(FReferenceCollector& Collector) override {}

	UPROPERTY()
	TObjectPtr<URootMotionTask_Base> AssociatedTask;
};

USTRUCT()
struct COREFRAMEWORK_API FRootMotionSourceGroupCFW : public FRootMotionSourceGroup
{
	GENERATED_BODY()

	void CleanUpInvalidRootMotion(float DeltaTime, const ARadicalCharacter& Character, URadicalMovementComponent& MoveComponent);

	/** 
	 *  Generates root motion by accumulating transforms through current root motion sources. 
	 *  @param bForcePrepareAll - Used during "live" PerformMovements() to ensure all sources get prepared
	 *                            Needed due to SavedMove playback/server correction only applying corrections to
	 *                            Sources that need updating, so in that case we only Prepare those that need it.
	 */
	void PrepareRootMotion(float DeltaTime, const ARadicalCharacter& Character, const URadicalMovementComponent& InMoveComponent, bool bForcePrepareAll = false);

	/**  Helper function for accumulating override velocity into InOutVelocity */
	void AccumulateOverrideRootMotionVelocity(float DeltaTime, const ARadicalCharacter& Character, const URadicalMovementComponent& MoveComponent, FVector& InOutVelocity) const;

	/**  Helper function for accumulating additive velocity into InOutVelocity */
	void AccumulateAdditiveRootMotionVelocity(float DeltaTime, const ARadicalCharacter& Character, const URadicalMovementComponent& MoveComponent, FVector& InOutVelocity) const;

	/** Get rotation output of current override root motion source, returns true if OutRotation was filled */
	bool GetOverrideRootMotionRotation(float DeltaTime, const ARadicalCharacter& Character, const URadicalMovementComponent& MoveComponent, FQuat& OutRotation) const;
	
	void ClearAndDestroy(); 
	
protected:

	/** Accumulates contributions for velocity into InOutVelocity for a given type of root motion from this group */
	void AccumulateRootMotionVelocity(ERootMotionAccumulateMode RootMotionType, float DeltaTime, const ARadicalCharacter& Character, const URadicalMovementComponent& MoveComponent, FVector& InOutVelocity) const;

	/** Accumulates contributions for velocity into InOutVelocity for a given type of root motion from this group */
	void AccumulateRootMotionVelocityFromSource(const FRootMotionSource& RootMotionSource, float DeltaTime, const ARadicalCharacter& Character, const URadicalMovementComponent& MoveComponent, FVector& InOutVelocity) const;

};

/** ConstantForce applies a fixed force to the target */
USTRUCT()
struct COREFRAMEWORK_API FRootMotionSourceCFW_ConstantForce : public FRootMotionSourceCFW
{
	GENERATED_USTRUCT_BODY()

	FRootMotionSourceCFW_ConstantForce();

	virtual ~FRootMotionSourceCFW_ConstantForce() {}

	UPROPERTY()
	FVector Force;

	UPROPERTY()
	TObjectPtr<UCurveFloat> StrengthOverTime;

	virtual FRootMotionSourceCFW* Clone() const override;

	virtual bool Matches(const FRootMotionSource* Other) const override;

	virtual bool MatchesAndHasSameState(const FRootMotionSource* Other) const override;

	virtual bool UpdateStateFrom(const FRootMotionSource* SourceToTakeStateFrom, bool bMarkForSimulatedCatchup = false) override;

	virtual void PrepareCustomRootMotion(
		float SimulationTime, 
		float MovementTickTime,
		const ARadicalCharacter& Character, 
		const URadicalMovementComponent& MoveComponent
		) override;
	
	virtual UScriptStruct* GetScriptStruct() const override;

	virtual FString ToSimpleString() const override;

	virtual void AddReferencedObjects(class FReferenceCollector& Collector) override;
};

/** RadialForce applies a force pulling or pushing away from a given world location to the target */
USTRUCT()
struct COREFRAMEWORK_API FRootMotionSourceCFW_RadialForce : public FRootMotionSourceCFW
{
	GENERATED_BODY()

	FRootMotionSourceCFW_RadialForce();

	virtual ~FRootMotionSourceCFW_RadialForce() {}

	UPROPERTY()
	FVector Location;

	UPROPERTY()
	TObjectPtr<AActor> LocationActor;

	UPROPERTY()
	float Radius;

	UPROPERTY()
	float Strength;

	UPROPERTY()
	bool bIsPush;

	UPROPERTY()
	bool bNoZForce;

	UPROPERTY()
	TObjectPtr<UCurveFloat> StrengthDistanceFalloff;

	UPROPERTY()
	TObjectPtr<UCurveFloat> StrengthOverTime;

	UPROPERTY()
	bool bUseFixedWorldDirection;

	UPROPERTY()
	FRotator FixedWorldDirection;

	virtual FRootMotionSourceCFW* Clone() const override;

	virtual bool Matches(const FRootMotionSource* Other) const override;

	virtual bool MatchesAndHasSameState(const FRootMotionSource* Other) const override;

	virtual bool UpdateStateFrom(const FRootMotionSource* SourceToTakeStateFrom, bool bMarkForSimulatedCatchup = false) override;

	virtual void PrepareCustomRootMotion(
		float SimulationTime, 
		float MovementTickTime,
		const ARadicalCharacter& Character, 
		const URadicalMovementComponent& MoveComponent
		) override;
	
	virtual UScriptStruct* GetScriptStruct() const override;

	virtual FString ToSimpleString() const override;

	virtual void AddReferencedObjects(class FReferenceCollector& Collector) override;
};

/** MoveToForce moves the target to a given fixed location in world space over the duration */
USTRUCT()
struct COREFRAMEWORK_API FRootMotionSourceCFW_MoveToForce : public FRootMotionSourceCFW
{
	GENERATED_BODY()

	FRootMotionSourceCFW_MoveToForce();

	virtual ~FRootMotionSourceCFW_MoveToForce() {}

	UPROPERTY()
	FVector StartLocation;

	UPROPERTY()
	FVector TargetLocation;

	UPROPERTY()
	bool bRestrictSpeedToExpected;

	UPROPERTY()
	TObjectPtr<UCurveVector> PathOffsetCurve;

	FVector GetPathOffsetInWorldSpace(const float MoveFraction) const;

	virtual FRootMotionSource* Clone() const override;

	virtual bool Matches(const FRootMotionSource* Other) const override;

	virtual bool MatchesAndHasSameState(const FRootMotionSource* Other) const override;

	virtual bool UpdateStateFrom(const FRootMotionSource* SourceToTakeStateFrom, bool bMarkForSimulatedCatchup = false) override;

	virtual void SetTime(float NewTime) override;

	virtual void PrepareCustomRootMotion(
		float SimulationTime, 
		float MovementTickTime,
		const ARadicalCharacter& Character, 
		const URadicalMovementComponent& MoveComponent
		) override;
	
	virtual UScriptStruct* GetScriptStruct() const override;

	virtual FString ToSimpleString() const override;

	virtual void AddReferencedObjects(class FReferenceCollector& Collector) override;
	
};


/** 
 * MoveToDynamicForce moves the target to a given location in world space over the duration, where the end location
 * is dynamic and can change during the move (meant to be used for things like moving to a moving target)
 */
USTRUCT()
struct COREFRAMEWORK_API FRootMotionSourceCFW_MoveToDynamicForce : public FRootMotionSourceCFW
{
	GENERATED_USTRUCT_BODY()

	FRootMotionSourceCFW_MoveToDynamicForce();

	virtual ~FRootMotionSourceCFW_MoveToDynamicForce() {}

	UPROPERTY()
	FVector StartLocation;

	UPROPERTY()
	FVector InitialTargetLocation;

	UPROPERTY()
	FVector TargetLocation;

	UPROPERTY()
	bool bRestrictSpeedToExpected;

	UPROPERTY()
	TObjectPtr<UCurveVector> PathOffsetCurve;

	UPROPERTY()
	TObjectPtr<UCurveFloat> TimeMappingCurve;

	void SetTargetLocation(FVector NewTargetLocation);

	FVector GetPathOffsetInWorldSpace(const float MoveFraction) const;

	virtual FRootMotionSource* Clone() const override;

	virtual bool Matches(const FRootMotionSource* Other) const override;

	virtual bool MatchesAndHasSameState(const FRootMotionSource* Other) const override;

	virtual bool UpdateStateFrom(const FRootMotionSource* SourceToTakeStateFrom, bool bMarkForSimulatedCatchup = false) override;

	virtual void SetTime(float NewTime) override;

	virtual void PrepareCustomRootMotion(
		float SimulationTime, 
		float MovementTickTime,
		const ARadicalCharacter& Character, 
		const URadicalMovementComponent& MoveComponent
		) override;


	virtual UScriptStruct* GetScriptStruct() const override;

	virtual FString ToSimpleString() const override;

	virtual void AddReferencedObjects(class FReferenceCollector& Collector) override;
};


USTRUCT()
struct COREFRAMEWORK_API FRootMotionSourceCFW_JumpForce : public FRootMotionSourceCFW
{
	GENERATED_BODY()

	FRootMotionSourceCFW_JumpForce();

	virtual ~FRootMotionSourceCFW_JumpForce() {}

	UPROPERTY()
	FRotator Rotation;

	UPROPERTY()
	float Distance;

	UPROPERTY()
	float Height;

	UPROPERTY()
	bool bDisableTimeout;

	UPROPERTY()
	TObjectPtr<UCurveVector> PathOffsetCurve;

	UPROPERTY()
	TObjectPtr<UCurveFloat> TimeMappingCurve;

	FVector SavedHalfwayLocation;

	FVector GetPathOffset(float MoveFraction) const;

	FVector GetRelativeLocation(float MoveFraction) const;

	virtual bool IsTimeOutEnabled() const override;

	virtual FRootMotionSource* Clone() const override;

	virtual bool Matches(const FRootMotionSource* Other) const override;

	virtual bool MatchesAndHasSameState(const FRootMotionSource* Other) const override;

	virtual bool UpdateStateFrom(const FRootMotionSource* SourceToTakeStateFrom, bool bMarkForSimulatedCatchup = false) override;

	virtual void PrepareCustomRootMotion(
		float SimulationTime, 
		float MovementTickTime,
		const ARadicalCharacter& Character, 
		const URadicalMovementComponent& MoveComponent
		) override;


	virtual UScriptStruct* GetScriptStruct() const override;

	virtual FString ToSimpleString() const override;

	virtual void AddReferencedObjects(class FReferenceCollector& Collector) override;

};