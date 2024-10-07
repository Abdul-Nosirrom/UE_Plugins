// Fill out your copyright notice in the Description page of Project Settings.

#include "MovementData.h"

#include "InputData.h"
#include "RadicalCharacter.h"
#include "RMC_LOG.h"
#include "Components/RadicalMovementComponent.h"
#include "StaticLibraries/CoreMathLibrary.h"

UMovementData::UMovementData()
{
	/*~~~~~ Default UE_DEFAULT Values ~~~~~*/

	GroundFriction = 8.f;
	AerialLateralFriction = 0.f;

	MaxSpeed = 700.f;
	MaxSpeedMultiplier = 1.f;
	MaxAcceleration = 2048.0f;
	MinAnalogSpeed = 0.f;

	BrakingDecelerationGrounded = 0;//FVector2D(0, MaxAcceleration);
	BrakingDecelerationAerial = 0;//FVector2D::ZeroVector;

	AirControl = 0.35f;
	AirControlBoostMultiplier = 2.f;
	AirControlBoostVelocityThreshold = 25.f;

	/*~~~~ Other Defaults ~~~~*/

	bAccelerationRotates = false;

	bUseAccelerationCurve = false;
	bUseForwardFrictionCurve = false;
	bUseTurnFrictionCurve = false;
	bUseBrakingDecelerationCurve = false;

	GroundRotationMethod = METHOD_OrientToGroundAndVelocity;
	AirRotationMethod = METHOD_OrientToGroundAndVelocity;
	
	RotationRate = 10.f;
	bRevertToGlobalUpWhenFalling = true;
}

#pragma region Acceleration

void UMovementData::CalculateVelocity(URadicalMovementComponent* MovementComponent, float DeltaTime)
{
	const FVector GravityDir = MovementComponent->GetGravityDir();

	FVector Velocity = MovementComponent->GetVelocity();
	// NOTE: Not using the getter just so the value we get back reflects any TGuardValue<float> RestoreMaxAccel which wouldnt be reflected in the val computed by the movement comp
	FVector Acceleration = ComputeInputAcceleration(MovementComponent);//MovementComponent->GetInputAcceleration();

	if (MovementComponent->GetMovementState() == STATE_Grounded || MovementComponent->GetMovementState() == STATE_General)
	{
		if (bAccelerationRotates) CalculateArcadeyInputVelocity(MovementComponent, Velocity, Acceleration, GetFriction(STATE_Grounded), GetMaxBrakingDeceleration(STATE_Grounded), DeltaTime);
		else CalculateInputVelocity(MovementComponent, Velocity, Acceleration, GetFriction(STATE_Grounded), GetMaxBrakingDeceleration(STATE_Grounded), DeltaTime);

		if (Velocity.Size() <= TopSpeed) InstanceTopSpeed = TopSpeed;
		else InstanceTopSpeed = Velocity.Size();
		InstanceTopSpeed = FMath::Lerp(InstanceTopSpeed, TopSpeed, DeltaTime * TopSpeedInterpSpeed);
		Velocity = Velocity.GetClampedToMaxSize(InstanceTopSpeed);
	}
	else if (MovementComponent->GetMovementState() == STATE_Falling)
	{
		FVector FallAcceleration = GetFallingLateralAcceleration(Acceleration, Velocity, DeltaTime);
		FallAcceleration = FVector::VectorPlaneProject(FallAcceleration, GravityDir);
		
		{
			TGuardValue<FVector> RestoreAcceleration(Acceleration, FallAcceleration);
			const FVector CachedVerticalVel = Velocity.ProjectOnTo(GravityDir);
			FVector PlanarVel = Velocity - CachedVerticalVel;
			MovementComponent->SetVelocity(PlanarVel); // DEBUG: This is so IsExceedingMaxSpeed has up-to-date velocity information and doesn't account for vertical vel
			if (bAccelerationRotates) CalculateArcadeyInputVelocity(MovementComponent, PlanarVel, Acceleration, GetFriction(STATE_Falling), GetMaxBrakingDeceleration(STATE_Falling), DeltaTime);
			else CalculateInputVelocity(MovementComponent, PlanarVel, Acceleration, GetFriction(STATE_Falling), GetMaxBrakingDeceleration(STATE_Falling), DeltaTime);
			
			if (PlanarVel.Size() <= TopSpeed || true) InstanceTopSpeed = TopSpeed;
			else InstanceTopSpeed = PlanarVel.Size();
			InstanceTopSpeed = FMath::Lerp(InstanceTopSpeed, TopSpeed, DeltaTime * TopSpeedInterpSpeed);
			
			PlanarVel = PlanarVel.GetClampedToMaxSize(InstanceTopSpeed);
			Velocity = PlanarVel + CachedVerticalVel;
		}

		ApplyGravity(MovementComponent, Velocity, MovementComponent->GetPhysicsVolume()->TerminalVelocity, DeltaTime);
	}
	
	MovementComponent->SetVelocity(Velocity);
}

void UMovementData::CalculateVelocityCustomAcceleration(URadicalMovementComponent* MovementComponent, const FVector& Acceleration, float DeltaTime)
{
	// NOTE: Doesn't work because we're recalculating input acceleration so it reflects changes in GuardValue of MaxAcceleration, so we're not using the stored InputAcceleration variable
	TGuardValue<FVector> SwapAcceleration(MovementComponent->InputAcceleration, Acceleration);
	CalculateVelocity(MovementComponent, DeltaTime);
}

void UMovementData::ConserveEnergy(URadicalMovementComponent* MovementComponent, const FVector& OldLocation, float UpEffectiveGravity, float DownEffectiveGravity, bool bChangeDirection, const FVector& GravityTangentToSurface, float EnergyMaxSpeed)
{
	// NOTE: We don't do this for falling because gravity is being applied there and it does essentially the same thing
	if (MovementComponent->IsFalling()) return;
	
	/* Get Change In Height */
	const FVector CurrentActorLocation = MovementComponent->GetActorLocation();

	const float DeltaH = (CurrentActorLocation - OldLocation) | (-MovementComponent->GetGravityDir());

	/* dV = Sqrt(2 m g dH) */
	const float EffectiveGravityScale = FMath::Sign(DeltaH) < 0 ? DownEffectiveGravity : UpEffectiveGravity;
	const float DeltaV = -FMath::Sign(DeltaH) * FMath::Sqrt(2 * MovementComponent->GetGravity().Length() * EffectiveGravityScale * FMath::Abs(DeltaH) / 100.f); // NOTE: Dividing by 100 to make the effective gravity scales nicer numbers

	const FVector Tangent = GravityTangentToSurface.IsZero() && MovementComponent->CurrentFloor.bBlockingHit ? UCoreMathLibrary::GetDirectionTangentToSurface(MovementComponent->GetGravity(), MovementComponent->UpdatedComponent->GetUpVector(), MovementComponent->CurrentFloor.HitResult.ImpactNormal) : FVector::ZeroVector;

	if (bChangeDirection && !Tangent.IsZero())
	{
		MovementComponent->Velocity += Tangent.GetSafeNormal() * FMath::Max(FMath::Abs(DeltaV), 0.1f); // NOTE: Gravity tangent to surface will be zero on flat ground so our FMath::Max check is safe
	}
	else MovementComponent->Velocity = MovementComponent->Velocity.GetSafeNormal() * (MovementComponent->Velocity.Length() + DeltaV);


	MovementComponent->Velocity = MovementComponent->Velocity.GetClampedToMaxSize(EnergyMaxSpeed);
}

// TODO: Curve & Rotation Specifiers still need a lot of work, though it's decent at the moment
void UMovementData::CalculateInputVelocity(URadicalMovementComponent* MovementComponent, FVector& Velocity, FVector& Acceleration, float Friction, float BrakingDeceleration, float DeltaTime) const
{
	Friction = FMath::Max(0.f, Friction); // Ensure friction is greater than zero
	
	const float AnalogInputModifier = (Acceleration.SizeSquared() > 0.f && MaxAcceleration > UE_SMALL_NUMBER) ? FMath::Clamp<FVector::FReal>(Acceleration.Size() / MaxAcceleration, 0.f, 1.f) : 0.f;
	const float MaxInputSpeed = FMath::Max(MaxSpeed * MaxSpeedMultiplier * (AnalogInputModifier), MinAnalogSpeed);
	
	const bool bZeroAcceleration = Acceleration.IsZero();
	const bool bVelocityOverMax = MovementComponent->IsExceedingMaxSpeed(MaxSpeed * MaxSpeedMultiplier);
	const float TurnFrictionFactor = SampleCurve(TurnFrictionCurve, Velocity, bUseTurnFrictionCurve);
	const float ForwardFrictionFactor = SampleCurve(ForwardFrictionCurve, Velocity, bUseForwardFrictionCurve);
	const float MaxVelBrakingDecel = MovementComponent->IsMovingOnGround() ? MaxVelBrakingDecelerationGrounded : MaxVelBrakingDecelerationAerial;

	/* Check if path following requested movement */
	bool bZeroRequestedAcceleration = true;
	FVector RequestedAcceleration = FVector::ZeroVector;
	float RequestedSpeed = 0.f;

	const float BrakingDecelerationToUse = (bVelocityOverMax && bSeparateMaxVelAndInputBrakingDeceleration) ? (bZeroAcceleration ? FMath::Max(MaxVelBrakingDecel, BrakingDeceleration) : MaxVelBrakingDecel) : BrakingDeceleration;

	if (MovementComponent->ApplyRequestedMove(DeltaTime, MaxAcceleration, MaxSpeed * MaxSpeedMultiplier, Friction, BrakingDecelerationToUse, RequestedAcceleration, RequestedSpeed))
	{
		bZeroRequestedAcceleration = false;
	}
	
	/* Only apply braking if there is no acceleration or we are over max speed */
	if ((bZeroAcceleration && bZeroRequestedAcceleration) || bVelocityOverMax)
	{
		const FVector OldVelocity = Velocity;

		const float BFMultiplier = (MovementComponent->IsInAir() ? BrakingFrictionAerial : BrakingFrictionGrounded) * SampleCurve(BrakingDecelerationCurve, Velocity, bUseBrakingDecelerationCurve); 
		ApplyVelocityBraking(Velocity, DeltaTime, BFMultiplier, BrakingDecelerationToUse);

		/* Don't allow braking to lower us below max speed if we started above it and input is still in the same direction */
		if (bVelocityOverMax && Velocity.SizeSquared() < FMath::Square(MaxSpeed * MaxSpeedMultiplier) && FVector::DotProduct(Acceleration, OldVelocity) > 0.f)
		{
			Velocity = Velocity.GetSafeNormal() * MaxSpeed * MaxSpeedMultiplier; 
		}
	}
	else if (!bZeroAcceleration)
	{
		/* (Non-Braking) Friction affects our ability to change direction. Only done for input acceleration not path following */
		const FVector AccelDir = Acceleration.GetSafeNormal();
		const float VelSize = Velocity.Size();
		
		//if (!bAccelerationRotates)
		{
			// NOTE: How the friction is only actually applied to tangential components of velocity (Want to prevent skidding from being so quick. Also can have a blendspace for skidding animations when Accel & Vel are opposite dirs?)
			const FVector TargetVel = Velocity - (Velocity - AccelDir * ForwardFrictionFactor * VelSize) * FMath::Min(DeltaTime * Friction * TurnFrictionFactor, 1.f);
			Velocity = TargetVel;
		}

	}

	/* Apply input acceleration */
	if (!bZeroAcceleration)
	{
		const float NewMaxInputSpeed = MovementComponent->IsExceedingMaxSpeed(MaxInputSpeed) ? Velocity.Size() : MaxInputSpeed;
		const float AccelFactor = SampleCurve(AccelerationCurve, Velocity, bUseAccelerationCurve);
		const float VoA = FMath::Sign(Velocity | Acceleration);

		// Only apply acceleration scaling factors to components parallel to velocity so we don't fuck up turn rates
		if (/*MovementComponent->IsMovingOnGround() &&*/ VoA > 0.f)
		{
			Acceleration = Acceleration.ProjectOnToNormal(Velocity.GetSafeNormal()) * AccelFactor + FVector::VectorPlaneProject(Acceleration, Velocity.GetSafeNormal());
		}
		
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

void UMovementData::CalculateArcadeyInputVelocity(URadicalMovementComponent* MovementComponent, FVector& Velocity,
	FVector& Acceleration, float Friction, float BrakingDeceleration, float DeltaTime) const
{
	constexpr float AimBackForAirBreak = -0.8f; // Compare against dot product of input & velocity when in air
	constexpr float AirBreakTurnAroundSpeed = 100.f; // During air break, if vel is less than this, we just turn around
	
	if (MovementComponent->IsMovingOnGround())
	{
		Friction *= SampleCurve(TurnFrictionCurve, Velocity, bUseTurnFrictionCurve);
	}
	// Setup braking decel to use
	const float MaxVelBrakingDecel = MovementComponent->IsMovingOnGround() ? MaxVelBrakingDecelerationGrounded : MaxVelBrakingDecelerationAerial;
	BrakingDeceleration = (MovementComponent->IsExceedingMaxSpeed(MaxSpeed) && bSeparateMaxVelAndInputBrakingDeceleration) ? (Acceleration.IsZero() ? FMath::Max(MaxVelBrakingDecel, BrakingDeceleration) : MaxVelBrakingDecel) : BrakingDeceleration;
	
	// If on ground, rotate
	FVector TargetDir;
	float TargetSpeed;
	{
		const FVector PlaneNormal = MovementComponent->IsInAir() ? -MovementComponent->GetGravityDir() : MovementComponent->CurrentFloor.HitResult.ImpactNormal;
		TargetDir = MovementComponent->GetLastInputVector(); // Target steering vector
		TargetSpeed = MaxSpeed * MovementComponent->GetLastInputVector().Size();
		
		FVector CurrentVelDirection = Velocity.IsZero() ? MovementComponent->UpdatedComponent->GetForwardVector() : Velocity.GetSafeNormal();
		if (TargetDir.IsZero()) TargetDir = CurrentVelDirection;

		if (MovementComponent->IsFalling()) // Air breaking
		{
			if (Velocity.Size() <= AirBreakTurnAroundSpeed)
			{
				CurrentVelDirection = TargetDir;
			}
			else if (((Velocity.GetSafeNormal() | Acceleration.GetSafeNormal()) < AimBackForAirBreak))
			{
				// Air break
				BrakingDeceleration *= 3.f;
				TargetSpeed = 0.f;
				Friction = 0.f; // We won't be turning, just breaking so remove steering during air-breaks
			}

			// Make sure our TargetHeading & Current Heading are on the UpVector plane
			CurrentVelDirection = FVector::VectorPlaneProject(CurrentVelDirection, PlaneNormal).GetSafeNormal();
			if (CurrentVelDirection.IsZero()) // Could be because we're facing up or down
			{
				CurrentVelDirection = TargetDir; // Only really happens if our velocity on the plane is zero and we're facing fully up or down
			}
		}
		
		{
			FQuat FromRot = UKismetMathLibrary::MakeRotFromZX(PlaneNormal, CurrentVelDirection).Quaternion();
			FQuat ToRot = UKismetMathLibrary::MakeRotFromZX(PlaneNormal, TargetDir).Quaternion();
			TargetDir = FQuat::Slerp(FromRot, ToRot, FMath::Clamp(Friction * DeltaTime, 0, 1)).Vector();//FQuat::Slerp(CurrentVelDirection.ToOrientationQuat(), TargetDir.ToOrientationQuat(), Friction * DeltaTime).Vector();
		}
		// Ensure its properly oriented
		//TargetDir = UCoreMathLibrary::GetDirectionTangentToSurface(TargetDir, MovementComponent->UpdatedComponent->GetUpVector(), PlaneNormal);
	}

	// Braking deceleration applied if we're exceeding our target speed or acceleration is zero
	if (Acceleration.IsZero() || MovementComponent->IsExceedingMaxSpeed(TargetSpeed))
	{
		TargetSpeed = FMath::Max(Velocity.Size() - BrakingDeceleration * DeltaTime, 0.f);
	}
	else // Apply regular acceleration
	{
		const bool bTargetOverCurrent = TargetSpeed > Velocity.Size();
		const float AccelFactor = SampleCurve(AccelerationCurve, Velocity, bUseAccelerationCurve);
		float AccelSpeed = Velocity.Size() + Acceleration.Size() * AccelFactor * DeltaTime; // Might be interesting to try adding the acceleration amount projected onto the TargetDir
		//float AccelSpeed = Velocity.Size() + (Acceleration | TargetDir) * DeltaTime;
		if (bTargetOverCurrent) TargetSpeed = FMath::Clamp(AccelSpeed, 0.f, TargetSpeed); // Don't go over the target if we were below it
	}

	// Finally apply it
	//if (MovementComponent->IsMovingOnGround())
		Velocity = TargetDir * TargetSpeed;
	//else
	//	Velocity += (TargetDir * TargetSpeed - Velocity);
}

void UMovementData::ApplyVelocityBraking(FVector& Velocity, float DeltaTime, float InBrakingFriction, float InBrakingDeceleration) const
{
	if (Velocity.IsZero() ||  DeltaTime < MIN_DELTA_TIME) return;
	
	InBrakingDeceleration = FMath::Max(0.f, InBrakingDeceleration);
	InBrakingFriction = FMath::Max(0.f, InBrakingFriction);
	
	const bool bZeroBraking = (InBrakingDeceleration == 0.f);

	if (bZeroBraking) return;

	const FVector OldVel = Velocity;

	// Decelerate to brake to a stop
	const FVector RevAccel = (bZeroBraking ? FVector::ZeroVector : (-InBrakingDeceleration * Velocity.GetSafeNormal()));
	
	Velocity = Velocity + (RevAccel - InBrakingFriction * Velocity) * DeltaTime;

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

void UMovementData::ApplyGravity(const URadicalMovementComponent* MovementComponent, FVector& Velocity, float TerminalLimit, float DeltaTime) const
{
	Velocity += MovementComponent->GetGravity() * DeltaTime;
	
	TerminalLimit = FMath::Abs(TerminalLimit);

	if (Velocity.SizeSquared() > FMath::Square(TerminalLimit))
	{
		if ((Velocity | MovementComponent->GetGravityDir()) > TerminalLimit)
		{
			Velocity = FVector::PointPlaneProject(Velocity, FVector::ZeroVector, MovementComponent->GetGravityDir()) + MovementComponent->GetGravityDir() * TerminalLimit;
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
		if (MovementComponent->IsFalling() && bRevertToGlobalUpWhenFalling)
		{
			FVector Up = DesiredRotation.GetUpVector();
			FVector Forward = DesiredRotation.GetForwardVector();

			// Slerp 'em
			Up = UCoreMathLibrary::VectorSlerp(CurrentRotation.GetUpVector(), Up, RevertingUpRotationRate * DeltaTime);
			Forward = UCoreMathLibrary::VectorSlerp(CurrentRotation.GetForwardVector(), Forward, RotationRate * DeltaTime);

			DesiredRotation = UKismetMathLibrary::MakeRotFromZX(Up, Forward).Quaternion();
		}
		else 
			DesiredRotation = FQuat::Slerp(CurrentRotation, DesiredRotation, FMath::Clamp(RotationRate * DeltaTime, 0.f, 1.f));
		MovementComponent->MoveUpdatedComponent(FVector::ZeroVector, DesiredRotation, false);
	}
}

FQuat UMovementData::GetDesiredRotation(const URadicalMovementComponent* MovementComp) const
{
	FVector Acceleration = MovementComp->GetInputAcceleration();
	const FVector Velocity = MovementComp->GetVelocity();
	const FQuat CurrentRotation = MovementComp->UpdatedComponent->GetComponentRotation().Quaternion();
	
	if (MovementComp->bHasRequestedVelocity)
	{
		// Do it this way since all these variables are local so just wanna use it to compute the right rotation
		Acceleration = MovementComp->RequestedVelocity.GetSafeNormal();
	}

	const auto TargetRotationMethod = MovementComp->IsFalling() ? AirRotationMethod : GroundRotationMethod;
	
	switch (TargetRotationMethod)
	{
		case METHOD_OrientToInput:
			{
				return Acceleration.SizeSquared() < UE_KINDA_SMALL_NUMBER ? CurrentRotation : Acceleration.GetSafeNormal().ToOrientationQuat();
			}
		case METHOD_OrientToGroundAndInput:
			{
				FVector GroundNormal = MovementComp->CurrentFloor.HitResult.ImpactNormal;
				if (MovementComp->IsFalling())
				{
					if (bRevertToGlobalUpWhenFalling)
					{
						GroundNormal = -MovementComp->GetGravityDir();
					}
					else
					{
						// Unsure how to handle this rotation case, maybe just orient to input with player up defining the plane
						GroundNormal = MovementComp->UpdatedComponent->GetUpVector();
					}
				}
				const FRotator TargetRot = UKismetMathLibrary::MakeRotFromZX(GroundNormal, Acceleration.SizeSquared() < UE_KINDA_SMALL_NUMBER ? CurrentRotation.GetForwardVector() : FVector::VectorPlaneProject(Acceleration, GroundNormal).GetSafeNormal());
				return FQuat::MakeFromRotator(TargetRot);
			}
		case METHOD_OrientToVelocity:
			{
				const FVector OrientedVel = FVector::VectorPlaneProject(Velocity, -MovementComp->GetGravityDir());
				const FRotator OrientedRot = UKismetMathLibrary::MakeRotFromZX(-MovementComp->GetGravityDir(), OrientedVel.IsZero() ? MovementComp->UpdatedComponent->GetForwardVector() : OrientedVel);
				return OrientedRot.Quaternion();
			}
		case METHOD_OrientToGroundAndVelocity:
			{
				FVector GroundNormal = MovementComp->CurrentFloor.HitResult.ImpactNormal;
				if (MovementComp->IsFalling())
				{
					if (bRevertToGlobalUpWhenFalling)
					{
						GroundNormal = -MovementComp->GetGravityDir();
					}
					else
					{
						// Unsure how to handle this rotation case, maybe just orient to input with player up defining the plane
						GroundNormal = MovementComp->UpdatedComponent->GetUpVector();
					}
				}
				const FRotator TargetRot = UKismetMathLibrary::MakeRotFromZX(GroundNormal, FVector::VectorPlaneProject(Velocity, GroundNormal).SizeSquared() < UE_KINDA_SMALL_NUMBER ? CurrentRotation.GetForwardVector() : Velocity.GetSafeNormal());
				return FQuat::MakeFromRotator(TargetRot);
			}
		case METHOD_ControllerDesiredRotation:
			{
				if (const AController* ControllerOwner = Cast<AController>(MovementComp->CharacterOwner->GetOwner()))
				{
					return UKismetMathLibrary::MakeRotFromZX(FVector::UpVector, ControllerOwner->GetDesiredRotation().Quaternion().GetForwardVector()).Quaternion();
				}
			}
	}
	
	return CurrentRotation;
}

FQuat UMovementData::GetUniformRotation(const URadicalMovementComponent* MovementComponent) const
{
	FVector Input = MovementComponent->GetLastInputVector();
	
	/* Transform Input Back To Its Local Basis */
	{
		// Currently passing input locally so commenting this out
		//const FRotator Rotation = MovementComponent->CharacterOwner->Controller->GetControlRotation();
		//const FQuat YawRotation = FRotator(0, -Rotation.Yaw, 0).Quaternion();
		//Input = YawRotation * Input;
	}
	
	const FQuat CurrentRotation = MovementComponent->UpdatedComponent->GetComponentRotation().Quaternion();

	if (Input.Y == 0) return CurrentRotation; 
	
	return UKismetMathLibrary::MakeRotFromXZ(MovementComponent->CharacterOwner->GetActorRightVector() * Input.Y, MovementComponent->CharacterOwner->GetActorUpVector()).Quaternion();
}


#pragma endregion Rotation

#pragma region Getters

FVector UMovementData::ComputeInputAcceleration(URadicalMovementComponent* MovementComponent) const
{
	FVector RawAccel = MovementComponent->GetLastInputVector();
	return RawAccel * MaxAcceleration;
	
	// If we need a > 90 degree rot, we rotate our input axis by an additional 180 to match

	const FVector PlaneNormal = MovementComponent->IsInAir() ? -MovementComponent->GetGravityDir() : MovementComponent->CurrentFloor.HitResult.ImpactNormal;
	FVector CameraUp = FVector::UpVector;
	if (MovementComponent->CharacterOwner->IsPlayerControlled())
	{
		if (auto PCM = UGameplayStatics::GetPlayerCameraManager(MovementComponent, 0))
		{
			CameraUp = PCM->GetCameraRotation().Quaternion().GetUpVector();
		}
	}
	const FQuat PlaneRotation = UKismetMathLibrary::Quat_FindBetweenNormals(CameraUp, PlaneNormal);//UCoreMathLibrary::FromToRotation(FVector::UpVector, PlaneNormal);
	const FVector InputAccel = PlaneRotation * RawAccel;

	return InputAccel * MaxAcceleration;
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
