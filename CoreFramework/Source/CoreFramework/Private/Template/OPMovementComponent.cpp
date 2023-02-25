﻿// Fill out your copyright notice in the Description page of Project Settings.


#include "Template/OPMovementComponent.h"


UOPMovementComponent::UOPMovementComponent()
{
}

void UOPMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	if (ModCharacterOwner)
	{
		ModCharacterOwner = Cast<ATOPCharacter>(CharacterOwner);
	}
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

void UOPMovementComponent::SubsteppedTick(FVector& CurrentVelocity, float DeltaTime)
{
	if (IsMovingOnGround())
	{
		// Imitate PhysWalking
		if (!HasAnimRootMotion())
		{
			CalcVelocity(DeltaTime, GroundFriction, BrakingDecelerationGround);
		}
	}
	else
	{
		// Imitate PhysFalling
		if (!HasAnimRootMotion())
		{
			const float zVel = Velocity.Z;
			Velocity.Z = 0;
			CalcVelocity(DeltaTime, FallingLateralFriction, BrakingDecelerationAir);
			Velocity.Z = zVel;
		}
		/* Jump Code Found In PhysFalling */

		// If jump is providing force, gravity may be affected.
		bool bEndingJumpForce = false;
		float GravityTime = DeltaTime;

		if (ModCharacterOwner->JumpForceTimeRemaining > 0.0f)
		{
			// Consume some of the force time. Only the remaining time (if any) is affected by gravity when bApplyGravityWhileJumping=false.
			const float JumpForceTime = FMath::Min(ModCharacterOwner->JumpForceTimeRemaining, DeltaTime);
			GravityTime = bApplyGravityWhileJumping ? DeltaTime : FMath::Max(0.0f, DeltaTime - JumpForceTime);
			
			// Update Character state
			ModCharacterOwner->JumpForceTimeRemaining -= JumpForceTime;
			if (ModCharacterOwner->JumpForceTimeRemaining <= 0.0f)
			{
				ModCharacterOwner->ResetJumpState();
				bEndingJumpForce = true;
			}
		}
		
		/* Apply Gravity As Found In PhysFalling */
		CurrentVelocity = NewFallVelocity(CurrentVelocity, GetGravityZ() * FVector::UpVector, GravityTime);

		/* Portion here maybe (CMC Line 4498) */
		

		/* Limit air control */
		//FVector FallAcceleration = GetFallingLateralAcceleration(DeltaTime);
		//FallAcceleration.Z = 0.f;
		//const bool bHasLimitedAirControl = ShouldLimitAirControl(DeltaTime, FallAcceleration);
	}
}


void UOPMovementComponent::UpdateVelocity(FVector& CurrentVelocity, float DeltaTime)
{
	//Super::UpdateVelocity_Implementation(CurrentVelocity, DeltaTime);

	/* Calc Velocity applies the input acceleration */

	/* Lots of things embedded in PhysFalling so worth taking a look there, unsure if necessary but educational */
	// (Ex: Checking ValidLandingSpot to transition to landing, it also resets the physics when movement modes change,
	// CalcVel & JumpApex stuff verified there too) //

	/* ControlledCharacterMove */
	ModCharacterOwner->CheckJumpInput(DeltaTime);
	Acceleration = ScaleInputAcceleration(ConstrainInputAcceleration(InputVector));
	AnalogInputModifier = ComputeAnalogInputModifier();
}

void UOPMovementComponent::UpdateRotation(FQuat& CurrentRotation, float DeltaTime)
{
	//Super::UpdateRotation_Implementation(CurrentRotation, DeltaTime);

	if (!bOrientRotationToMovement)
	{
		if (!CurrentFloor.bBlockingHit) return;

		const FVector FloorNormal = CurrentFloor.HitResult.Normal;
	
		// Velocity defines forward, floor normal defines up
		FVector Forward = Velocity.IsZero() ? UpdatedComponent->GetForwardVector() : Velocity.GetSafeNormal();

		FRotator Target = UKismetMathLibrary::MakeRotFromXZ(Forward, FloorNormal.GetSafeNormal());

		constexpr float AngleTolerance = 1e-3f;
/*
		if (!CurrentRotation.Rotator().Equals(Target, AngleTolerance))
		{
			const auto DeltaRot = GetDeltaRotation(DeltaTime);
			// PITCH
			if (!FMath::IsNearlyEqual(CurrentRotation.Rotator().Pitch, Target.Pitch, AngleTolerance))
			{
				Target.Pitch = FMath::FixedTurn(CurrentRotation.Rotator().Pitch, Target.Pitch, DeltaRot.Pitch);
			}

			// YAW
			if (!FMath::IsNearlyEqual(CurrentRotation.Rotator().Yaw, Target.Yaw, AngleTolerance))
			{
				Target.Yaw = FMath::FixedTurn(CurrentRotation.Rotator().Yaw, Target.Yaw, DeltaRot.Yaw);
			}

			// ROLL
			if (!FMath::IsNearlyEqual(CurrentRotation.Rotator().Roll, Target.Roll, AngleTolerance))
			{
				Target.Roll = FMath::FixedTurn(CurrentRotation.Rotator().Roll, Target.Roll, DeltaRot.Roll);
			}

			// Set the new rotation.
			MoveUpdatedComponent( FVector::ZeroVector, Target.Quaternion(), false );
		}
	*/
		MoveUpdatedComponent(FVector::ZeroVector, Target.Quaternion(), false);
	}
	else 
		PhysicsRotation(DeltaTime);
}

// From CMC
void UOPMovementComponent::CalcVelocity(float DeltaTime, float Friction, float BrakingDeceleration)
{
	// Do not update velocity when using root motion
	if (HasAnimRootMotion())
	{
		return;
	}

	Friction = FMath::Max(0.f, Friction);
	const float MaxAccel = MaxAcceleration;
	float MaxSpeed = MaxWalkSpeed;

	// Check if path following requested movement
	// Check if path following requested movement
	bool bZeroRequestedAcceleration = true;
	FVector RequestedAcceleration = FVector::ZeroVector;
	float RequestedSpeed = 0.0f;
	//if (ApplyRequestedMove(DeltaTime, MaxAccel, MaxSpeed, Friction, BrakingDeceleration, RequestedAcceleration, RequestedSpeed))
	//{
	//	bZeroRequestedAcceleration = false;
	//}

	if (bForceMaxAccel)
	{
		// Force acceleration at full speed
		// In consideration order for direction: Acceleration, then Velocity, then Pawn's rotation
		if (Acceleration.SizeSquared() > UE_SMALL_NUMBER)
		{
			Acceleration = Acceleration.GetSafeNormal() * MaxAccel;
		}
		else
		{
			Acceleration = MaxAccel * (Velocity.SizeSquared() < UE_SMALL_NUMBER ? UpdatedComponent->GetForwardVector() : Velocity.GetSafeNormal());
		}

		AnalogInputModifier = 1.f;
	}

	const float MaxInputSpeed = FMath::Max(MaxSpeed * AnalogInputModifier, MinAnalogWalkSpeed);
	MaxSpeed = MaxInputSpeed;//FMath::Max(RequestedSpeed, MaxInputSpeed);

	// Apply braking or deceleration
	const bool bZeroAcceleration = Acceleration.IsZero();
	const bool bVelocityOverMax = IsExceedingMaxSpeed(MaxSpeed);

	// Only apply braking if there is no acceleration, or we are over our max speed and need to slow down to it.
	if ((bZeroAcceleration && bZeroRequestedAcceleration) || bVelocityOverMax)
	{
		const FVector OldVelocity = Velocity;

		const float ActualBrakingFriction = (bUseSeparateBrakingFriction ? BrakingFriction : Friction);
		ApplyVelocityBraking(DeltaTime, ActualBrakingFriction, BrakingDeceleration);
	
		// Don't allow braking to lower us below max speed if we started above it.
		if (bVelocityOverMax && Velocity.SizeSquared() < FMath::Square(MaxSpeed) && FVector::DotProduct(Acceleration, OldVelocity) > 0.0f)
		{
			Velocity = OldVelocity.GetSafeNormal() * MaxSpeed;
		}
	}
	else if (!bZeroAcceleration)
	{
		// Friction affects our ability to change direction. This is only done for input acceleration, not path following.
		const FVector AccelDir = Acceleration.GetSafeNormal();
		const float VelSize = Velocity.Size();
		Velocity = Velocity - (Velocity - AccelDir * VelSize) * FMath::Min(DeltaTime * Friction, 1.f);
	}

	// Apply input acceleration
	if (!bZeroAcceleration)
	{
		const float NewMaxInputSpeed = IsExceedingMaxSpeed(MaxInputSpeed) ? Velocity.Size() : MaxInputSpeed;
		Velocity += Acceleration * DeltaTime;
		Velocity = Velocity.GetClampedToMaxSize(NewMaxInputSpeed);
	}

	// Apply additional requested acceleration
	if (!bZeroRequestedAcceleration)
	{
		const float NewMaxRequestedSpeed = IsExceedingMaxSpeed(RequestedSpeed) ? Velocity.Size() : RequestedSpeed;
		Velocity += RequestedAcceleration * DeltaTime;
		Velocity = Velocity.GetClampedToMaxSize(NewMaxRequestedSpeed);
	}

	//if (bUseRVOAvoidance)
	//{
	//	CalcAvoidanceVelocity(DeltaTime);
	//}
}

FVector UOPMovementComponent::NewFallVelocity(const FVector& InitialVelocity, const FVector& InGravity, float DeltaTime) const
{
	FVector Result = InitialVelocity;

	if (DeltaTime > 0.f)
	{
		// Apply Gravity
		Result += InGravity * DeltaTime;

		// Don't exceed terminal velocity
		const float TerminalLimit = FMath::Abs(GetPhysicsVolume()->TerminalVelocity);
		if (Result.SizeSquared() > FMath::Square(TerminalLimit))
		{
			const FVector GravityDir = InGravity.GetSafeNormal();
			if ((Result | GravityDir) > TerminalLimit)
			{
				Result = FVector::PointPlaneProject(Result, FVector::ZeroVector, GravityDir) + GravityDir * TerminalLimit;
			}
		}
	}

	return Result;
}

void UOPMovementComponent::ApplyVelocityBraking(float DeltaTime, float Friction, float BrakingDeceleration)
{
	/** Stop completely when braking and velocity magnitude is lower than this. */
	static constexpr float BRAKE_TO_STOP_VELOCITY = 10.f;
	
	if (Velocity.IsZero() || HasAnimRootMotion() || DeltaTime < MIN_TICK_TIME)
	{
		return;
	}

	const float FrictionFactor = FMath::Max(0.f, BrakingFrictionFactor);
	Friction = FMath::Max(0.f, Friction * FrictionFactor);
	BrakingDeceleration = FMath::Max(0.f, BrakingDeceleration);
	const bool bZeroFriction = (Friction == 0.f);
	const bool bZeroBraking = (BrakingDeceleration == 0.f);

	if (bZeroFriction && bZeroBraking)
	{
		return;
	}

	const FVector OldVel = Velocity;

	// subdivide braking to get reasonably consistent results at lower frame rates
	// (important for packet loss situations w/ networking)
	float RemainingTime = DeltaTime;
	const float MaxTimeStep = FMath::Clamp(BrakingSubStepTime, 1.0f / 75.0f, 1.0f / 20.0f);

	// Decelerate to brake to a stop
	const FVector RevAccel = (bZeroBraking ? FVector::ZeroVector : (-BrakingDeceleration * Velocity.GetSafeNormal()));
	while( RemainingTime >= MIN_TICK_TIME )
	{
		// Zero friction uses constant deceleration, so no need for iteration.
		const float dt = ((RemainingTime > MaxTimeStep && !bZeroFriction) ? FMath::Min(MaxTimeStep, RemainingTime * 0.5f) : RemainingTime);
		RemainingTime -= dt;

		// apply friction and braking
		Velocity = Velocity + ((-Friction) * Velocity + RevAccel) * dt ; 
		
		// Don't reverse direction
		if ((Velocity | OldVel) <= 0.f)
		{
			Velocity = FVector::ZeroVector;
			return;
		}
	}

	// Clamp to zero if nearly zero, or if below min threshold and braking.
	const float VSizeSq = Velocity.SizeSquared();
	if (VSizeSq <= UE_KINDA_SMALL_NUMBER || (!bZeroBraking && VSizeSq <= FMath::Square(BRAKE_TO_STOP_VELOCITY)))
	{
		Velocity = FVector::ZeroVector;
	}
}

// From CMC
void UOPMovementComponent::PhysicsRotation(float DeltaTime)
{
	
	if (!(bOrientRotationToMovement || bUseControllerDesiredRotation))
	{
		return;
	}
	
	//if (!CharacterOwner->Controller)
	//{
	//	return;
	//}
	
	FRotator CurrentRotation = UpdatedComponent->GetComponentRotation(); // Normalized
	
	FRotator DeltaRot = GetDeltaRotation(DeltaTime);
	DeltaRot.DiagnosticCheckNaN(TEXT("CharacterMovementComponent::PhysicsRotation(): GetDeltaRotation"));

	FRotator DesiredRotation = CurrentRotation;
	if (bOrientRotationToMovement)
	{
		DesiredRotation = ComputeOrientToMovementRotation(CurrentRotation, DeltaTime, DeltaRot);
	}
	else if (CharacterOwner->Controller && bUseControllerDesiredRotation)
	{
		DesiredRotation = CharacterOwner->Controller->GetDesiredRotation();
	}
	else if (!CharacterOwner->Controller && bRunPhysicsWithNoController && bUseControllerDesiredRotation)
	{
		if (const AController* ControllerOwner = Cast<AController>(CharacterOwner->GetOwner()))
		{
			DesiredRotation = ControllerOwner->GetDesiredRotation();
		}
	}
	else
	{
		return;
	}

	if (ShouldRemainVertical())
	{
		DesiredRotation.Pitch = 0.f;
		DesiredRotation.Yaw = FRotator::NormalizeAxis(DesiredRotation.Yaw);
		DesiredRotation.Roll = 0.f;
	}
	else
	{
		DesiredRotation.Normalize();
	}

	// Accumulate a desired new rotation.
	constexpr float AngleTolerance = 1e-3f;

	if (!CurrentRotation.Equals(DesiredRotation, AngleTolerance))
	{
		// PITCH
		if (!FMath::IsNearlyEqual(CurrentRotation.Pitch, DesiredRotation.Pitch, AngleTolerance))
		{
			DesiredRotation.Pitch = FMath::FixedTurn(CurrentRotation.Pitch, DesiredRotation.Pitch, DeltaRot.Pitch);
		}

		// YAW
		if (!FMath::IsNearlyEqual(CurrentRotation.Yaw, DesiredRotation.Yaw, AngleTolerance))
		{
			DesiredRotation.Yaw = FMath::FixedTurn(CurrentRotation.Yaw, DesiredRotation.Yaw, DeltaRot.Yaw);
		}

		// ROLL
		if (!FMath::IsNearlyEqual(CurrentRotation.Roll, DesiredRotation.Roll, AngleTolerance))
		{
			DesiredRotation.Roll = FMath::FixedTurn(CurrentRotation.Roll, DesiredRotation.Roll, DeltaRot.Roll);
		}

		// Set the new rotation.
		DesiredRotation.DiagnosticCheckNaN(TEXT("CharacterMovementComponent::PhysicsRotation(): DesiredRotation"));
		MoveUpdatedComponent( FVector::ZeroVector, DesiredRotation, /*bSweep*/ false );
	}
}

float UOPMovementComponent::GetAxisDeltaRotation(float InAxisRotationRate, float DeltaTime) const
{
	return (InAxisRotationRate >= 0.f) ? FMath::Min(InAxisRotationRate * DeltaTime, 360.f) : 360.f;
}

FRotator UOPMovementComponent::GetDeltaRotation(float DeltaTime) const
{
	return FRotator(GetAxisDeltaRotation(RotationRate.Pitch, DeltaTime), GetAxisDeltaRotation(RotationRate.Yaw, DeltaTime), GetAxisDeltaRotation(RotationRate.Roll, DeltaTime));
}

FRotator UOPMovementComponent::ComputeOrientToMovementRotation(FRotator CurrentRotation, float DeltaTime, FRotator DesiredRotation) const
{
	if (Acceleration.SizeSquared() < UE_KINDA_SMALL_NUMBER)
	{
		// AI path following request can orient us in that direction (it's effectively an acceleration)
		//if (bHasRequestedVelocity && RequestedVelocity.SizeSquared() > UE_KINDA_SMALL_NUMBER)
		//{
		//	return RequestedVelocity.GetSafeNormal().Rotation();
		//}

		// Don't change rotation if there is no acceleration.
		return CurrentRotation;
	}

	// Rotate toward direction of acceleration.
	return Acceleration.GetSafeNormal().Rotation();
}

bool UOPMovementComponent::ShouldRemainVertical() const
{
	return true;
}

bool UOPMovementComponent::DoJump()
{
	if (ModCharacterOwner && ModCharacterOwner->CanJump())
	{
		// Don't jump if we can't move up/down
		if (!bConstrainToPlane || FMath::Abs(PlaneConstraintNormal.Z) != 1.f)
		{
			SetMovementState(STATE_Falling);
			Velocity.Z = FMath::Max<FVector::FReal>(Velocity.Z, JumpZVelocity);
			return true;
		}
	}
	return false;
}

bool UOPMovementComponent::CanAttemptJump() const
{
	return IsJumpAllowed();
}

void UOPMovementComponent::Launch(FVector const& LaunchVel)
{
	PendingLaunchVelocity = LaunchVel;
}

void UOPMovementComponent::NotifyJumpApex()
{
	if (ModCharacterOwner)
	{
		//CharacterOwner->NotifyJumpApex();
	}
}

float UOPMovementComponent::GetMaxJumpHeight() const
{
	const float GravityMag = GetGravityZ();
	if (FMath::Abs(GravityMag) > UE_KINDA_SMALL_NUMBER)
	{
		return FMath::Square(JumpZVelocity) / (-2.f * GravityMag);
	}
	else
	{
		return 0.f;
	}
}

float UOPMovementComponent::GetMaxJumpHeightWithJumpTime() const
{
	const float MaxJumpHeight = GetMaxJumpHeight();

	if (CharacterOwner)
	{
		// When bApplyGravityWhileJumping is true, the actual max height will be lower than this.
		// However, it will also be dependent on framerate (and substep iterations) so just return this
		// to avoid expensive calculations.

		// This can be imagined as the character being displaced to some height, then jumping from that height.
		return (ModCharacterOwner->JumpMaxHoldTime * JumpZVelocity) + MaxJumpHeight;
	}

	return MaxJumpHeight;
}
