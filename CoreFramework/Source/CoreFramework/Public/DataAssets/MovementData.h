// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "MovementData.generated.h"

/*~~~~~ Forward Declarations ~~~~~*/
class UCustomMovementComponent;
enum EMovementState;

UENUM(BlueprintType)
enum EAccelerationMethod
{
	METHOD_Default						UMETA(DisplayName="Default"),

	METHOD_Legacy						UMETA(DisplayName="Legacy")
};


UENUM(BlueprintType)
enum ERotationMethod
{
	METHOD_OrientToMovement				UMETA(DisplayName="Orient To Movement"),

	METHOD_ControllerDesiredRotation	UMETA(DisplayName="Orient To Controller"),

	METHOD_ThirdRando					UMETA(DisplayName="I swear to god...")
};

/**
 * 
 */
UCLASS(BlueprintType)
class COREFRAMEWORK_API UMovementData : public UDataAsset
{
	GENERATED_BODY()

	UMovementData();

public:
	UFUNCTION()
	void CalculateVelocity(UCustomMovementComponent* MovementComponent, float DeltaTime);
	UFUNCTION()
	void UpdateRotation(UCustomMovementComponent* MovementComponent, float DeltaTime);

protected:
	static constexpr float MIN_DELTA_TIME = 1e-6;
	static constexpr float BRAKE_TO_STOP_VELOCITY = 10.f;

#pragma region ACCELERATION_METHODS
public:
	UPROPERTY(Category="Acceleration Data | Core", EditDefaultsOnly)
	TEnumAsByte<EAccelerationMethod> AccelerationMethod;
	
	/// @brief  Gravity Direction. Will be normalized so magnitude does not matter.
	UPROPERTY(Category="Acceleration Data | Core", EditDefaultsOnly, BlueprintReadWrite)
	FVector Gravity;

	/// @brief  Scale applied to default physics volume gravity strength
	UPROPERTY(Category="Acceleration Data | Core", EditDefaultsOnly, BlueprintReadWrite, meta=(ClampMin="0", UIMin="0"))
	float GravityScale;

	float GravityZ; // Cache value from movement component as to avoid swapping it around (NOTE: GetWorld() is invalid in a data asset)

	/// @brief  
	UPROPERTY(Category="Acceleration Data | Core", EditDefaultsOnly, BlueprintReadWrite, meta=(EditCondition="AccelerationMethod==EAccelerationMethod::METHOD_Default", EditConditionHides, ClampMin="0", UIMin="0", ForceUnits="cm/s^2"))
	float MaxAcceleration;

	/// @brief  Maximum lateral speed
	UPROPERTY(Category="Acceleration Data | Core", EditDefaultsOnly, BlueprintReadWrite, meta=(EditCondition="AccelerationMethod==EAccelerationMethod::METHOD_Default", EditConditionHides, ClampMin="0", UIMin="0", ForceUnits="cm/s"))
	float MaxSpeed;

	/// @brief  Speed we should accelerate up to when walking at minimum analog stick tilt
	UPROPERTY(Category="Acceleration Data | Core", EditDefaultsOnly, BlueprintReadWrite, meta=(EditCondition="AccelerationMethod==EAccelerationMethod::METHOD_Default", EditConditionHides, ClampMin="0", UIMin="0", ForceUnits="cm/s"))
	float MinAnalogSpeed;
	
	UFUNCTION(Category="Shared Movement Data", BlueprintCallable)
	FORCEINLINE FVector GetGravityDir() const { return Gravity.GetSafeNormal(); }

	UFUNCTION(Category="Shared Movement Data", BlueprintCallable)
	FORCEINLINE FVector GetGravity() const { return Gravity.GetSafeNormal() * GravityScale; }

#pragma region ACCELERATION_METHODS: Default UE
protected:
	// Parameters

	/// @brief  Setting that affects movement control. Higher values allow faster changes in direction.
	///			If bUseSeparateBrakingFriction is false, also affects the ability to stop more quickly when braking (Zero Acceleration) where it's multiplied by BrakingFrictionFactor.
	///			When braking, this property allows you to control how much friction is applied on ground, applying an opposing force that scales with current velocity.
	///			Can be used to simulate slippery surfaces.
	UPROPERTY(Category="Acceleration Data | Grounded", EditDefaultsOnly, BlueprintReadWrite, meta=(EditCondition="AccelerationMethod==EAccelerationMethod::METHOD_Default", EditConditionHides))
	float GroundFriction;

	/// @brief  Friction to apply to lateral air movement when falling.
	///			If bUseSeparateBrakingFriction is false, also affects the ability to stop more quickly when braking (Zero Acceleration)
	UPROPERTY(Category="Acceleration Data | Aerial", EditDefaultsOnly, BlueprintReadWrite, meta=(EditCondition="AccelerationMethod==EAccelerationMethod::METHOD_Default", EditConditionHides))
	float AerialLateralFriction;

	/// @brief  If true, BrakingFriction will be used to slow a character to a stop (when there's no Acceleration).
	///			If false, braking uses the same friction passed to CalcVelocity (GroundFriction or AerialLateralFriction), multiplied
	///			by BrakingFrictionFactor. @see BrakingFriction
	UPROPERTY(Category="Acceleration Data | Shared", EditDefaultsOnly, BlueprintReadWrite, meta=(EditCondition="AccelerationMethod==EAccelerationMethod::METHOD_Default", EditConditionHides))
	bool bUseSeparateBrakingFriction;

	/// @brief  Factor used to multiply the actual value of friction when braking. Applies to any friction value currently used.
	///			@note This is 2 by default, a value of 1 gives the true drag equation
	UPROPERTY(Category="Acceleration Data | Shared", EditDefaultsOnly, BlueprintReadWrite, meta=(EditCondition="AccelerationMethod==EAccelerationMethod::METHOD_Default", EditConditionHides))
	float BrakingFrictionFactor;

	/// @brief  Friction coefficient applied when braking (Acceleration = 0 or character is exceeding max speed). Actual value used is this multiplied by BrakingFrictionFactor.
	///			When braking, this allows you to control how much friction is applied when moving, applying an opposing force that scales with current velocity (Linearly).
	///			Braking is composed of Friction (velocity-dependent drag) and a constant deceleration.
	///			@note Only used if bUseSeparateBrakingFriction is true. Otherwise, current friction (ground or aerial) is used.
	UPROPERTY(Category="Acceleration Data | Shared", EditDefaultsOnly, BlueprintReadWrite, meta=(EditCondition="AccelerationMethod==EAccelerationMethod::METHOD_Default && bUseSeparateBrakingFriction", EditConditionHides))
	float BrakingFriction;

	/// @brief  Deceleration when on ground and not applying acceleration. @see MaxAcceleration
	UPROPERTY(Category="Acceleration Data | Grounded", EditDefaultsOnly, BlueprintReadWrite, meta=(EditCondition="AccelerationMethod==EAccelerationMethod::METHOD_Default", EditConditionHides, ClampMin="0", UIMin="0", ForceUnits="cm/s^2"))
	float BrakingDecelerationGrounded;

	/// @brief  Deceleration when in air and not applying acceleration. @see MaxAcceleration
	UPROPERTY(Category="Acceleration Data | Aerial", EditDefaultsOnly, BlueprintReadWrite, meta=(EditCondition="AccelerationMethod==EAccelerationMethod::METHOD_Default", EditConditionHides, ClampMin="0", UIMin="0", ForceUnits="cm/s^2"))
	float BrakingDecelerationAerial;

	/// @brief  When in air, amount of lateral movement control available to the character.
	///			0 = no control, 1 = full control at max speed of MaxSpeed.
	UPROPERTY(Category="Acceleration Data | Aerial", EditDefaultsOnly, BlueprintReadWrite, meta=(EditCondition="AccelerationMethod==EAccelerationMethod::METHOD_Default", EditConditionHides, ClampMin="0", ClampMax="1", UIMin="0", UIMax="1"))
	float AirControl;

	/// @brief  When in air, multiplier applied to AirControl when Lateral Velocity is less than AirControlBoostVelocityThreshold.
	///			Setting this to zero will disable air control boosting. Final result is clamped to 1.
	UPROPERTY(Category="Acceleration Data | Aerial", EditDefaultsOnly, BlueprintReadWrite, meta=(EditCondition="AccelerationMethod==EAccelerationMethod::METHOD_Default", EditConditionHides, ClampMin="0", UIMin="0"))
	float AirControlBoostMultiplier;

	/// @brief  When in air, if lateral velocity magnitude is less than this value, Air Control is multiplied by AirControlBoostMultiplier.
	///			Setting this to zero will disable air control boosting.
	UPROPERTY(Category="Acceleration Data | Aerial", EditDefaultsOnly, BlueprintReadWrite, meta=(EditCondition="AccelerationMethod==EAccelerationMethod::METHOD_Default", EditConditionHides, ClampMin="0", UIMin="0", ForceUnits="cm/s"))
	float AirControlBoostVelocityThreshold;

protected:
	float GetFriction(EMovementState MoveState) const;
	float GetMaxBrakingDeceleration(EMovementState MoveState) const;

	void ApplyVelocityBraking(FVector& Velocity, float DeltaTime, float BrakingFriction, float BrakingDeceleration) const;
	//void UpdateDefaultVelocity()
	
	void CalculateDefaultVelocity(UCustomMovementComponent* MovementComponent, float DeltaTime) const;

	FVector GetFallingLateralAcceleration(const FVector& Acceleration, const FVector& Velocity, float DeltaTime) const;
	
	FVector ComputeInputAcceleration(UCustomMovementComponent* MovementComponent) const;

	void CalculateInputVelocity(const UCustomMovementComponent* MovementComponent, FVector& Velocity, FVector& Acceleration, float Friction, float BrakingDeceleration, float DeltaTime) const;

	void ApplyGravity(FVector& Velocity, float TerminalLimit, float DeltaTime) const;

#pragma endregion ACCELERATION_METHODS: Default UE

#pragma region ACCELERATION_METHODS: Legacy Unity
protected:
	// Parameters

	

protected:
	//void CalculateLegacyVelocity(FVector& Velocity, FVector& Acceleration, float DeltaTime) const;

#pragma endregion METHOD: Legacy Unity

#pragma endregion ACCELERATION_METHODS


#pragma region ROTATION_METHODS
public:
	UPROPERTY(Category="Rotation Data", EditDefaultsOnly, BlueprintReadWrite)
	TEnumAsByte<ERotationMethod> RotationMethod;
	
	UPROPERTY(Category="Rotation Data", EditDefaultsOnly, BlueprintReadWrite)
	FRotator RotationRate;

	UPROPERTY(Category="Rotation Data", EditDefaultsOnly, BlueprintReadWrite)
	bool bOrientToGroundNormal;

protected:
	FORCEINLINE float GetAxisDeltaRotation(float InAxisRotationRate, float DeltaTime) const
	{
		// Values over 360 don't do anything, see FMath::FixedTurn. However we are trying to avoid giant floats from overflowing other calculations.
		return (InAxisRotationRate >= 0.f) ? FMath::Min(InAxisRotationRate * DeltaTime, 360.f) : 360.f;
	}
	
	void PhysicsRotation(UCustomMovementComponent* MovementComponent, float DeltaTime);

	FORCEINLINE bool ShouldRemainVertical() const { return !bOrientToGroundNormal; }

#pragma region ROTATION_METHOD: Orient To Movement

	FRotator ComputeOrientToMovementRotation(const FRotator& CurrentRotation, FRotator& DeltaRotation, FVector Acceleration, float DeltaTime) const;

#pragma endregion ROTATION_METHOD: Orient To Movement

#pragma endregion ROTATION_METHODS
};
