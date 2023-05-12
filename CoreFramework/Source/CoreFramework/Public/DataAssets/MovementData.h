// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "MovementData.generated.h"

/*~~~~~ Forward Declarations ~~~~~*/
class URadicalMovementComponent;
enum EMovementState;

UENUM(BlueprintType)
enum ERotationMethod
{
	METHOD_OrientToInput				UMETA(DisplayName="Orient To Input"),

	METHOD_OrientToGroundAndInput		UMETA(DisplayName="Orient To Ground And Input"),

	METHOD_OrientToVelocity				UMETA(DisplayName="Orient To Velocity"),

	METHOD_OrientToGroundAndVelocity	UMETA(DisplayName="Orient To Ground And Velocity"),

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

protected:
	UMovementData();

public:
	UFUNCTION()
	void CalculateVelocity(URadicalMovementComponent* MovementComponent, float DeltaTime);
	UFUNCTION()
	void UpdateRotation(URadicalMovementComponent* MovementComponent, float DeltaTime) { PhysicsRotation(MovementComponent, DeltaTime); };

	/// @brief  Adjusts velocity based on change in height, conserving energy via mgh = 1/2 mv^2
	/// @param	OldLocation				Previous location in which we compare our current location to to get Delta height
	/// @param  UpEffectiveGravity		Gravity scaling factor to apply when dH > 0
	/// @param  DownEffectiveGravity	Gravity scaling factor to apply when dH < 0
	/// @param  bChangeDirection		Whether energy conservation should change direction to align with gravity if Speed = 0
	/// @param  GravityTangentToSurface	Direction of gravity along the surface in question. Could be floor impact normal (2D) or gravity projected onto a spline tangent (1D)
	UFUNCTION()
	void ConserveEnergy(URadicalMovementComponent* MovementComponent, const FVector& OldLocation, float UpEffectiveGravity, float DownEffectiveGravity, bool bChangeDirection, const FVector& GravityTangentToSurface = FVector::ZeroVector);

protected:
	static constexpr float MIN_DELTA_TIME = 1e-6;
	static constexpr float BRAKE_TO_STOP_VELOCITY = 10.f;

#pragma region ACCELERATION_METHODS
public:
	/// @brief  Gravity Direction. Will be normalized so magnitude does not matter.
	UPROPERTY(Category="Acceleration Data | Core", EditDefaultsOnly, BlueprintReadWrite)
	FVector Gravity;

	/// @brief  Scale applied to default physics volume gravity strength
	UPROPERTY(Category="Acceleration Data | Core", EditDefaultsOnly, BlueprintReadWrite, meta=(ClampMin="0", UIMin="0"))
	float GravityScale;

	UPROPERTY(Category="Acceleration Data | Core", EditDefaultsOnly, BlueprintReadWrite, meta=(InlineEditConditionToggle))
	bool bUseSeparateGravityScaleGoingUp;
	
	UPROPERTY(Category="Acceleration Data | Core", EditDefaultsOnly, BlueprintReadWrite, meta=(EditCondition="bUseSeparateGravityScaleGoingUp", ClampMin="0", UIMin="0"))
	float UpGravityScale;

	float GravityZ; // Cache value from movement component as to avoid swapping it around (NOTE: GetWorld() is invalid in a data asset)

	/// @brief  
	UPROPERTY(Category="Acceleration Data | Core", EditDefaultsOnly, BlueprintReadWrite, meta=(ClampMin="0", UIMin="0", ForceUnits="cm/s^2"))
	float MaxAcceleration;

	/// @brief  Maximum lateral speed
	UPROPERTY(Category="Acceleration Data | Core", EditDefaultsOnly, BlueprintReadWrite, meta=(ClampMin="0", UIMin="0", ForceUnits="cm/s"))
	float MaxSpeed;

	/// @brief  Speed we should accelerate up to when walking at minimum analog stick tilt
	UPROPERTY(Category="Acceleration Data | Core", EditDefaultsOnly, BlueprintReadWrite, meta=(ClampMin="0", UIMin="0", ForceUnits="cm/s"))
	float MinAnalogSpeed;
	
	UFUNCTION(Category="Shared Movement Data", BlueprintCallable)
	FORCEINLINE FVector GetGravityDir() const { return Gravity.GetSafeNormal(); }

	UFUNCTION(Category="Shared Movement Data", BlueprintCallable)
	FORCEINLINE FVector GetGravity() const { return Gravity.GetSafeNormal() * GravityZ; }

public:

	/// @brief  Setting that affects movement control. Higher values allow faster changes in direction.
	///			If bUseSeparateBrakingFriction is false, also affects the ability to stop more quickly when braking (Zero Acceleration) where it's multiplied by BrakingFrictionFactor.
	///			When braking, this property allows you to control how much friction is applied on ground, applying an opposing force that scales with current velocity.
	///			Can be used to simulate slippery surfaces.
	UPROPERTY(Category="Acceleration Data | Grounded", EditDefaultsOnly, BlueprintReadWrite)
	float GroundFriction;
	
	/// @brief  Friction to apply to lateral air movement when falling.
	///			If bUseSeparateBrakingFriction is false, also affects the ability to stop more quickly when braking (Zero Acceleration)
	UPROPERTY(Category="Acceleration Data | Aerial", EditDefaultsOnly, BlueprintReadWrite)
	float AerialLateralFriction;

	/// @brief  If true, BrakingFriction will be used to slow a character to a stop (when there's no Acceleration).
	///			If false, braking uses the same friction passed to CalcVelocity (GroundFriction or AerialLateralFriction), multiplied
	///			by BrakingFrictionFactor. @see BrakingFriction
	UPROPERTY(Category="Acceleration Data | Shared", EditDefaultsOnly, BlueprintReadWrite)
	uint8 bUseSeparateBrakingFriction	: 1;

	/// @brief  If True, we don't use friction calculations for turns, instead, we rotate the full velocity vector by some fixed rotation rate
	UPROPERTY(Category="Acceleration Data | Shared", EditDefaultsOnly, BlueprintReadWrite, meta=(InlineEditConditionToggle))
	uint8 bAccelerationRotates			: 1;

	/// @brief  Velocity rotation rate. Higher values means much tighter turns.
	UPROPERTY(Category="Acceleration Data | Shared", EditDefaultsOnly, BlueprintReadWrite, meta=(EditCondition="bAccelerationRotates", ClampMin="0", UIMin="0"))
	float AccelerationRotationRate;

	/// @brief  Factor used to multiply the actual value of friction when braking. Applies to any friction value currently used.
	///			@note This is 2 by default, a value of 1 gives the true drag equation
	UPROPERTY(Category="Acceleration Data | Shared", EditDefaultsOnly, BlueprintReadWrite)
	float BrakingFrictionFactor;

	/// @brief  Friction coefficient applied when braking (Acceleration = 0 or character is exceeding max speed). Actual value used is this multiplied by BrakingFrictionFactor.
	///			When braking, this allows you to control how much friction is applied when moving, applying an opposing force that scales with current velocity (Linearly).
	///			Braking is composed of Friction (velocity-dependent drag) and a constant deceleration.
	///			@note Only used if bUseSeparateBrakingFriction is true. Otherwise, current friction (ground or aerial) is used.
	UPROPERTY(Category="Acceleration Data | Shared", EditDefaultsOnly, BlueprintReadWrite, meta=(EditCondition="bUseSeparateBrakingFriction"))
	float BrakingFriction;

	/// @brief  Deceleration when on ground and not applying acceleration. @see MaxAcceleration
	UPROPERTY(Category="Acceleration Data | Grounded", EditDefaultsOnly, BlueprintReadWrite, meta=(ClampMin="0", UIMin="0", ForceUnits="cm/s^2"))
	float BrakingDecelerationGrounded;

	/// @brief  Deceleration when in air and not applying acceleration. @see MaxAcceleration
	UPROPERTY(Category="Acceleration Data | Aerial", EditDefaultsOnly, BlueprintReadWrite, meta=(ClampMin="0", UIMin="0", ForceUnits="cm/s^2"))
	float BrakingDecelerationAerial;

	/// @brief  When in air, amount of lateral movement control available to the character.
	///			0 = no control, 1 = full control at max speed of MaxSpeed.
	UPROPERTY(Category="Acceleration Data | Aerial", EditDefaultsOnly, BlueprintReadWrite, meta=(ClampMin="0", ClampMax="1", UIMin="0", UIMax="1"))
	float AirControl;

	/// @brief  When in air, multiplier applied to AirControl when Lateral Velocity is less than AirControlBoostVelocityThreshold.
	///			Setting this to zero will disable air control boosting. Final result is clamped to 1.
	UPROPERTY(Category="Acceleration Data | Aerial", EditDefaultsOnly, BlueprintReadWrite, meta=(ClampMin="0", UIMin="0"))
	float AirControlBoostMultiplier;

	/// @brief  When in air, if lateral velocity magnitude is less than this value, Air Control is multiplied by AirControlBoostMultiplier.
	///			Setting this to zero will disable air control boosting.
	UPROPERTY(Category="Acceleration Data | Aerial", EditDefaultsOnly, BlueprintReadWrite, meta=(ClampMin="0", UIMin="0", ForceUnits="cm/s"))
	float AirControlBoostVelocityThreshold;


	/// @brief  If true, curve is sampled for Ground Friction
	UPROPERTY(Category="Acceleration Data | Curves", EditDefaultsOnly, BlueprintReadWrite, meta=(InlineEditConditionToggle))
	uint8 bUseForwardFrictionCurve	: 1;

	UPROPERTY(Category="Acceleration Data | Curves", EditDefaultsOnly, BlueprintReadWrite, meta=(InlineEditConditionToggle))
	uint8 bUseTurnFrictionCurve		: 1;

	UPROPERTY(Category="Acceleration Data | Curves", EditDefaultsOnly, BlueprintReadWrite, meta=(InlineEditConditionToggle))
	uint8 bUseBrakingFrictionCurve	: 1;

	/// @brief	Normalized curve to sample values [0, GroundFriction] parametrized by normalized max speed
	UPROPERTY(Category="Acceleration Data | Curves", EditDefaultsOnly, BlueprintReadWrite, meta=(EditCondition="bUseForwardFrictionCurve"))
	UCurveFloat* ForwardFrictionCurve;

	/// @brief	Normalized curve to sample values [0, GroundFriction] parametrized by normalized max speed
	UPROPERTY(Category="Acceleration Data | Curves", EditDefaultsOnly, BlueprintReadWrite, meta=(EditCondition="bUseTurnFrictionCurve"))
	UCurveFloat* TurnFrictionCurve;
	
	UPROPERTY(Category="Acceleration Data | Curves", EditDefaultsOnly, BlueprintReadWrite, meta=(EditCondition="bUseBrakingFrictionCurve"))
	UCurveFloat* BrakingFrictionCurve;

	FORCEINLINE float SampleCurve(const UCurveFloat* Curve, const FVector& Velocity, bool bSampleStatus = false) const
	{
		return (bSampleStatus && Curve) ? Curve->GetFloatValue(Velocity.Length() / (MaxSpeed)) : 1.f;
	}
	
public:
	float GetFriction(EMovementState MoveState) const;
	
	float GetMaxBrakingDeceleration(EMovementState MoveState) const;

	void ApplyVelocityBraking(FVector& Velocity, float DeltaTime, float BrakingFriction, float BrakingDeceleration) const;
	
	FVector GetFallingLateralAcceleration(const FVector& Acceleration, const FVector& Velocity, float DeltaTime) const;
	
	FVector ComputeInputAcceleration(URadicalMovementComponent* MovementComponent) const;
	
	void ApplyGravity(FVector& Velocity, float TerminalLimit, float DeltaTime) const;

protected:
	virtual void CalculateInputVelocity(URadicalMovementComponent* MovementComponent, FVector& Velocity, FVector& Acceleration, float Friction, float BrakingDeceleration, float DeltaTime) const;
	
#pragma endregion ACCELERATION_METHODS


#pragma region ROTATION_METHODS
public:
	UPROPERTY(Category="Rotation Data", EditDefaultsOnly, BlueprintReadWrite)
	TEnumAsByte<ERotationMethod> RotationMethod;

	/// @brief  Rotation rate values should be within [0,360]
	UPROPERTY(Category="Rotation Data", EditDefaultsOnly, BlueprintReadWrite)
	float RotationRate;

	UPROPERTY(Category="Rotation Data", EditDefaultsOnly, BlueprintReadWrite, meta=(EditCondition="RotationMethod==ERotationMethod::METHOD_OrientToGroundAndInput || RotationMethod==ERotationMethod::METHOD_OrientToGroundAndVelocity", EditConditionHides))
	bool bRevertToGlobalUpWhenFalling;

	UPROPERTY(Category="Rotation Data", EditDefaultsOnly, BlueprintReadWrite, meta=(EditCondition="RotationMethod==ERotationMethod::METHOD_OrientToGroundAndInput || RotationMethod==ERotationMethod::METHOD_OrientToGroundAndVelocity", EditConditionHides))
	bool bInputOnlyRotatesWhenFalling;

protected:
	
	virtual void PhysicsRotation(URadicalMovementComponent* MovementComponent, float DeltaTime);
	
	FQuat GetDesiredRotation(const URadicalMovementComponent* MovementComp) const;

	FQuat GetUniformRotation(const URadicalMovementComponent* MovementComp) const;


#pragma endregion ROTATION_METHODS
};
