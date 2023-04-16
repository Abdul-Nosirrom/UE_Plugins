// Fill out your copyright notice in the Description page of Project Settings.

#include "MovementData.h"

#include "RadicalCharacter.h"
#include "RMC_LOG.h"
#include "Components/RadicalMovementComponent.h"

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

#pragma region Acceleration

void UMovementData::CalculateVelocity(URadicalMovementComponent* MovementComponent, float DeltaTime)
{
	GravityZ = FMath::Abs(MovementComponent->GetGravityZ());

	
	FVector Velocity = MovementComponent->GetVelocity();
	FVector Acceleration = ComputeInputAcceleration(MovementComponent);

	if (MovementComponent->GetMovementState() == STATE_Grounded || MovementComponent->GetMovementState() == STATE_General)
	{
		CalculateInputVelocity(MovementComponent, Velocity, Acceleration, GetFriction(STATE_Grounded), GetMaxBrakingDeceleration(STATE_Grounded), DeltaTime);
	}
	else if (MovementComponent->GetMovementState() == STATE_Falling)
	{
		FVector FallAcceleration = GetFallingLateralAcceleration(Acceleration, Velocity, DeltaTime);
		FallAcceleration = bInputOnlyRotatesWhenFalling ? FVector::ZeroVector : FVector::VectorPlaneProject(FallAcceleration, GetGravityDir());

		const FVector OldVelocity = Velocity;
		
		{
			TGuardValue<FVector> RestoreAcceleration(Acceleration, FallAcceleration);
			Velocity.Z = 0.f;
			MovementComponent->SetVelocity(Velocity); // DEBUG: This is so IsExceedingMaxSpeed has up-to-date velocity information and doesn't account for vertical vel
			CalculateInputVelocity(MovementComponent, Velocity, Acceleration, GetFriction(STATE_Falling), GetMaxBrakingDeceleration(STATE_Falling), DeltaTime);
			Velocity.Z = OldVelocity.Z;
		}

		ApplyGravity(Velocity, MovementComponent->GetPhysicsVolume()->TerminalVelocity, DeltaTime);
	}
	
	MovementComponent->SetVelocity(Velocity);
	MovementComponent->SetAcceleration(Acceleration);
}

void UMovementData::ConserveEnergy(URadicalMovementComponent* MovementComponent, const FVector& OldLocation, float UpEffectiveGravity, float DownEffectiveGravity, bool bChangeDirection, const FVector& GravityTangentToSurface)
{
	// NOTE: We don't do this for falling because gravity is being applied there and it does essentially the same thing
	if (MovementComponent->IsFalling()) return;
	
	/* Get Change In Height */
	const FVector CurrentActorLocation = MovementComponent->GetActorLocation();

	const float DeltaH = (CurrentActorLocation - OldLocation) | (-GetGravityDir());

	/* dV = Sqrt(2 m g dH) */
	const float EffectiveGravityScale = FMath::Sign(DeltaH) < 0 ? DownEffectiveGravity : UpEffectiveGravity;
	const float DeltaV = -FMath::Sign(DeltaH) * FMath::Sqrt(2 * GetGravity().Length() * EffectiveGravityScale * FMath::Abs(DeltaH) / 100.f); // NOTE: Dividing by 100 to make the effective gravity scales nicer numbers

	const FVector Tangent = GravityTangentToSurface.IsZero() && MovementComponent->CurrentFloor.bBlockingHit ? MovementComponent->GetDirectionTangentToSurface(GetGravity(), MovementComponent->CurrentFloor.HitResult.ImpactNormal) : FVector::ZeroVector;

	if (bChangeDirection && !Tangent.IsZero())
	{
		MovementComponent->Velocity += Tangent.GetSafeNormal() * FMath::Max(FMath::Abs(DeltaV), 0.1f); // NOTE: Gravity tangent to surface will be zero on flat ground so our FMath::Max check is safe
	}
	else MovementComponent->Velocity = MovementComponent->Velocity.GetSafeNormal() * (MovementComponent->Velocity.Length() + DeltaV);


	MovementComponent->Velocity = MovementComponent->Velocity.GetClampedToMaxSize(MaxSpeed);
}

// TODO: Curve & Rotation Specifiers still need a lot of work, though it's decent at the moment
void UMovementData::CalculateInputVelocity(URadicalMovementComponent* MovementComponent, FVector& Velocity, FVector& Acceleration, float Friction, float BrakingDeceleration, float DeltaTime) const
{
	Friction = FMath::Max(0.f, Friction); // Ensure friction is greater than zero
	
	const float AnalogInputModifier = (Acceleration.SizeSquared() > 0.f && MaxAcceleration > UE_SMALL_NUMBER) ? FMath::Clamp<FVector::FReal>(Acceleration.Size() / MaxAcceleration, 0.f, 1.f) : 0.f;
	const float MaxInputSpeed = FMath::Max(MaxSpeed * (AnalogInputModifier), MinAnalogSpeed);
	
	const bool bZeroAcceleration = Acceleration.IsZero();
	const bool bVelocityOverMax = MovementComponent->IsExceedingMaxSpeed(MaxSpeed);
	const float TurnFrictionFactor = SampleCurve(TurnFrictionCurve, Velocity, bUseTurnFrictionCurve);
	const float ForwardFrictionFactor = SampleCurve(ForwardFrictionCurve, Velocity, bUseForwardFrictionCurve);

	/* Check if path following requested movement */
	bool bZeroRequestedAcceleration = true;
	FVector RequestedAcceleration = FVector::ZeroVector;
	float RequestedSpeed = 0.f;
	
	if (MovementComponent->ApplyRequestedMove(DeltaTime, MaxAcceleration, MaxSpeed, Friction, BrakingDeceleration, RequestedAcceleration, RequestedSpeed))
	{
		bZeroRequestedAcceleration = false;
	}
	
	/* Only apply braking if there is no acceleration or we are over max speed */
	if ((bZeroAcceleration && bZeroRequestedAcceleration) || bVelocityOverMax)
	{
		const FVector OldVelocity = Velocity;

		const float ActualBrakingFriction = (bUseSeparateBrakingFriction ? BrakingFriction : Friction);
		const float BFMultiplier = SampleCurve(BrakingFrictionCurve, Velocity, bUseBrakingFrictionCurve); 
		ApplyVelocityBraking(Velocity, DeltaTime, ActualBrakingFriction * BFMultiplier, BrakingDeceleration);

		/* Don't allow braking to lower us below max speed if we started above it and input is still in the same direction */
		if (bVelocityOverMax && Velocity.SizeSquared() < FMath::Square(MaxSpeed) && FVector::DotProduct(Acceleration, OldVelocity) > 0.f)
		{
			Velocity = Velocity.GetSafeNormal() * MaxSpeed; 
		}
	}
	else if (!bZeroAcceleration)
	{
		/* (Non-Braking) Friction affects our ability to change direction. Only done for input acceleration not path following */
		const FVector AccelDir = Acceleration.GetSafeNormal();
		const float VelSize = Velocity.Size();
		
		if (!bAccelerationRotates)
		{
			// NOTE: How the friction is only actually applied to tangential components of velocity (Want to prevent skidding from being so quick. Also can have a blendspace for skidding animations when Accel & Vel are opposite dirs?)
			const FVector TargetVel = Velocity - (Velocity - AccelDir * ForwardFrictionFactor * VelSize) * FMath::Min(DeltaTime * Friction * TurnFrictionFactor, 1.f);
			Velocity = TargetVel;
		}
		else
		{
			// Testing quaternion acceleration method
			const FVector TargetVel = Velocity - (Velocity  - AccelDir * VelSize ) * FMath::Min(DeltaTime * ForwardFrictionFactor * Friction, 1.f);
			const FQuat TargetVelDir = AccelDir.ToOrientationQuat();
			const FQuat CurrentVelDir = Velocity.ToOrientationQuat();
			
			// Modulate rotation rate by turn friction curve sample and acceleration mag (Otherwise turn speed is always const)
			Velocity = FQuat::Slerp(CurrentVelDir, TargetVelDir, AccelerationRotationRate * TurnFrictionFactor * (Acceleration.Length()/MaxAcceleration) *  DeltaTime).Vector().GetSafeNormal() * TargetVel.Length();
		}
	}

	/* Apply input acceleration */
	if (!bZeroAcceleration)
	{
		const float NewMaxInputSpeed = MovementComponent->IsExceedingMaxSpeed(MaxInputSpeed) ? Velocity.Size() : MaxInputSpeed;
		Velocity += Acceleration * DeltaTime;
		Velocity = Velocity.GetClampedToMaxSize(NewMaxInputSpeed);
	}

	/* Apply additional requested acceleration */
	if (!bZeroRequestedAcceleration)
	{
		const float NewMaxRequestedSpeed = MovementComponent->IsExceedingMaxSpeed(RequestedSpeed) ? Velocity.Size() : RequestedSpeed;
		Velocity += RequestedAcceleration * DeltaTime;
		Velocity = Velocity.GetClampedToMaxSize(NewMaxRequestedSpeed);
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

#pragma endregion Acceleration

#pragma region Air Specific Acceleration

FVector UMovementData::GetFallingLateralAcceleration(const FVector& Acceleration, const FVector& Velocity, float DeltaTime) const
{
	FVector FallAcceleration = FVector(Acceleration.X, Acceleration.Y, 0.f);

	// Bound acceleration, falling object has minimal ability to impact acceleration
	if (FallAcceleration.SizeSquared() > 0.f)
	{
		// Get Air Control
		{
			float BoostAirControl = AirControl;
			if (BoostAirControl != 0.f)
			{
				// Boost air control
				if (AirControlBoostMultiplier > 0.f && Velocity.SizeSquared2D() < FMath::Square(AirControlBoostVelocityThreshold))
				{
					BoostAirControl = FMath::Min(1.f, AirControlBoostMultiplier * BoostAirControl);
				}
			}
			FallAcceleration *= BoostAirControl;
			
		}
		FallAcceleration = FallAcceleration.GetClampedToMaxSize(MaxAcceleration);
	}

	return FallAcceleration;
}

void UMovementData::ApplyGravity(FVector& Velocity, float TerminalLimit, float DeltaTime) const
{
	if ((Velocity | GetGravityDir()) < 0 && bUseSeparateGravityScaleGoingUp)
	{
		Velocity += UpGravityScale * GetGravity() * DeltaTime;
	}
	else Velocity += GetGravity() * DeltaTime;
	
	TerminalLimit = FMath::Abs(TerminalLimit);

	if (Velocity.SizeSquared() > FMath::Square(TerminalLimit))
	{
		if ((Velocity | GetGravityDir()) > TerminalLimit)
		{
			Velocity = FVector::PointPlaneProject(Velocity, FVector::ZeroVector, GetGravityDir()) + GetGravityDir() * TerminalLimit;
		}
	}
}

#pragma endregion Air Specific Acceleration

#pragma region Rotation

void UMovementData::PhysicsRotation(URadicalMovementComponent* MovementComponent, float DeltaTime)
{

	const FQuat CurrentRotation = MovementComponent->UpdatedComponent->GetComponentRotation().Quaternion();
	
	FQuat DesiredRotation = GetDesiredRotation(MovementComponent);


	// Accumulate desired new rotation
	const float AngleTolerance = 1e-3f;

	if (!CurrentRotation.Equals(DesiredRotation, AngleTolerance))
	{
		DesiredRotation = FQuat::Slerp(CurrentRotation, DesiredRotation, RotationRate * DeltaTime);
		MovementComponent->MoveUpdatedComponent(FVector::ZeroVector, DesiredRotation, false);
	}
}

FQuat UMovementData::GetDesiredRotation(const URadicalMovementComponent* MovementComp) const
{
	FVector Acceleration = MovementComp->GetAcceleration();
	const FVector Velocity = MovementComp->GetVelocity();
	const FQuat CurrentRotation = MovementComp->UpdatedComponent->GetComponentRotation().Quaternion();
	
	if (MovementComp->bHasRequestedVelocity)
	{
		// Do it this way since all these variables are local so just wanna use it to compute the right rotation
		Acceleration = MovementComp->RequestedVelocity.GetSafeNormal();
	}
	
	switch (RotationMethod)
	{
		case METHOD_OrientToInput:
			{
				return (MovementComp->IsFalling() && bInputOnlyRotatesWhenFalling) ? GetUniformRotation(MovementComp) : Acceleration.SizeSquared() < UE_KINDA_SMALL_NUMBER ? CurrentRotation : Acceleration.GetSafeNormal().ToOrientationQuat();
			}
			break;
		case METHOD_OrientToGroundAndInput:
			{
				FVector GroundNormal = MovementComp->CurrentFloor.HitResult.ImpactNormal;
				if (MovementComp->IsFalling())
				{
					if (bRevertToGlobalUpWhenFalling)
					{
						GroundNormal = -GetGravityDir();
					}
					else
					{
						// Unsure how to handle this rotation case, maybe just orient to input with player up defining the plane
					}
				}
				const FRotator TargetRot = UKismetMathLibrary::MakeRotFromZX(GroundNormal, Acceleration.SizeSquared() < UE_KINDA_SMALL_NUMBER ? CurrentRotation.GetForwardVector() : FVector::VectorPlaneProject(Acceleration, GroundNormal).GetSafeNormal());
				return (MovementComp->IsFalling() && bInputOnlyRotatesWhenFalling) ? GetUniformRotation(MovementComp) : FQuat::MakeFromRotator(TargetRot);
			}
			break;
		case METHOD_OrientToVelocity:
			{
				const FVector OrientedVel = FVector::VectorPlaneProject(Velocity, -GetGravityDir());
				return (MovementComp->IsFalling() && bInputOnlyRotatesWhenFalling) ? GetUniformRotation(MovementComp) : OrientedVel.SizeSquared() < UE_KINDA_SMALL_NUMBER ? CurrentRotation : OrientedVel.ToOrientationQuat();
			}
			break;
		case METHOD_OrientToGroundAndVelocity:
			{
				FVector GroundNormal = MovementComp->CurrentFloor.HitResult.ImpactNormal;
				if (MovementComp->IsFalling())
				{
					if (bRevertToGlobalUpWhenFalling)
					{
						GroundNormal = -GetGravityDir();
					}
					else
					{
						// Unsure how to handle this rotation case, maybe just orient to input with player up defining the plane
					}
				}
				const FRotator TargetRot = UKismetMathLibrary::MakeRotFromZX(GroundNormal, FVector::VectorPlaneProject(Velocity, GroundNormal).SizeSquared() < UE_KINDA_SMALL_NUMBER ? CurrentRotation.GetForwardVector() : Velocity.GetSafeNormal());
				return (MovementComp->IsFalling() && bInputOnlyRotatesWhenFalling) ? GetUniformRotation(MovementComp) : FQuat::MakeFromRotator(TargetRot);
			}
			break;
		case METHOD_ControllerDesiredRotation:
			{
				if (const AController* ControllerOwner = Cast<AController>(MovementComp->CharacterOwner->GetOwner()))
				{
					return ControllerOwner->GetDesiredRotation().Quaternion();
				}
			}
			break;
	}
	
	return CurrentRotation;
}

FQuat UMovementData::GetUniformRotation(const URadicalMovementComponent* MovementComponent) const
{
	FVector Input = MovementComponent->GetLastInputVector();
	
	/* Transform Input Back To Its Local Basis */
	{
		const FRotator Rotation = MovementComponent->CharacterOwner->Controller->GetControlRotation();
		const FQuat YawRotation = FRotator(0, -Rotation.Yaw, 0).Quaternion();
		Input = YawRotation * Input;
	}
	
	const FQuat CurrentRotation = MovementComponent->UpdatedComponent->GetComponentRotation().Quaternion();

	if (Input.Y == 0) return CurrentRotation;
	
	return UKismetMathLibrary::MakeRotFromXZ(MovementComponent->CharacterOwner->GetActorRightVector() * Input.Y, MovementComponent->CharacterOwner->GetActorUpVector()).Quaternion();
}


#pragma endregion Rotation

#pragma region Getters

FVector UMovementData::ComputeInputAcceleration(URadicalMovementComponent* MovementComponent) const
{
	const FVector RawAccel = MaxAcceleration * FVector::VectorPlaneProject(MovementComponent->GetLastInputVector(), MovementComponent->GetUpOrientation(MODE_Gravity));;
	if (MovementComponent->IsMovingOnGround()) 
	{
		return MovementComponent->GetDirectionTangentToSurface(RawAccel, MovementComponent->CurrentFloor.HitResult.ImpactNormal) * RawAccel.Length();
	}
	return RawAccel;
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

#pragma endregion Getters
