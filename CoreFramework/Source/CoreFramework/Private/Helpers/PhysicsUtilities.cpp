// Copyright 2023 CoC All rights reserved


#include "Helpers/PhysicsUtilities.h"

#include "InputBufferSubsystem.h"
#include "RadicalCharacter.h"
#include "RadicalMovementComponent.h"
#include "Components/SplineComponent.h"
#include "StaticLibraries/CoreMathLibrary.h"

#pragma region Movement Curve

void FCurveMovementParams::Init(URadicalMovementComponent* MovementComponent, float InDuration)
{
	const auto UpdatedComponent = MovementComponent->UpdatedComponent;
	TargetVelUp = MovementComponent->IsMovingOnGround() ? MovementComponent->CurrentFloor.HitResult.ImpactNormal : -MovementComponent->GetGravityDir();

	// Cache Initial Reference Frmae
	{
		RotationReferenceFrame = UpdatedComponent->GetComponentQuat();
		if (ReferenceFrame == EPhysicsCurveRefFrame::InitialInput)
		{
			FVector Input = MovementComponent->GetInputAcceleration().GetSafeNormal();
			if (Input.IsZero()) RotationReferenceFrame = UpdatedComponent->GetComponentQuat();
			else RotationReferenceFrame = UKismetMathLibrary::MakeRotFromXZ(Input, TargetVelUp).Quaternion();
		}

		TargetVelForward = RotationReferenceFrame.GetAxisX();
	}

	EntryVelocity = MovementComponent->GetVelocity();
	EntryRotation = MovementComponent->UpdatedComponent->GetComponentQuat();

	// Bind & Begin
	MovementComponent->RequestSecondaryVelocityBinding().BindRaw(this, &FCurveMovementParams::CalculateVelocity);
	if (RotationType != EPhysicsCurveRotationMethod::None)
	{
		MovementComponent->RequestSecondaryRotationBinding().BindRaw(this, &FCurveMovementParams::UpdateRotation);
	}

	CurrentDuration = 0.f;
	TotalDuration = InDuration;
	InitialMovementState = MovementComponent->GetMovementState();
	bActive = true;
}

void FCurveMovementParams::DeInit(URadicalMovementComponent* MovementComponent)
{
	if (!bActive) return;

	bActive = false;
	
	//const FVector DampenAmount = RotationReferenceFrame * ExitVelocityDampen;
	MovementComponent->Velocity *= ExitVelocityDampen;

	if (MaxExitSpeed >= 0.f)
	{
		MovementComponent->Velocity = MovementComponent->Velocity.GetClampedToMaxSize(MaxExitSpeed);
	}

	// UnBind
	MovementComponent->RequestSecondaryVelocityBinding().Unbind();
	if (RotationType != EPhysicsCurveRotationMethod::None)
	{
		MovementComponent->RequestSecondaryRotationBinding().Unbind();
	}

	// Notify
	CurveEvalComplete.ExecuteIfBound();
}

void FCurveMovementParams::CalculateVelocity(URadicalMovementComponent* MovementComponent, float DeltaTime)
{
	// Iterate time
	FVector OutVelocity;
	CurrentDuration += DeltaTime;
	const float NormalizedTime = CurrentDuration / TotalDuration;

	// Update the reference frame our velocity is meant to be in
	UpdateReferenceFrame(MovementComponent, DeltaTime);

	// Sample curve and get the target velocity based on the curve type
	const FVector CurveSample = MovementCurve.Sample(NormalizedTime);
	// TODO: Maybe option to always treat Z values of the curve as GravDir instead of on ground it being impact normal
	if (MovementComponent->IsMovingOnGround() && !bMaintainPhysicsState && CurveSample.Z > 0.f)
	{
		// Curve sample is meant to send us into aerial if we were grounded
		MovementComponent->SetMovementState(STATE_Falling);
	}

	if (CurveType == EPhysicsCurveType::Position)
	{
		const FVector PrevSample = MovementCurve.Sample(NormalizedTime - DeltaTime);
		OutVelocity = (CurveSample - PrevSample) / (TotalDuration * DeltaTime);
	}
	else // Velocity curve type
	{
		OutVelocity = CurveSample;
	}

	// Now convert the rotation into the appropriate ref frame
	OutVelocity = RotationReferenceFrame * OutVelocity;

	// Check axis ignore if we need to discard anything, either discarding the plane or up vector
	{
		// Calculate velocity regularly to be able to inject it here
		//MovementComponent->GetMovementData()->CalculateVelocity(MovementComponent, DeltaTime);
		MovementComponent->RequestPriorityVelocityBinding().Execute(MovementComponent, DeltaTime);

		if (NormalizedTime >= AxisIgnore.X)
		{
			UCoreMathLibrary::SetVectorAxisValue(RotationReferenceFrame.GetAxisX(), OutVelocity,
												 MovementComponent->Velocity);
		}
		if (NormalizedTime >= AxisIgnore.Y)
		{
			UCoreMathLibrary::SetVectorAxisValue(RotationReferenceFrame.GetAxisY(), OutVelocity,
												 MovementComponent->Velocity);
		}
		if (NormalizedTime >= AxisIgnore.Z) // TODO: If grounded, hitresult.impactnormal is the Z axis here, otherwise gravity if aerial (shouldnt rely on rotation ref frame for these?)
		{
			const FVector UpIgnore = MovementComponent->IsMovingOnGround() ? MovementComponent->CurrentFloor.HitResult.ImpactNormal : MovementComponent->GetGravityDir();
			UCoreMathLibrary::SetVectorAxisValue(UpIgnore, OutVelocity,
												 MovementComponent->Velocity);
		}
	}


	FVector BlendAlpha = FVector::OneVector * NormalizedTime / BlendInTime;
	{
		BlendAlpha.X = BlendInTime.X <= 0.f ? 1.f : BlendAlpha.X;
		BlendAlpha.Y = BlendInTime.Y <= 0.f ? 1.f : BlendAlpha.Y;
		BlendAlpha.Z = BlendInTime.Z <= 0.f ? 1.f : BlendAlpha.Z;
	}

	if (ReferenceFrame == EPhysicsCurveRefFrame::Input || ReferenceFrame == EPhysicsCurveRefFrame::InitialInput) // Edge case with blending time where our rotation gets a bit fucked
	{
		const FVector UpVector = MovementComponent->IsMovingOnGround() ? MovementComponent->CurrentFloor.HitResult.ImpactNormal : MovementComponent->GetGravityDir();
		FVector PlanarVel = RotationReferenceFrame.GetForwardVector() * FVector::VectorPlaneProject(EntryVelocity, UpVector).Size();
		FVector VerticalVel = EntryVelocity.ProjectOnToNormal(RotationReferenceFrame.GetUpVector());
		// Use the same directions, interp magnitude only because we have turn control.
		OutVelocity = UCoreMathLibrary::VInterpAxisValues(RotationReferenceFrame.Rotator(), PlanarVel + VerticalVel, OutVelocity,
												  BlendAlpha);
	}
	else
	{
		OutVelocity = UCoreMathLibrary::VInterpAxisValues(RotationReferenceFrame.Rotator(), EntryVelocity, OutVelocity,
														  BlendAlpha);
	}
	
	MovementComponent->Velocity = OutVelocity;

	// Check for completion
	if (NormalizedTime >= 1.f)
	{
		DeInit(MovementComponent);
	}
}

void FCurveMovementParams::UpdateRotation(URadicalMovementComponent* MovementComponent, float DeltaTime)
{
	const float NormalizedTime = CurrentDuration / TotalDuration;

	if (RotationType == EPhysicsCurveRotationMethod::Physics)
	{
		TGuardValue RestoreAirRotMethod(MovementComponent->GetMovementData()->AirRotationMethod, RotationMethodOverride);
		TGuardValue RestoreGroundRotMethod(MovementComponent->GetMovementData()->GroundRotationMethod, RotationMethodOverride);
		TGuardValue RestoreRotRate(MovementComponent->GetMovementData()->RotationRate, RotationRate);
		MovementComponent->GetMovementData()->UpdateRotation(MovementComponent, DeltaTime);
		return;
	}
	
	// Curve Eval
	const FVector TargetUpVector = MovementComponent->IsMovingOnGround() ? MovementComponent->CurrentFloor.HitResult.ImpactNormal : MovementComponent->UpdatedComponent->GetUpVector();
	
	// We compute this relative to our starting rotation
	const float TargetYaw = FMath::DegreesToRadians(RotationCurve.Sample(NormalizedTime));
	
	FQuat RotApply = FQuat(MovementComponent->UpdatedComponent->GetUpVector(), TargetYaw);

	// Readjust forward vector if our up vector changed a lot
	const FQuat AlignForwardVector = FQuat::FindBetweenNormals(EntryRotation.GetUpVector(), TargetUpVector);
	
	FVector Forward = RotApply * (AlignForwardVector * EntryRotation.GetForwardVector());
	FQuat FinalRotation =  UKismetMathLibrary::MakeRotFromZX(TargetUpVector, Forward).Quaternion();

	MovementComponent->MoveUpdatedComponent(FVector::ZeroVector, FinalRotation, false);
}

void FCurveMovementParams::UpdateReferenceFrame(URadicalMovementComponent* MovementComponent, float DeltaTime)
{
	// Get target forward vector (current forward or input)
	// If grounded, up vector is ground normal
	/// In all cases, we just manipulate the forward vector, the up vector is derived from context
	/// STARTED GROUNDED ///
	/// Grounded: Get target forward vector (Input or rotation forward), UpVector is ground normal
	/// Went To Aerial: Maintain what we had before
	/// STARTED AERIAL ///
	/// Aerial: Get target forward vector (input or rotation forward), UpVector is gravity dir
	/// Went To Grounded: Do the grounded logic as if we started grounded

	FVector NewTargetUp = TargetVelUp;

	// Figure out our up vector
	//if (InitialMovementState == STATE_Grounded)
	{
		// if we've changed states maintain the prev target up & forward, so dont update
		if (MovementComponent->GetMovementState() == STATE_Falling)
		{
			// Or interpolate the up vector back to Grav
			NewTargetUp = UCoreMathLibrary::VectorSlerp(TargetVelUp, -MovementComponent->GetGravityDir(), AerialUpVectorReturnSpeed * DeltaTime);
		}
		else // Else, ensure that target up is always the ground normal
			NewTargetUp = MovementComponent->CurrentFloor.HitResult.ImpactNormal;
	}

	// We've figured out our up vector, lets rotate our target forward to be up-to-date with it
	{
		// Get quaternion that rotates from prev up to new up (TargetVelUp -> NewTargetUp) that is on the plane defined by gravity (so no side-to-side motion)
		const FVector GravPlane = MovementComponent->GetGravityDir();
		const FQuat ForwardRotAdjustment = FQuat::FindBetweenNormals(TargetVelUp, NewTargetUp);
		TargetVelForward = FVector::VectorPlaneProject(ForwardRotAdjustment * TargetVelForward, NewTargetUp).GetSafeNormal(); // NOTE: Fucks up if we don't start in grounded because the scope above isn't correctly setting the UpVector
	}
	FVector NewTargetForward = TargetVelForward;

	// Figure out our forward vector
	if (ReferenceFrame == EPhysicsCurveRefFrame::Local)
	{
		NewTargetForward = MovementComponent->UpdatedComponent->GetForwardVector();
	}
	else if (ReferenceFrame == EPhysicsCurveRefFrame::Input || ReferenceFrame == EPhysicsCurveRefFrame::InitialInput)
	{
		if (!MovementComponent->GetInputAcceleration().IsZero())
			NewTargetForward = MovementComponent->GetInputAcceleration().GetSafeNormal();
		else 
			NewTargetForward = MovementComponent->UpdatedComponent->GetForwardVector();;//TargetVelForward;//RotationReferenceFrame.GetForwardVector();//MovementComponent->UpdatedComponent->GetForwardVector();

		// TODO: IMPORTANT Need to make sure that we always rotate TargetVelForward to be aligned with the ground plane regardless when we do the itnerp, currently causing the same magnitude issues because its too slow to align with the ground via the slerp
		// Alzibda, need to make sure both of these vectors (from,to) are aligned with our UpPlane otherwise we be runnin into issues
		NewTargetForward = UCoreMathLibrary::VectorSlerp(TargetVelForward, NewTargetForward, InputInterpSpeed * DeltaTime);
	}
	
	// Create a quaternion from them as our ref frame
	TargetVelForward = NewTargetForward.GetSafeNormal();
	TargetVelUp = NewTargetUp.GetSafeNormal();

	RotationReferenceFrame = UKismetMathLibrary::MakeRotFromZX(TargetVelUp, TargetVelForward).Quaternion();
}

#pragma region Queries/Helpers

bool FCurveMovementParams::HasNaNs(float InDuration, float& MaxOutputSpeed) const
{
	// Reference frame doesn't matter here
	if (CurveType != EPhysicsCurveType::Position) return false;

	float TimeStep = 1 / (60.f); // 60fps
	if (TimeStep <= UE_SMALL_NUMBER) return false;

	MaxOutputSpeed = 0.f;
	float Time = 0.f;
	float FrameSpeed = 0.f;
	while (Time <= 1.f)
	{
		if (FMath::IsNaN(FrameSpeed)) return true;
		float BackSample = MovementCurve.Sample(Time).Length();
		float ForwardSample = MovementCurve.Sample(Time + TimeStep).Length();
		FrameSpeed = (ForwardSample - BackSample)/(InDuration * TimeStep);
		if (FrameSpeed > MaxOutputSpeed) MaxOutputSpeed = FrameSpeed;
		Time += TimeStep;
	}

	return false;
}

FVector FCurveMovementParams::IntegrateVelocityCurve(float InDuration) const
{
	if (CurveType != EPhysicsCurveType::Velocity) return FVector::ZeroVector;

	float TimeStep = 1 / 60.f; // 60fps
	if (TimeStep <= UE_SMALL_NUMBER) return FVector::ZeroVector;

	FVector DistanceTravelled;
	float Time = 0.f;
	while (Time <= 1.f)
	{
		FVector InstVel = MovementCurve.Sample(Time);
		DistanceTravelled += InstVel * TimeStep;
		Time += TimeStep;
	}

	return DistanceTravelled * InDuration;
}

FString FCurveMovementParams::ValidateCurve(float InDuration) const
{
	float MaxOutputSpeed;
	if (HasNaNs(InDuration, MaxOutputSpeed))
	{
		return "WARNING: Curve has NaNs!";
	}
	
	if (CurveType == EPhysicsCurveType::Position)
		return FString::Printf(TEXT("ALL OK! Max Speed [%.2f m/s]"), MaxOutputSpeed/100.f);
	
	if (CurveType == EPhysicsCurveType::Velocity)
		return FString::Printf(TEXT("ALL OK! Travel Distance [%s]"), *IntegrateVelocityCurve(InDuration).ToCompactString());
	

	return "ALL OK!";
}

#pragma endregion

#pragma endregion

#pragma region Spline Follower

void FPhysicsSplineFollower::Init(USplineComponent* TargetSpline, AActor* TargetActor, const FVector& StartLocation,
	float InitialSpeed)
{
	Spline = TargetSpline;
	OwnerActor = TargetActor;
	FollowSpeed = InitialSpeed;

	TotalDistance = Spline->GetSplineLength();
	CurrentDistance = Spline->GetDistanceAlongSplineAtSplineInputKey(Spline->FindInputKeyClosestToWorldLocation(StartLocation));
	CurrentTangent = Spline->GetTangentAtDistanceAlongSpline(CurrentDistance, ESplineCoordinateSpace::World);

	UpdateFollower(0.f);
}

void FPhysicsSplineFollower::UpdateFollower(float DeltaTime)
{
	if (!(OwnerActor && Spline)) return;

	CurrentDistance += FollowSpeed * DeltaTime;
	const int SplineDirection = FMath::Sign(FollowSpeed);
	
	if (Spline->IsClosedLoop())
	{
		if (SplineDirection > 0 && CurrentDistance >= TotalDistance)
		{
			CurrentDistance -= TotalDistance;
		}
		else if (SplineDirection < 0 && CurrentDistance <= 0)
		{
			CurrentDistance += TotalDistance;
		}
	}


	FollowerTransform = Spline->GetTransformAtDistanceAlongSpline(CurrentDistance, ESplineCoordinateSpace::World);
	FollowerTransform.SetLocation(FollowerTransform.GetLocation() + FollowerTransform.GetRotation().RotateVector(LocalOffset));
	CurrentTangent = Spline->GetTangentAtDistanceAlongSpline(CurrentDistance, ESplineCoordinateSpace::World).GetSafeNormal();
}

bool FPhysicsSplineFollower::IsAtEndOfSpline() const
{
	if (!(OwnerActor && Spline)) return false;
	
	if (Spline->IsClosedLoop()) return false;

	if(FMath::Sign(FollowSpeed) > 0) return CurrentDistance >= TotalDistance;
	return CurrentDistance <= 0;
}

#pragma endregion
