// Copyright 2023 CoC All rights reserved

#pragma once

#include "CoreMinimal.h"
#include "MovementData.h"
#include "TimerManager.h"
#include "Curves/CurveVector.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "DynamicMontageBase.generated.h"

/* Forward Declarations */
class URadicalMovementComponent;
class ARadicalCharacter;
class UAnimMontage;

/* Execution pin signatures */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FDynamicMontageComplete);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDynamicMontageInterrupted, const FGameplayTagContainer&, InterruptTag);

/* Custom Event Type */
DECLARE_DYNAMIC_DELEGATE_ThreeParams(FDynamicMontageCustomVel, FVector, Velocity, float, DeltaTime, float, NormalizedTime);
DECLARE_DYNAMIC_DELEGATE_ThreeParams(FDynamicMontageCustomRotation, FRotator, Rotation, float, DeltaTime, float, NormalizedTime);

/* Data-Keeping Events (When another plays, it must call this first if being played on the same RadicalCharacter before it binds to events) */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FPostProccessEventsRemoved);

/* Action Types */ // NOTE: Custom can go through an execution pin through an event ^
UENUM(BlueprintType)
enum class EDynamicMontageVelocity : uint8
{
	None,
	Default,
	Custom,
	AccelCurve,
	VelCurve
};

UENUM(BlueprintType)
enum class EDynamicMontageVectorBasis : uint8
{
	World,
	Player,
	PlayerZUp
};

UENUM(BlueprintType)
enum class EDynamicMontageRotation : uint8
{
	None,
	OrientToInput,
	OrientToGroundAndInput,
	OrientToVelocity,
	OrientToGroundAndVelocity,
	RotationCurve,
	Custom
};

USTRUCT(BlueprintType)
struct FDynamicMontageCustomMovementCalculations
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite)
	FVector Velocity;
	UPROPERTY(BlueprintReadWrite)
	FQuat Rotation;
};

UCLASS()
class ACTIONFRAMEWORK_API UDynamicMontageBase : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

public:

	UPROPERTY(BlueprintAssignable)
	FDynamicMontageComplete OnComplete;
	UPROPERTY(BlueprintAssignable)
	FDynamicMontageInterrupted OnInterrupted;
	
	UFUNCTION(Category=Animation, BlueprintCallable, meta=(BlueprintInternalUseOnly = "true"))
	static UDynamicMontageBase* PlayDynamicMontage(
		ARadicalCharacter* Character,
		FGameplayTagContainer CancelActionsWithTag,
		FGameplayTagContainer BlockActionsWithTag,
		FGameplayTagContainer ActionsThatCanCancel, // These tags do block actions, but then unblock when we can cancel. Blocked actions remain blocked throughout
		UAnimMontage* Montage,
		float& MontageLength,
		float CanCancelAfterSeconds = -1.f,
		float PlayRate = 1.f,
		EDynamicMontageVelocity VelMethod = EDynamicMontageVelocity::Default,
		FRuntimeVectorCurve VectorCurve = FRuntimeVectorCurve(),
		EDynamicMontageVectorBasis VectorBasis = EDynamicMontageVectorBasis::PlayerZUp,
		bool bIsAdditive = true,
		bool bApplyGravity = true,
		float GravityScale = 1.f,
		EDynamicMontageRotation RotMethod = EDynamicMontageRotation::OrientToGroundAndVelocity,
		FRuntimeVectorCurve RotationCurve = FRuntimeVectorCurve(),
		float RotationRate = 5.f,
		bool bBlendMovement = true,
		bool bUnbindMovementOnBlendOut = true);

	UFUNCTION(Category=Animation, BlueprintCallable, meta=(BlueprintInternalUseOnly = "true"))
	static UDynamicMontageBase* PlayDynamicMontageAction(
		ARadicalCharacter* Character,
		FGameplayTagContainer CancelActionsWithTag,
		FGameplayTagContainer BlockActionsWithTag,
		FGameplayTagContainer ActionsThatCanCancel, 
		UAnimMontage* Montage,
		float& MontageLength,
		FDynamicMontageCustomMovementCalculations& CustomCalculations,
		FDynamicMontageCustomVel CustomVelDelegate,
		FDynamicMontageCustomRotation CustomRotDelegate,
		float CanCancelAfterSeconds = -1.f,
		float PlayRate = 1.f,
		ERotationMethod RotMethod = METHOD_OrientToGroundAndVelocity,
		bool bBlendMovement = true);

	// Overriding BP Async action base
	virtual void Activate() override;

protected:

	/// @brief Individual objects should call this when they start, to notify others that are running and about to be overridden to stop
	static FPostProccessEventsRemoved OnDynamicMontageStarted;

	UPROPERTY(Transient)
	ARadicalCharacter* CharContext;
	UPROPERTY()
	UAnimMontage* Montage;

	FGameplayTagContainer BlockedActions;
	FGameplayTagContainer CancelledActions;
	FGameplayTagContainer CancelWhenTagsGranted;

	FDynamicMontageCustomVel CustomVelDelegate;
	FDynamicMontageCustomRotation CustomRotDelegate;
	
	float PlayRate;
	EDynamicMontageVelocity VelMethod;
	EDynamicMontageRotation RotMethod;
	EDynamicMontageVectorBasis VectorBasis;
	FRuntimeVectorCurve VelMethodCurve;
	FRuntimeVectorCurve RotMethodCurve;
	bool bIsAdditive;
	bool bBlendMovement;
	bool bUnbindMovementOnBlendOut;

	FRotator StartingRotation;
	
	FOnMontageBlendingOutStarted BlendingOutDelegate;
	FOnMontageEnded MontageEndedDelegate;

	float MontageLength;
	FVector VelocityOnStart;

	bool bBlendingOut;
	bool bCanCancel;
	float CanCancelAfterSeconds;

	bool bApplyGravity;
	float GravityScale;

	float RotationRate;

	FTimerDelegate CancelTimerDelegate;
	FTimerHandle CancelTimerHandle;

	FDynamicMontageCustomMovementCalculations DynamicCalculations;
	
	/// @brief Depending on when this is called (right after a new montage is played), then its a good place to reset PostProcess events
	UFUNCTION()
	void OnMontageBlendingOut(UAnimMontage* AnimMontage, bool bInterrupted);
	UFUNCTION()
	void OnMontageCompleted(UAnimMontage* AnimMontage, bool bInterrupted);

	void CalcRMVelocity(URadicalMovementComponent* MovementComponent, float DeltaTime);
	void UpdateRMRotation(URadicalMovementComponent* MovementComponent, float DeltaTime);

	UFUNCTION()
	void OnGameplayTagsAdded(FGameplayTagContainer TagsAdded);

	float GetTimeSinceStarted();
	
private:
	float TimeStarted;

	ERotationMethod TranslateRotationMethod() const
	{
		switch (RotMethod)
		{
			case EDynamicMontageRotation::OrientToInput: return METHOD_OrientToInput;
			case EDynamicMontageRotation::OrientToGroundAndInput: return METHOD_OrientToGroundAndInput;
			case EDynamicMontageRotation::OrientToVelocity: return METHOD_OrientToVelocity;
			case EDynamicMontageRotation::OrientToGroundAndVelocity: return METHOD_OrientToGroundAndVelocity;
		}

		return ERotationMethod::METHOD_OrientToGroundAndVelocity;
	}

	EDynamicMontageRotation TranslateDynamicRotationMethod(const ERotationMethod InMethod) const
	{
		switch (InMethod)
		{
			case METHOD_OrientToInput: return EDynamicMontageRotation::OrientToInput;
			case METHOD_OrientToGroundAndInput: return EDynamicMontageRotation::OrientToGroundAndInput;
			case METHOD_OrientToVelocity: return EDynamicMontageRotation::OrientToVelocity;
			case METHOD_OrientToGroundAndVelocity: return EDynamicMontageRotation::OrientToGroundAndVelocity;
		}

		return EDynamicMontageRotation::Custom;
	}
	
	void PerformCleanup();

	void UnbindEvents();
	
	void Failed(const TCHAR* Reason);
};
