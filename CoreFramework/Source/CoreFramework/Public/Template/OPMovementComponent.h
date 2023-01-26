// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CustomMovementComponent.h"
#include "Template/TOPCharacter.h"
#include "Components/ActorComponent.h"
#include "OPMovementComponent.generated.h"


UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class COREFRAMEWORK_API UOPMovementComponent : public UCustomMovementComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UOPMovementComponent();
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
#pragma region Event Overrides
protected:

	/* Hey friend, when you're back from your walk, be sure to check the advanced tab of CharacterMovement (General Settings) :3 */

	virtual void UpdateVelocity(FVector& CurrentVelocity, float DeltaTime) override;

	virtual void UpdateRotation(FQuat& CurrentRotation, float DeltaTime) override;

	virtual void SubsteppedTick(FVector& CurrentVelocity, float DeltaTime) override;

#pragma endregion Event Overrides
	
public:

#pragma region Gameplay Parameters
	
	/** Custom gravity scale. Gravity is multiplied by this amount for the character. */
	UPROPERTY(Category="Character Movement (General Settings)", EditAnywhere, BlueprintReadWrite)
	float GravityScale;

	/** Max Acceleration (rate of change of velocity) */
	UPROPERTY(Category="Character Movement (General Settings)", EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", UIMin="0"))
	float MaxAcceleration;

	/** The ground speed that we should accelerate up to when walking at minimum analog stick tilt */
	UPROPERTY(Category = "Character Movement: Walking", EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0", UIMin = "0", ForceUnits="cm/s"))
	float MinAnalogWalkSpeed;

	/** The maximum ground speed when walking. Also determines maximum lateral speed when falling. */
	UPROPERTY(Category="Character Movement: Walking", EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", UIMin="0", ForceUnits="cm/s"))
	float MaxWalkSpeed;
	
	/**
	* Setting that affects movement control. Higher values allow faster changes in direction.
	* If bUseSeparateBrakingFriction is false, also affects the ability to stop more quickly when braking (whenever Acceleration is zero), where it is multiplied by BrakingFrictionFactor.
	* When braking, this property allows you to control how much friction is applied when moving across the ground, applying an opposing force that scales with current velocity.
	* This can be used to simulate slippery surfaces such as ice or oil by changing the value (possibly based on the material pawn is standing on).
	* @see BrakingDecelerationWalking, BrakingFriction, bUseSeparateBrakingFriction, BrakingFrictionFactor
	*/
	UPROPERTY(Category="Character Movement: Walking", EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", UIMin="0"))
	float GroundFriction;

	/**
	 * Time substepping when applying braking friction. Smaller time steps increase accuracy at the slight cost of performance, especially if there are large frame times.
	 */
	UPROPERTY(Category="Character Movement (General Settings)", EditAnywhere, BlueprintReadWrite, AdvancedDisplay, meta=(ClampMin="0.0166", ClampMax="0.05", UIMin="0.0166", UIMax="0.05"))
	float BrakingSubStepTime;
	
	/**
	 * Friction (drag) coefficient applied when braking (whenever Acceleration = 0, or if character is exceeding max speed); actual value used is this multiplied by BrakingFrictionFactor.
	 * When braking, this property allows you to control how much friction is applied when moving across the ground, applying an opposing force that scales with current velocity.
	 * Braking is composed of friction (velocity-dependent drag) and constant deceleration.
	 * This is the current value, used in all movement modes; if this is not desired, override it or bUseSeparateBrakingFriction when movement mode changes.
	 * @note Only used if bUseSeparateBrakingFriction setting is true, otherwise current friction such as GroundFriction is used.
	 * @see bUseSeparateBrakingFriction, BrakingFrictionFactor, GroundFriction, BrakingDecelerationWalking
	 */
	UPROPERTY(Category="Character Movement (General Settings)", EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", UIMin="0", EditCondition="bUseSeparateBrakingFriction"))
	float BrakingFriction;
	
	/**
	 * Factor used to multiply actual value of friction used when braking.
	* This applies to any friction value that is currently used, which may depend on bUseSeparateBrakingFriction.
	* @note This is 2 by default for historical reasons, a value of 1 gives the true drag equation.
	* @see bUseSeparateBrakingFriction, GroundFriction, BrakingFriction
	*/
	UPROPERTY(Category="Character Movement (General Settings)", EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", UIMin="0"))
	float BrakingFrictionFactor;

	/**
	* Deceleration when walking and not applying acceleration. This is a constant opposing force that directly lowers velocity by a constant value.
	* @see GroundFriction, MaxAcceleration
	 */
	UPROPERTY(Category="Character Movement: Walking", EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", UIMin="0"))
	float BrakingDecelerationGround;

	UPROPERTY(Category="Character Movement: Walking", EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", UIMin="0"))
	float BrakingDecelerationAir;
	
	/** Initial velocity (instantaneous vertical acceleration) when jumping. */
	UPROPERTY(Category="Character Movement: Jumping / Falling", EditAnywhere, BlueprintReadWrite, meta=(DisplayName="Jump Z Velocity", ClampMin="0", UIMin="0", ForceUnits="cm/s"))
	float JumpZVelocity;

	/** Fraction of JumpZVelocity to use when automatically "jumping off" of a base actor that's not allowed to be a base for a character. (For example, if you're not allowed to stand on other players.) */
	UPROPERTY(Category="Character Movement: Jumping / Falling", EditAnywhere, BlueprintReadWrite, AdvancedDisplay, meta=(ClampMin="0", UIMin="0"))
	float JumpOffJumpZFactor;
	
	/**
	* Friction to apply to lateral air movement when falling.
	* If bUseSeparateBrakingFriction is false, also affects the ability to stop more quickly when braking (whenever Acceleration is zero).
	* @see BrakingFriction, bUseSeparateBrakingFriction
	*/
	UPROPERTY(Category="Character Movement: Jumping / Falling", EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", UIMin="0"))
	float FallingLateralFriction;
	
	/**
	* When falling, amount of lateral movement control available to the character.
	* 0 = no control, 1 = full control at max speed of MaxWalkSpeed.
	*/
	UPROPERTY(Category="Character Movement: Jumping / Falling", EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", UIMin="0"))
	float AirControl;

	/**
	 * When falling, multiplier applied to AirControl when lateral velocity is less than AirControlBoostVelocityThreshold.
	 * Setting this to zero will disable air control boosting. Final result is clamped at 1.
	 */
	UPROPERTY(Category="Character Movement: Jumping / Falling", EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", UIMin="0"))
	float AirControlBoostMultiplier;

	/**
	 * When falling, if lateral velocity magnitude is less than this value, AirControl is multiplied by AirControlBoostMultiplier.
	 * Setting this to zero will disable air control boosting.
	 */
	UPROPERTY(Category="Character Movement: Jumping / Falling", EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", UIMin="0"))
	float AirControlBoostVelocityThreshold;

		/** Change in rotation per second, used when UseControllerDesiredRotation or OrientRotationToMovement are true. Set a negative value for infinite rotation rate and instant turns. */
	UPROPERTY(Category="Character Movement (Rotation Settings)", EditAnywhere, BlueprintReadWrite)
	FRotator RotationRate;

	/**
	 * If true, BrakingFriction will be used to slow the character to a stop (when there is no Acceleration).
	 * If false, braking uses the same friction passed to CalcVelocity() (ie GroundFriction when walking), multiplied by BrakingFrictionFactor.
	 * This setting applies to all movement modes; if only desired in certain modes, consider toggling it when movement modes change.
	 * @see BrakingFriction
	 */
	UPROPERTY(Category="Character Movement (General Settings)", EditDefaultsOnly, BlueprintReadWrite)
	uint8 bUseSeparateBrakingFriction:1;

	/**
	 *	Apply gravity while the character is actively jumping (e.g. holding the jump key).
	 *	Helps remove frame-rate dependent jump height, but may alter base jump height.
	 */
	UPROPERTY(Category="Character Movement: Jumping / Falling", EditAnywhere, BlueprintReadWrite, AdvancedDisplay)
	uint8 bApplyGravityWhileJumping:1;

	/**
	 * If true, movement will be performed even if there is no Controller for the Character owner.
	 * Normally without a Controller, movement will be aborted and velocity and acceleration are zeroed if the character is walking.
	 * Characters that are spawned without a Controller but with this flag enabled will initialize the movement mode to DefaultLandMovementMode or DefaultWaterMovementMode appropriately.
	 * @see DefaultLandMovementMode, DefaultWaterMovementMode
	 */
	UPROPERTY(Category="Character Movement (General Settings)", EditAnywhere, BlueprintReadWrite, AdvancedDisplay)
	uint8 bRunPhysicsWithNoController:1;
	
	/**
	 * If true, smoothly rotate the Character toward the Controller's desired rotation (typically Controller->ControlRotation), using RotationRate as the rate of rotation change. Overridden by OrientRotationToMovement.
	 * Normally you will want to make sure that other settings are cleared, such as bUseControllerRotationYaw on the Character.
	 */
	UPROPERTY(Category="Character Movement (Rotation Settings)", EditAnywhere, BlueprintReadWrite)
	uint8 bUseControllerDesiredRotation:1;

	/**
	 * If true, rotate the Character toward the direction of acceleration, using RotationRate as the rate of rotation change. Overrides UseControllerDesiredRotation.
	 * Normally you will want to make sure that other settings are cleared, such as bUseControllerRotationYaw on the Character.
	 */
	UPROPERTY(Category="Character Movement (Rotation Settings)", EditAnywhere, BlueprintReadWrite)
	uint8 bOrientRotationToMovement:1;

	/** Ignores size of acceleration component, and forces max acceleration to drive character at full velocity. */
	UPROPERTY()
	uint8 bForceMaxAccel:1;    
	
	/**
	* Modifier to applied to values such as acceleration and max speed due to analog input.
	*/
	UPROPERTY()
	float AnalogInputModifier;

#pragma endregion Gameplay Parameters

	UPROPERTY()
	TObjectPtr<ATOPCharacter> CharacterOwner;

#pragma region General Locomotion
protected:
	FORCEINLINE FVector ConstrainInputAcceleration(const FVector& InputAcceleration) const
	{
		if (InputAcceleration.Z != 0.f)
		{
			return FVector(InputAcceleration.X, InputAcceleration.Y, 0.f);
		}
		return InputAcceleration;
	}

	FORCEINLINE FVector ScaleInputAcceleration(const FVector& InputAcceleration) const
	{
		return MaxAcceleration * InputAcceleration.GetClampedToMaxSize(1.f);
	}

	FORCEINLINE float ComputeAnalogInputModifier() const
	{
		const float MaxAccel = MaxAcceleration;
		if (Acceleration.SizeSquared() > 0.f && MaxAccel > UE_SMALL_NUMBER)
		{
			return FMath::Clamp<FVector::FReal>(Acceleration.Size() / MaxAccel, 0.f, 1.f);
		}

		return 0.f;
	}
public:
	
	/* Called in the Phys methods each while loop iteration / sub-stepped */
	void CalcVelocity(float DeltaTime, float Friction, float BrakingDeceleration);

	/* Called in the PhysFalling method each while loop iteration / sub-stepped (Applies Gravity) */
	FVector NewFallVelocity(const FVector& InitialVelocity, const FVector& Gravity, float DeltaTime) const;

	/* Called in CalcVelocity */
	void ApplyVelocityBraking(float DeltaTime, float Friction, float BrakingDeceleration);

	/* Called in PerformMovement after the Phys method call. Preliminary root motion checks prior */
	void PhysicsRotation(float DeltaTime);

	/* Called in PhysicsRotation */
	FRotator GetDeltaRotation(float DeltaTime) const;

	/* Called in GetDeltaRotation */
	float GetAxisDeltaRotation(float InAxisRotationRate, float DeltaTime) const;

	/* Called in PhysicsRotation if the bOrientToMovement flag is set */
	FRotator ComputeOrientToMovementRotation(FRotator CurrentRotation, float DeltaTime, FRotator DesiredRotation) const;

	/* Called in PhysicsRotation, returns true if on ground or in air (changes only for Custom Movement Modes) */
	bool ShouldRemainVertical() const;

#pragma endregion General Locomotion

#pragma region Jump Stuff
protected:
	UPROPERTY()
	FVector PendingLaunchVelocity{0};	
public:
	
	/* Called in Character.cpp within CheckJumpInput */
	virtual bool DoJump();

	/* Called in Character.cpp within JumpIsAllowedInternal (called by CanJump) */
	virtual bool CanAttemptJump() const;

	/* General interface call. Called in Character.cpp within LaunchCharacter which is BlueprintCallable */
	virtual void Launch(FVector const& LaunchVel);

	/* Called in PerformMovement before the movement tick is done */
	virtual bool HandlePendingLaunch();

	/* Called in PhysFalling in the while loop, subsequently calls the Character version*/
	virtual void NotifyJumpApex();

	/* Just an interface call */
	UFUNCTION(BlueprintCallable, Category="Pawn|Components|CharacterMovement")
	virtual float GetMaxJumpHeight() const;

	/* Just an interface call */
	UFUNCTION(BlueprintCallable, Category="Pawn|Components|CharacterMovement")
	virtual float GetMaxJumpHeightWithJumpTime() const;

#pragma endregion Jump Stuff
};
