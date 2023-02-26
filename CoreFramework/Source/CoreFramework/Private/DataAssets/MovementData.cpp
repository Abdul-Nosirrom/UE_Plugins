// Fill out your copyright notice in the Description page of Project Settings.

#include "MovementData.h"

#include "Components/CustomMovementComponent.h"

UMovementData::UMovementData()
{
	/*~~~~~ Default UE_DEFAULT Values ~~~~~*/
	Gravity = FVector(0.f, 0.f, -1.f);
	GravityScale = 1.f;

	GroundFriction = 8.f;
	AerialLateralFriction = 0.f;

	MaxSpeed = 700.f;
	MaxAcceleration = 2048.0f;
	MinAnalogSpeed = 0.f;

	BrakingDecelerationGrounded = MaxAcceleration;
	BrakingDecelerationAerial = 0.f;
	BrakingFrictionFactor = 2.f;
	BrakingFriction = 12.f;

	bUseSeparateBrakingFriction = false;

	AirControl = 0.35f;
	AirControlBoostMultiplier = 2.f;
	AirControlBoostVelocityThreshold = 25.f;
}


void UMovementData::CalculateVelocity(UCustomMovementComponent* MovementComponent, float DeltaTime)
{
	GravityZ = FMath::Abs(MovementComponent->GetGravityZ());
	
	switch (AccelerationMethod)
	{
		case METHOD_Default:
			CalculateDefaultVelocity(MovementComponent, DeltaTime);
			break;
		case METHOD_Legacy:
			//CalculateLegacyVelocity(Velocity, Acceleration, DeltaTime);
		default:
			break;
	}
}

void UMovementData::UpdateRotation(UCustomMovementComponent* MovementComponent, float DeltaTime)
{
	PhysicsRotation(MovementComponent, DeltaTime);
}


float UMovementData::GetMaxBrakingDeceleration(EMovementState MoveState) const
{
	switch (MoveState)
	{
		case STATE_Grounded:
			return BrakingDecelerationGrounded;
		case STATE_Falling:
			return BrakingDecelerationAerial;
		case STATE_None:
		default:
			return 0.f;
	}
}

float UMovementData::GetFriction(EMovementState MoveState) const
{
	switch (MoveState)
	{
		case STATE_Grounded:
			return GroundFriction;
		case STATE_Falling:
			return AerialLateralFriction;
		case STATE_None:
			default:
				return 0.f;
	}
}


void UMovementData::ApplyVelocityBraking(FVector& Velocity, float DeltaTime, float InBrakingFriction, float InBrakingDeceleration) const
{
	if (Velocity.IsZero() ||  DeltaTime < MIN_DELTA_TIME) return;

	const float FrictionFactor = FMath::Max(0.f, BrakingFrictionFactor);
	InBrakingFriction = FMath::Max(0.f, InBrakingFriction * FrictionFactor);
	InBrakingDeceleration = FMath::Max(0.f, InBrakingDeceleration);
	
	const bool bZeroFriction = (InBrakingFriction == 0.f);
	const bool bZeroBraking = (InBrakingDeceleration == 0.f);

	if (bZeroFriction && bZeroBraking) return;

	const FVector OldVel = Velocity;

	// Decelerate to brake to a stop
	const FVector RevAccel = (bZeroBraking ? FVector::ZeroVector : (-InBrakingDeceleration * Velocity.GetSafeNormal()));
	
	Velocity = Velocity + (RevAccel  - InBrakingFriction * Velocity) * DeltaTime;

	// Don't reverse direction
	if ((Velocity | OldVel) <= 0.f)
	{
		Velocity = FVector::ZeroVector;
		return;
	}

	// Clamp to zero if nearly zero or below min threshold and braking
	const float VSizeSq = Velocity.SizeSquared();
	if (VSizeSq <= UE_KINDA_SMALL_NUMBER || (!bZeroBraking && VSizeSq <= FMath::Square(BRAKE_TO_STOP_VELOCITY)))
	{
		Velocity = FVector::ZeroVector;
	}
}


// Takes in Friction & BrakingDeceleration
void UMovementData::CalculateDefaultVelocity(UCustomMovementComponent* MovementComponent, float DeltaTime) const
{
	// Skip update if we have root motion or delta time is too small
	// TODO: Maybe move these events 
	if (MovementComponent->HasAnimRootMotion() || DeltaTime < MIN_DELTA_TIME) return;

	FVector Velocity = MovementComponent->GetVelocity();
	FVector Acceleration = ComputeInputAcceleration(MovementComponent);

	if (MovementComponent->GetMovementState() == STATE_Grounded)
	{
		CalculateInputVelocity(MovementComponent, Velocity, Acceleration, GetFriction(STATE_Grounded), GetMaxBrakingDeceleration(STATE_Grounded), DeltaTime);
	}
	else if (MovementComponent->GetMovementState() == STATE_Falling)
	{
		FVector FallAcceleration = GetFallingLateralAcceleration(Acceleration, Velocity, DeltaTime);
		FallAcceleration = FVector::VectorPlaneProject(FallAcceleration, GetGravityDir());

		const FVector OldVelocity = Velocity;
		
		{
			TGuardValue<FVector> RestoreAcceleration(Acceleration, FallAcceleration);
			Velocity.Z = 0.f;
			CalculateInputVelocity(MovementComponent, Velocity, Acceleration, GetFriction(STATE_Falling), GetMaxBrakingDeceleration(STATE_Falling), DeltaTime);
			Velocity.Z = OldVelocity.Z;
		}

		ApplyGravity(Velocity, MovementComponent->GetPhysicsVolume()->TerminalVelocity, DeltaTime);
	}
	
	MovementComponent->SetVelocity(Velocity);
	MovementComponent->SetAcceleration(Acceleration);
}

FVector UMovementData::GetFallingLateralAcceleration(const FVector& Acceleration, const FVector& Velocity, float DeltaTime) const
{
	FVector FallAcceleration = FVector(Acceleration.X, Acceleration.Y, 0.f);

	// Bound acceleration, falling object has minimal ability to impact acceleration
	if (FVector::VectorPlaneProject(FallAcceleration, GetGravityDir()).SizeSquared() > 0.f)
	{
		float BoostAirControl = AirControl;
		if (BoostAirControl != 0.f)
		{
			// Boost air control
			if (AirControlBoostMultiplier > 0.f && FVector::VectorPlaneProject(Velocity, GetGravityDir()).SizeSquared() < FMath::Square(AirControlBoostVelocityThreshold))
			{
				BoostAirControl = FMath::Min(1.f, AirControlBoostMultiplier * BoostAirControl);
			}
		}
		FallAcceleration *= BoostAirControl;
		FallAcceleration = FallAcceleration.GetClampedToMaxSize(MaxAcceleration);
	}

	return FallAcceleration;
}


FVector UMovementData::ComputeInputAcceleration(UCustomMovementComponent* MovementComponent) const
{
	return MaxAcceleration * FVector::VectorPlaneProject(MovementComponent->GetLastInputVector(), MovementComponent->GetUpOrientation(MODE_Gravity));;
}

void UMovementData::CalculateInputVelocity(const UCustomMovementComponent* MovementComponent, FVector& Velocity, FVector& Acceleration, float Friction, float BrakingDeceleration, float DeltaTime) const
{
	Friction = FMath::Max(0.f, Friction); // Ensure friction is greater than zero
	
	const float AnalogInputModifier = (Acceleration.SizeSquared() > 0.f && MaxAcceleration > UE_SMALL_NUMBER) ? FMath::Clamp<FVector::FReal>(Acceleration.Size() / MaxAcceleration, 0.f, 1.f) : 0.f;
	const float MaxInputSpeed = FMath::Max(MaxSpeed * (AnalogInputModifier), MinAnalogSpeed);
	
	const bool bZeroAcceleration = Acceleration.IsZero();
	const bool bVelocityOverMax = MovementComponent->IsExceedingMaxSpeed(MaxSpeed);

	/* Only apply braking if there is no acceleration or we are over max speed */
	if ((bZeroAcceleration) || bVelocityOverMax)
	{
		const FVector OldVelocity = Velocity;

		const float ActualBrakingFriction = (bUseSeparateBrakingFriction ? BrakingFriction : Friction);
		ApplyVelocityBraking(Velocity, DeltaTime, ActualBrakingFriction, BrakingDeceleration);

		/* Don't allow braking to lower us below max speed if we started above it and input is still in the same direction */
		if (bVelocityOverMax && Velocity.SizeSquared() < FMath::Square(MaxSpeed) && FVector::DotProduct(Acceleration, OldVelocity) > 0.f)
		{
			Velocity = OldVelocity.GetSafeNormal() * MaxSpeed;
		}
	}
	else if (!bZeroAcceleration)
	{
		/* (Non-Braking) Friction affects our ability to change direction. Only done for input acceleration not path following */
		const FVector AccelDir = Acceleration.GetSafeNormal();
		const float VelSize = Velocity.Size();

		// NOTE: How the friction is only actually applied to tangential components of velocity
		Velocity = Velocity - (Velocity - AccelDir * VelSize) * FMath::Min(DeltaTime * Friction, 1.f);
	}

	/* Apply input acceleration */
	if (!bZeroAcceleration)
	{
		const float NewMaxInputSpeed = MovementComponent->IsExceedingMaxSpeed(MaxInputSpeed) ? Velocity.Size() : MaxInputSpeed;
		Velocity += MovementComponent->GetAcceleration() * DeltaTime;
		Velocity = Velocity.GetClampedToMaxSize(NewMaxInputSpeed);
	}

}

void UMovementData::ApplyGravity(FVector& Velocity, float TerminalLimit, float DeltaTime) const
{
	Velocity += GetGravity() * GravityZ * DeltaTime;
	TerminalLimit = FMath::Abs(TerminalLimit);

	if (Velocity.SizeSquared() > FMath::Square(TerminalLimit))
	{
		if ((Velocity | GetGravityDir()) > TerminalLimit)
		{
			Velocity = FVector::PointPlaneProject(Velocity, FVector::ZeroVector, GetGravityDir()) + GetGravityDir() * TerminalLimit;
		}
	}
}


void UMovementData::PhysicsRotation(UCustomMovementComponent* MovementComponent, float DeltaTime)
{
	FRotator CurrentRotation = MovementComponent->UpdatedComponent->GetComponentRotation();
	FRotator DeltaRot = FRotator(GetAxisDeltaRotation(RotationRate.Pitch, DeltaTime), GetAxisDeltaRotation(RotationRate.Yaw, DeltaTime), GetAxisDeltaRotation(RotationRate.Roll, DeltaTime));
	FRotator DesiredRotation = CurrentRotation;

	switch (RotationMethod)
	{
		case METHOD_OrientToMovement:
			DesiredRotation = ComputeOrientToMovementRotation(CurrentRotation, DeltaRot, MovementComponent->GetAcceleration(), DeltaTime);
			break;
		case METHOD_ControllerDesiredRotation:
			DesiredRotation = MovementComponent->GetPawnOwner()->Controller->GetDesiredRotation();
			break;
		default:
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
		// TODO: Just amounts to computing our rotators w.r.t to the plane, so we include the above rotator then project it onto a plane defined by normal
		// Orient to ground here
		const FVector Normal = MovementComponent->CurrentFloor.HitResult.ImpactNormal;
	}

	// Accumulate desired new rotation
	const float AngleTolerance = 1e-3f;

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

		MovementComponent->MoveUpdatedComponent(FVector::ZeroVector, DesiredRotation, false);
	}
}

FRotator UMovementData::ComputeOrientToMovementRotation(const FRotator& CurrentRotation, FRotator& DeltaRotation, FVector Acceleration, float DeltaTime) const
{
	if (Acceleration.SizeSquared() < UE_KINDA_SMALL_NUMBER)
	{
		// AI path following request can orient us in that direction (it's effectively an acceleration)
		/*if (bHasRequestedVelocity && RequestedVelocity.SizeSquared() > UE_KINDA_SMALL_NUMBER)
		{
			return RequestedVelocity.GetSafeNormal().Rotation();
		}*/

		// Don't change rotation if there is no acceleration.
		return CurrentRotation;
	}

	// Rotate toward direction of acceleration.
	return Acceleration.GetSafeNormal().Rotation();
}
