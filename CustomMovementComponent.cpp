// Fill out your copyright notice in the Description page of Project Settings.


#include "CustomMovementComponent.h"

#include "Kismet/KismetSystemLibrary.h"

// Sets default values for this component's properties
UCustomMovementComponent::UCustomMovementComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	// Set Avoidance Defaults

	// Set Nav-Movement defaults
}


// Called when the game starts
void UCustomMovementComponent::BeginPlay()
{
	// This will reserve this component specifically for pawns, do we want that?
	if (!PawnOwner)
	{
		Super::BeginPlay();
		return;
	}

	// Set a reference to the skeletal mesh of the owning pawn if present
	//const auto Mesh = PawnOwner->FindComponentByClass(USkeletalMeshComponent::StaticClass());
	//if (Mesh)
	//{
	//	SetSkeletalMeshReference(Cast<USkeletalMeshComponent>(Mesh));
	//}

	// Set root collision Shape

	// Cache any data we wanna always hold the original reference to

	// Call Super
	Super::BeginPlay();
}


void UCustomMovementComponent::SetUpdatedComponent(USceneComponent* NewUpdatedComponent)
{
	// Check if it exists
	if (!NewUpdatedComponent)
	{
		return;
	}

	// Check if its valid, and if anything is using the delegate event
	if (IsValid(UpdatedPrimitive) && UpdatedPrimitive->OnComponentBeginOverlap.IsBound())
	{
		//UpdatedPrimitive->OnComponentBeginOverlap.RemoveDynamic(this, &UCustomMovementComponent::RootCollisionTouched);
	}

	Super::SetUpdatedComponent(NewUpdatedComponent);
	
	if (!IsValid(UpdatedComponent) || !IsValid(UpdatedPrimitive))
	{
		DisableMovement();
		return;
	}

	if (PawnOwner->GetRootComponent() != UpdatedComponent)
	{
		//FLog(EMessageSeverity::Warning, "New updated component must be the root component");
		PawnOwner->SetRootComponent(UpdatedComponent);
	}

	if (bEnablePhysicsInteractions)
	{
		//UpdatedPrimitive->OnComponentBeginOverlap.AddUniqueDynamic(this, &UCustomMovementComponent::RootCollisionTouched);
	}
}


/* FUNCTIONAL */
void UCustomMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                             FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (UpdatedComponent->IsSimulatingPhysics() || ShouldSkipUpdate(DeltaTime))
	{
		DisableMovement();
		return;
	}
	
	//if (IsAIControlled())
	//{
	//	CheckAvoidance();
	//}

	/* Resolve penetrations that could've been caused from our previous movements or other actor movements */
	AutoResolvePenetration();

	/* Perform our move */
	PerformMovement(DeltaTime);

	//if (ShouldComputeAvoidance())
	//{
	//	AvoidanceLockTimer = FMath::Clamp(AvoidanceLockTimer - DeltaTime, 0.f, BIG_NUMBER);
	//}

	if (bEnablePhysicsInteractions)
	{
		//ApplyDownwardForce(DeltaTime);
		//ApplyRepulsionForce(DeltaTime);
	}

	//if (ShouldComputeAvoidance())
	//{
	//	UpdateAvoidance();
	//}

	if (UpdatedComponent->IsSimulatingPhysics())
	{
		DisableMovement();
		return;
	}
}


#pragma region Core Update Loop

void UCustomMovementComponent::PerformMovement(float DeltaTime)
{
	// Perform our ground probing and all that here, get setup for the move
	PreMovementUpdate(DeltaTime);

	if (!CanMove())
	{
		HaltMovement();
		return;
	}

	// Store our input vector, we might want to do this earlier however. So long as we do it before the UpdateVelocity call we're good
	//InputVector = ConsumeInputVector();

	if (GroundingStatus.bIsStableOnGround)
	{
		const float VelMag = Velocity.Size();
		Velocity = GetDirectionTangentToSurface(Velocity, GroundingStatus.GroundNormal).GetSafeNormal() * VelMag;
	}
	// Tell the updated component what velocity it should have stored after the velocity has been adjusted for sweeps
	UpdatedComponent->ComponentVelocity = Velocity;
	
	// We call these events before getting root motion, as root motion should override them. A separate event to override root motion is provided.
	//UpdateRotation(UpdatedComponent->GetComponentQuat(), DeltaTime);
	UpdateVelocity(Velocity, DeltaTime);
	
	// Internal Character Move - looking at CMC, it applies UpdateVelocity, RootMotion, etc... before the character move...
	// TODO: CMC mentions scoped is good for performance when there are multiple MoveUpdatedComp calls...
	MovementUpdate(Velocity, DeltaTime);

	

	// Might also want to make a flag whether we have a skeletal mesh or not in InitializeComponent or something such that we can keep this whole thing general
	//const bool bIsPlayingRootMotionMontage = false;//static_cast<bool>(GetRootMotionMontageInstance(SkeletalMesh));
	//if (bIsPlayingRootMotionMontage && SkeletalMesh->ShouldTickPose())
	//{
		// This will update velocity and rotation based on the root motion
	//	TickPose(DeltaTime);
	//}

	PostMovementUpdate(DeltaTime);
}

void UCustomMovementComponent::PreMovementUpdate(float DeltaTime)
{

#pragma region Dirty Move Calls
	/* First handle previous calls to MovePosition to get us up to date with our target position */
	// Should we also handle Rotation calls here too?
	if (bMovePositionDirty)
	{
		FVector MoveVelocity = GetVelocityFromMovement(MovePositionTarget - UpdatedComponent->GetComponentLocation(), DeltaTime);
		if (UpdatedPrimitive->GetCollisionEnabled() != ECollisionEnabled::NoCollision)
		{
			MovementUpdate(MoveVelocity, DeltaTime);
		}
		else
		{
			MoveUpdatedComponent(MoveVelocity * DeltaTime, UpdatedComponent->GetComponentQuat(), false);
		}

		bMovePositionDirty = false;
	}
	if (bMoveRotationDirty)
	{
		// We use moveupdatedcomponent for the sweep and dispatches. Maybe SetRotation would do that too since it has a sweep option
		if (UpdatedPrimitive->GetCollisionEnabled() != ECollisionEnabled::NoCollision)
		{
			// Do we wanna sweep here? Probably yes right? We don't care about the results though since we'll solve it later
			MoveUpdatedComponent(FVector::ZeroVector, MoveRotationTarget, true);
		}
		else
		{
			MoveUpdatedComponent(FVector::ZeroVector, MoveRotationTarget, false);
		}
		bMoveRotationDirty = false;
	}
#pragma endregion Dirty Move Calls
	
	// Equivalent to the Decollision iterations in KCC (sort of, depending on how we do implement the below)
	// What we could do is use our TestMoveUpdatedComp and get our own custom ResolutionDirection and stability reports,
	// or use SafeMoveUpdatedComponent which solves it using the internal implementation... might go with this for now
	/* Resolve penetrations that could've been caused from our previous movements or other actor movements */
	if (UpdatedPrimitive->GetCollisionEnabled() != ECollisionEnabled::NoCollision)
	{
		AutoResolvePenetration(); // This would've been called twice without anything in between if bMovePosDirt, might as well just do it here
	}

	// Copy over previous grounding status
	LastGroundingStatus.CopyFrom(GroundingStatus);

	GroundingStatus = FGroundingReport();
	GroundingStatus.GroundNormal = UpdatedComponent->GetUpVector();
	
	if (bSolveGrounding)
	{
		if (MustUnground())
		{
			// Might get away with enabling sweep, its such a small distance thats mainly for stopping to detect the ground i dont think it matters
			MoveUpdatedComponent(UpdatedComponent->GetUpVector() * (MINIMUM_GROUND_PROBING_DISTANCE * 1.5f), UpdatedComponent->GetComponentQuat(), false);
		}
		else
		{
			// Choose probing distance
			float SelectedGroundProbingDistance = MINIMUM_GROUND_PROBING_DISTANCE;

			if (!LastGroundingStatus.bSnappingPrevented && (LastGroundingStatus.bIsStableOnGround || bLastMovementIterationFoundAnyGround))
			{
				if (StepHandling != EStepHandlingMethod::None)
				{
					SelectedGroundProbingDistance = FMath::Max(UpdatedPrimitive->GetCollisionShape().GetCapsuleRadius(), MaxStepHeight);
				}
				else
				{
					SelectedGroundProbingDistance = UpdatedPrimitive->GetCollisionShape().GetCapsuleRadius();
				}

				SelectedGroundProbingDistance += GroundDetectionExtraDistance;
			}

			// Now that we've setup our ground probing distance, we're ready to actually do the probing (and snapping, there's one call
			// to MoveUpdatedComp to handle the snapping)

			ProbeGround(SelectedGroundProbingDistance, GroundingStatus);

			// Handle projecting our velocity if we just landed this frame
			if (!LastGroundingStatus.bIsStableOnGround && GroundingStatus.bIsStableOnGround)
			{
				/* EVENT: On Landed */
				Velocity = FVector::VectorPlaneProject(Velocity, UpdatedComponent->GetUpVector());
				Velocity = GetDirectionTangentToSurface(Velocity, GroundingStatus.GroundNormal) * Velocity.Size();
			}
		}

		// Handle Leaving/Landing On Moving Base
		// We can maybe place it outside of bSolveGrounding. It would implicitly imply that this whole thing wouldn't run
		// if bSolveGrounding is false since we wouldn't be able to get the components of the ground in that case
		// TODO: Also worth considering where this would fit in with root motion. Since we're not adding velocity and just
		// doing a MovementUpdate with the base velocity, I'm assuming it could fit fine either before or after
		if (bMoveWithBase)
		{
			// Get last moving base
			FHitResult LastGrounding, CurrentGrounding;
			auto LastMovingBase = LastGrounding.GetComponent();
			auto CurrentMovingBase = CurrentGrounding.GetComponent();

			FVector TmpVelFromCurrentBase = FVector::ZeroVector;
			FVector TmpAngularVelFromCurrentBase = FVector::ZeroVector;

			// We don't want to take velocity from something simulating physics due to instability
			if (CurrentMovingBase && CurrentMovingBase->IsSimulatingPhysics())
			{
				TmpVelFromCurrentBase = CurrentMovingBase->GetPhysicsLinearVelocity();
				// Might do this differently
				TmpAngularVelFromCurrentBase = CurrentMovingBase->GetPhysicsAngularVelocityInDegrees();
			}
			
			if ((bImpartBaseVelocity || bImpactAngularBaseVelocity) && LastGroundingStatus.GroundHit.GetComponent() != GroundingStatus.GroundHit.GetComponent())
			{
				// TODO: Finish this, I don't think we want to focus on our current new moving base, instead we probably
				// want to just add velocity from leaving it. Leaving the moving with base to MovementUpdate (might not matter actually)
			}
		
		}
	}
	bMustUnground = false;
	
}

void UCustomMovementComponent::MovementUpdate(FVector& MoveVelocity, float DeltaTime)
{
	if (UpdatedPrimitive->GetCollisionEnabled() == ECollisionEnabled::NoCollision)
	{
		MoveUpdatedComponent(MoveVelocity * DeltaTime, UpdatedComponent->GetComponentQuat(), false);
		return;
	}
	
	/* Ensure a valid delta time*/
	if (DeltaTime <= 0) return;

	/* Initialize sweep iteration data */
	FVector RemainingMoveDirection = MoveVelocity.GetSafeNormal();
	float RemainingMoveDistance = MoveVelocity.Size() * DeltaTime;
	FVector OriginalVelDirection = RemainingMoveDirection;

	EMovementSweepState SweepState = EMovementSweepState::Initial;
	int SweepsMade = 0;
	bool bHitSomethingThisSweepIteration = true;
	FHitResult CurrentHitResult(NoInit);

	/* Initialize previous data to iterate through (e.g checking for creases for example) */
	bool bPreviousHitIsStable = false;
	FVector PreviousVelocity = FVector::ZeroVector;
	FVector PreviousObstructionNormal = FVector::ZeroVector;
	FHitResult PreviousHitResult(NoInit);

	// TODO: In KCC, they first project the move against all current overlaps before doing the sweeps. I'm unsure if we need to do that. If we store them in RootCollisionTouched though then maybe?

	while (RemainingMoveDistance > 0.f && (SweepsMade < MaxMovementIterations) && bHitSomethingThisSweepIteration)
	{
		/* Cache the previous hit result which we'll need for computing creases */
		PreviousHitResult = CurrentHitResult;
		
		/* First sweep with the full current delta which will give us the first blocking hit */
		SafeMoveUpdatedComponent(RemainingMoveDirection * RemainingMoveDistance, UpdatedComponent->GetComponentQuat(), true, CurrentHitResult);
		RemainingMoveDistance *= 1.f - CurrentHitResult.Time;
		FVector RemainingMoveDelta = RemainingMoveDistance * RemainingMoveDirection;
		/* Update movement by the movement amount */

		if (CurrentHitResult.bStartPenetrating && false)
		{
			HandleImpact(CurrentHitResult);
			SlideAlongSurface(RemainingMoveDelta, 1.f, CurrentHitResult.Normal, CurrentHitResult, true);
			GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, "Started Penetrating...");
		}
		else if (CurrentHitResult.bBlockingHit)//CurrentHitResult.IsValidBlockingHit()) //CurrentHitResult.IsValidBlockingHit())
		{
			/* Setup hit stability evaluation */
			FHitStabilityReport MoveHitStabilityReport;
			EvaluateHitStability(CurrentHitResult, MoveVelocity, MoveHitStabilityReport);
			FVector HitNormal = CurrentHitResult.ImpactNormal;

			/* First we check for steps as that could be what we hit, so the move is valid and needs manual override */
			bool bFoundValidStepHit = false;
			if (bSolveGrounding && StepHandling != EStepHandlingMethod::None && MoveHitStabilityReport.bValidStepDetected)
			{
				float ObstructionCorrelation = FMath::Abs(HitNormal | UpdatedComponent->GetUpVector());

				if (ObstructionCorrelation <= CORRELATION_FOR_VERTICAL_OBSTRUCTION)
				{
					FVector StepForwardDirection = FVector::VectorPlaneProject(-HitNormal, UpdatedComponent->GetUpVector()).GetSafeNormal();

					// TODO: 1.) Need to think deeply about this, I like the CMC/GMC method of using a movement scope to go up and forward, check, then go back down...
					// TODO: 2.) KCC actually basically does just this with its collision sweep, substituting that with MoveUpdatedComponent in a deffered update scope like KCC/GMC would actually work just fine!
				}
			}

			/* If no steps were found, this is just a blocking hit so project against it and handle impact */
			if (!bFoundValidStepHit)
			{
				// TODO: NOTE, A hit result from a sweep - the impact normal is always the most obstructing normal/most opposed to sweep direction!!!!!!!!!
				// Reorients the obstruction normal based on pawn rotation depending on grounding status
				FVector ObstructionNormal = GetObstructionNormal(HitNormal, MoveHitStabilityReport.bIsStable);

				/* Were we stable when we hit this? */
				// For a crease, this would be false (I think), HitStabilityReport would give us false earlier for bStableOnHit
				bool bStableOnHit = MoveHitStabilityReport.bIsStable && !MustUnground();
				FVector VelocityBeforeProj = MoveVelocity;
				FVector PrevLocation = UpdatedComponent->GetComponentLocation();
				//HandleImpact(CurrentHitResult);
				//SlideAlongSurface(RemainingMoveDelta, 1.f - CurrentHitResult.Time, CurrentHitResult.Normal, CurrentHitResult, true);

				//FVector UpdateDelta = (UpdatedComponent->GetComponentLocation() - PrevLocation).GetSafeNormal();

				//if (UpdateDelta.SizeSquared() >= 0.f)
				//{
				//	MoveVelocity = UpdateDelta * MoveVelocity.Size();
				//	RemainingMoveDirection = UpdateDelta;
				//}
				
				RemainingMoveDistance *= 1.f - CurrentHitResult.Time;
				/* Project Velocity For Next Move Iteration */

				
				InternalHandleVelocityProjection(
												bStableOnHit,
												CurrentHitResult.ImpactNormal,
												ObstructionNormal,
												OriginalVelDirection,
												SweepState,
												bPreviousHitIsStable,
												PreviousVelocity,
												PreviousObstructionNormal,
												MoveVelocity, RemainingMoveDistance, RemainingMoveDirection); 

				// TODO: Use InternalHandleVelocityProjection, but then perform a move after it along the projected direction and get the hit (if any)
				// use that instead of PrevHitResult...
				
				/* Update previous data */
				bPreviousHitIsStable = bStableOnHit;
				PreviousVelocity = VelocityBeforeProj;
				PreviousObstructionNormal = ObstructionNormal;
			}
		}
		else
		{
			bHitSomethingThisSweepIteration = false;
		}

		//MoveVelocity = RemainingMoveDelta.GetSafeNormal() * MoveVelocity.Size();
		//RemainingMoveDirection = MoveVelocity.GetSafeNormal();

		SweepsMade++;
		if (SweepsMade > MaxMovementIterations)
		{
			if (bKillRemainingMovementWhenExceedMovementIterations) RemainingMoveDistance = 0.f;
			if (bKillVelocityWhenExceedMaxMovementIterations) MoveVelocity = FVector::ZeroVector;
		}
	}

	//MoveUpdatedComponent(RemainingMoveDirection * (RemainingMoveDistance + COLLISION_OFFSET), UpdatedComponent->GetComponentQuat(), true);

}


void UCustomMovementComponent::PostMovementUpdate(float DeltaTime)
{
	
}

#pragma endregion Core Update Loop

#pragma region Stability Evaluations

/* FUNCTIONAL */
bool UCustomMovementComponent::CanMove()
{
	return true;
	if (!UpdatedComponent || !PawnOwner) return false;
	if (UpdatedComponent->Mobility != EComponentMobility::Movable) return false;
	if (bStuckInGeometry) return false;

	return true;
}

/* FUNCTIONAL */
bool UCustomMovementComponent::MustUnground() const
{
	return bMustUnground;
}

/* FUNCTIONAL */
// Could this whole thing be made a lot simpler by sweeping with MoveUpdatedComponent?
void UCustomMovementComponent::ProbeGround(float ProbingDistance, FGroundingReport& OutGroundingReport)
{
	/* Ensure our probing distance is valid */
	if (ProbingDistance < MINIMUM_GROUND_PROBING_DISTANCE)
	{
		ProbingDistance = MINIMUM_GROUND_PROBING_DISTANCE;
	}

	/* Initialize ground sweep data */
	int GroundSweepsMade = 0;
	FHitResult GroundSweepHit(NoInit);
	bool bGroundSweepIsOver = false;
	FVector GroundSweepPosition = UpdatedComponent->GetComponentLocation();
	FVector GroundSweepDirection = -UpdatedComponent->GetUpVector(); // Ground sweeps relative to character orientation
	float GroundProbeDistanceRemaining = ProbingDistance;

	FScopedMovementUpdate GroundSweepScopedUpdate(UpdatedComponent, EScopedUpdate::DeferredUpdates);
	
	/* Begin performing sweep iterations */
	while (GroundProbeDistanceRemaining > 0 && (GroundSweepsMade <= MAX_GROUND_SWEEP_ITERATIONS) && !bGroundSweepIsOver)
	{
		if (GroundSweep(GroundSweepPosition, UpdatedComponent->GetComponentQuat(), GroundSweepDirection, GroundProbeDistanceRemaining, GroundSweepHit))
		{
			/* Evaluate stability of the ground hit */
			// Don't sweep, we just want the hit stability result to have up-to-date information about our location post sweep (target position)
			FVector TargetPosition = GroundSweepPosition + (GroundSweepDirection * GroundSweepHit.Distance);
			FVector SweepDelta = TargetPosition - UpdatedComponent->GetComponentLocation();
			
			MoveUpdatedComponent(SweepDelta, UpdatedComponent->GetComponentQuat(), false);
			FHitStabilityReport GroundHitStabilityReport;
			EvaluateHitStability(GroundSweepHit, Velocity, GroundHitStabilityReport);

			/* Fill out ground information from hit stability report */
			OutGroundingReport.bFoundAnyGround = true;
			OutGroundingReport.GroundNormal = GroundSweepHit.ImpactNormal;
			OutGroundingReport.InnerGroundNormal = GroundHitStabilityReport.InnerNormal;
			OutGroundingReport.OuterGroundNormal = GroundHitStabilityReport.OuterNormal;
			OutGroundingReport.GroundPoint = GroundSweepHit.ImpactPoint;
			OutGroundingReport.GroundHit = GroundSweepHit;
			OutGroundingReport.bSnappingPrevented = false;

			/* Evaluate the stability of the ground */
			if (GroundHitStabilityReport.bIsStable)
			{
				/* Revert the moves we used for sweeps and perform the actual snapping (or not)*/
				GroundSweepScopedUpdate.RevertMove();
				
				/* Find all scenarios where snapping should be cancelled */
				OutGroundingReport.bSnappingPrevented = !IsStableWithSpecialCases(GroundHitStabilityReport, Velocity);
				OutGroundingReport.bIsStableOnGround = true;

				/* Snap us to the ground */
				if (!OutGroundingReport.bSnappingPrevented)
				{
					FHitResult DummyHit;
					SafeMoveUpdatedComponent(GroundSweepDirection * (GroundSweepHit.Distance - COLLISION_OFFSET), UpdatedComponent->GetComponentQuat(), true, DummyHit);
				}

				bGroundSweepIsOver = true;
				return;
			}
			else
			{
				/* Calculate movement from this iteration and advance position*/
				FVector SweepMovement = (GroundSweepDirection * GroundSweepHit.Distance) + UpdatedComponent->GetUpVector() * FMath::Max(COLLISION_OFFSET, GroundSweepHit.Distance);

				/* Set remaining distance */
				GroundProbeDistanceRemaining = FMath::Min(GROUND_PROBING_REBOUND_DISTANCE, FMath::Max(GroundProbeDistanceRemaining - SweepMovement.Size(), 0.f));

				/* Reorient direction */
				GroundSweepDirection = FVector::VectorPlaneProject(GroundSweepDirection, GroundSweepHit.ImpactNormal);
			}
		}
		else
		{
			bGroundSweepIsOver = true;
		}

		GroundSweepsMade++;
	}

	/* No ground found, revert the move regardless */
	GroundSweepScopedUpdate.RevertMove();
}

/* FUNCTIONAL */
void UCustomMovementComponent::EvaluateHitStability(FHitResult Hit, FVector MoveDelta, FHitStabilityReport& OutStabilityReport)
{
	if (!bSolveGrounding)
	{
		OutStabilityReport.bIsStable = false;
		return;
	}

	/* Doing everything relative to character up so get that */
	FVector PawnUp = UpdatedComponent->GetUpVector(); // This will be up to date I think so no need to get the AtCharRotation (unless we're evaluating stability of a rotation?)
	const FVector HitNormal = Hit.ImpactNormal;
	FVector InnerHitDirection = FVector::VectorPlaneProject(HitNormal, PawnUp);

	/* Initialize the stability report */

	OutStabilityReport.bIsStable = IsStableOnNormal(HitNormal);
	OutStabilityReport.bFoundInnerNormal = false;
	OutStabilityReport.bFoundOuterNormal = false;
	OutStabilityReport.InnerNormal = HitNormal;
	OutStabilityReport.OuterNormal = HitNormal;

	/* Only really useful for Ledge and Denivelation Handling*/
	if (bLedgeAndDenivelationHandling)
	{
		/* Setup our raycast information */
		float LedgeCheckHeight = MIN_DISTANCE_FOR_LEDGE;
		if (StepHandling != EStepHandlingMethod::None)
		{
			LedgeCheckHeight = MaxStepHeight;
		}

		FVector InnerStart = Hit.ImpactPoint + (PawnUp * SECONDARY_PROBES_VERTICAL) + (InnerHitDirection * SECONDARY_PROBES_HORIZONTAL);
		FVector OuterStart = Hit.ImpactPoint + (PawnUp * SECONDARY_PROBES_VERTICAL) + (-InnerHitDirection * SECONDARY_PROBES_HORIZONTAL);

		FHitResult CastHit;
		bool bStableLedgeInner = false;
		bool bStableLedgeOuter = false;

		/* Cast for ledge, slightly offset in each case such that if one hits and one doesn't, its a ledge */
		if (CollisionLineCast(InnerStart, -PawnUp, LedgeCheckHeight + SECONDARY_PROBES_VERTICAL, CastHit))
		{
			FVector InnerLedgeNormal = CastHit.ImpactNormal;
			OutStabilityReport.InnerNormal = InnerLedgeNormal;
			OutStabilityReport.bFoundInnerNormal = true;
			bStableLedgeInner = IsStableOnNormal(InnerLedgeNormal);
		}
		if (CollisionLineCast(OuterStart, -PawnUp, LedgeCheckHeight + SECONDARY_PROBES_VERTICAL, CastHit))
		{
			FVector OuterLedgeNormal = CastHit.ImpactNormal;
			OutStabilityReport.OuterNormal = OuterLedgeNormal;
			OutStabilityReport.bFoundOuterNormal = true;
			bStableLedgeOuter = IsStableOnNormal(OuterLedgeNormal);
		}

		/* With both ledge cast information, evaluate whether it is a ledge and fill info accordingly */
		OutStabilityReport.bLedgeDetected = (bStableLedgeInner != bStableLedgeOuter);

		if (OutStabilityReport.bLedgeDetected)
		{
			// fill out information, leaving for later because i wanna understand the math
			OutStabilityReport.bIsOnEmptySideOfLedge = bStableLedgeOuter && !bStableLedgeInner;
			OutStabilityReport.LedgeGroundNormal = bStableLedgeOuter ? OutStabilityReport.OuterNormal : OutStabilityReport.InnerNormal;
			OutStabilityReport.LedgeRightDirection = (HitNormal ^ OutStabilityReport.LedgeGroundNormal).GetSafeNormal();
			OutStabilityReport.LedgeFacingDirection = FVector::VectorPlaneProject((OutStabilityReport.LedgeGroundNormal ^ OutStabilityReport.LedgeFacingDirection).GetSafeNormal(), PawnUp).GetSafeNormal();
			//OutStabilityReport.DistanceFromLedge = FVector::VectorPlaneProject()
			OutStabilityReport.bIsMovingTowardsEmptySideOfLedge = (MoveDelta.GetSafeNormal() | OutStabilityReport.LedgeFacingDirection) > 0.f;
		}
	}

	/* Step Handling */
	if (StepHandling != EStepHandlingMethod::None && !OutStabilityReport.bIsStable)
	{
		/* How important is a check for whether the hit component is simulating physics? Should be fine for now */
		// check if the collider is valid first perhaps
		
		DetectSteps(Hit, InnerHitDirection, OutStabilityReport);

		if (OutStabilityReport.bValidStepDetected) OutStabilityReport.bIsStable = true;
	}
}


void UCustomMovementComponent::EvaluateCrease(FVector MoveVelocity, FVector PrevVelocity, FVector CurHitNormal, FVector PrevHitNormal, bool bCurrentHitIsStable, bool bPreviousHitIsStable, bool bCharIsStable, bool& bIsValidCrease, FVector& CreaseDirection)
{
	/* Initialize out parameters for safety */
	bIsValidCrease = false;
	CreaseDirection = FVector::ZeroVector;

	// bCharIsStable is a bad name, if we're actually evaluating a crease it would be false and we'd enter this check. Look at how EvaluateHitStability sets bStableOnHit which is what is actually passed in here
	// If bPrevHitStable and bCharHitIsStable are both false, then this is a likely candidate to check for a crease - two obstructions basically is what it's saying...
	if (!bCharIsStable || !bCurrentHitIsStable || !bPreviousHitIsStable)
	{
		FVector BlockingCreaseDirection = (CurHitNormal ^ PrevHitNormal).GetSafeNormal();
		const float DotPlanes = CurHitNormal | PrevHitNormal; // Assuming they're normalized
		bool bIsVelocityConstrainedByCrease = false;

		/* Avoid calculations if the two planes are the same */
		if (DotPlanes < 0.999f)
		{
			/* Get the blocking hit normals projected onto the plane defined by the blocking crease direction */
			const FVector NormalAOnCreasePlane = FVector::VectorPlaneProject(CurHitNormal, BlockingCreaseDirection).GetSafeNormal();
			const FVector NormalBOnCreasePlane = FVector::VectorPlaneProject(PrevHitNormal, BlockingCreaseDirection).GetSafeNormal();
			const float DotPlanesOnCreasePlane = NormalAOnCreasePlane | NormalBOnCreasePlane;

			/* Our velocity during the hit projected onto the plane defined by the crease direction */
			const FVector EnteringVelocityDirectionOnCreasePlane = FVector::VectorPlaneProject(PrevVelocity, BlockingCreaseDirection).GetSafeNormal();

			/*
			 * If our velocity points inwards towards the crease basically - so if the crease was -v-, this check would pass if our velocity
			 * was pointed towards the inside of the (v). Done by comparing the dot product of the two normals defining each one side of (v), with their dot product
			 * with our velocity. If our entering velocity is constrained, that means it points inside the (v)
			 */
			if (DotPlanesOnCreasePlane <= (((-EnteringVelocityDirectionOnCreasePlane) | NormalAOnCreasePlane) + 0.001f) &&
				DotPlanesOnCreasePlane <= (((-EnteringVelocityDirectionOnCreasePlane) | NormalBOnCreasePlane) + 0.01f))
			{
				bIsVelocityConstrainedByCrease = true;
			}
		}

		if (bIsVelocityConstrainedByCrease)
		{
			/* Flip the crease direction to make it representative of the real direction our velocity would be projected to */
			if ((BlockingCreaseDirection | MoveVelocity) < 0.f)
			{
				BlockingCreaseDirection = -BlockingCreaseDirection;
			}

			bIsValidCrease = true;
			CreaseDirection = BlockingCreaseDirection;
		}
	}
}

/* FUNCTIONAL */
bool UCustomMovementComponent::IsStableOnNormal(FVector Normal) const
{
	float wangle = FMath::RadiansToDegrees(FMath::Acos(FVector::UpVector | Normal));
	float angle = FMath::RadiansToDegrees(FMath::Acos(UpdatedComponent->GetUpVector() | Normal));
	GEngine->AddOnScreenDebugMessage(-1, 0.f, FColor::Red, "Ground Angle w/ World = " + FString::SanitizeFloat(wangle));
	GEngine->AddOnScreenDebugMessage(-1, 0.f, FColor::Red, "Ground Angle w/ Pawn = " + FString::SanitizeFloat(angle));
	return angle <= MaxStableSlopeAngle;
}

/* FUNCTIONAL */
bool UCustomMovementComponent::IsStableWithSpecialCases(const FHitStabilityReport& StabilityReport, FVector CharVelocity)
{
	/* Only check for special cases if this is enabled, otherwise this method returns true by default */
	if (bLedgeAndDenivelationHandling)
	{
		if (StabilityReport.bLedgeDetected)
		{
			if (StabilityReport.bIsMovingTowardsEmptySideOfLedge)
			{
				const FVector VelocityOnLedgeNormal = CharVelocity.ProjectOnTo(StabilityReport.LedgeFacingDirection);
				if (VelocityOnLedgeNormal.Size() >= MaxVelocityForLedgeSnap)
				{
					return false;
				}
			}

			if (StabilityReport.bIsOnEmptySideOfLedge && StabilityReport.DistanceFromLedge > MaxStableDistanceFromLedge)
			{
				return false;
			}
		}

		if (LastGroundingStatus.bFoundAnyGround && StabilityReport.InnerNormal.SizeSquared() != 0.f && StabilityReport.OuterNormal.SizeSquared() != 0.f)
		{
			float DenivelationAngle = FMath::RadiansToDegrees(FMath::Acos(StabilityReport.InnerNormal.GetSafeNormal() | StabilityReport.OuterNormal.GetSafeNormal()));
			if (DenivelationAngle > MaxStableDenivelationAngle)
			{
				return false;
			}
			else
			{
				DenivelationAngle = FMath::RadiansToDegrees(FMath::Acos(LastGroundingStatus.InnerGroundNormal.GetSafeNormal() | StabilityReport.OuterNormal.GetSafeNormal()));
				if (DenivelationAngle > MaxStableDenivelationAngle)
				{
					return false;
				}
			}
		}
	}

	return true;
}

/* ===== Steps ===== */

/* Not Functional, but not being called right now for testing so not a problem */
void UCustomMovementComponent::DetectSteps(FHitResult Hit, FVector InnerHitDirection, FHitStabilityReport& StabilityReport)
{
	/* Initialize Stepping Vectors */
	FVector PawnUp = UpdatedComponent->GetUpVector();

	// I'm not too sure about VerticalPawnToHit...
	FVector VerticalPawnToHit = (Hit.ImpactPoint - UpdatedComponent->GetComponentLocation()).ProjectOnTo(PawnUp);
	FVector HorizontalPawnToHitDirection = FVector::VectorPlaneProject(Hit.ImpactPoint - UpdatedComponent->GetComponentLocation(), PawnUp).GetSafeNormal();
	float DistanceToFloor = FMath::Abs(VerticalPawnToHit.Size());

	float TravelUpHeight = FMath::Max(MaxStepHeight - DistanceToFloor, 0.f);
	
	FScopedMovementUpdate StepsScopedMovement(UpdatedComponent, EScopedUpdate::DeferredUpdates);

	FHitResult StepSweepHit;

	MoveUpdatedComponent(PawnUp * TravelUpHeight, UpdatedComponent->GetComponentQuat(), true, &StepSweepHit);

	if (StepSweepHit.bStartPenetrating)
	{
		StepsScopedMovement.RevertMove();
	}
	else
	{
		// we wanna check if the ground is walkable (if bAllowSteppingWithoutStableGround)
		// We do this with just raycasts, no need for a sweep or anything
	}

	StabilityReport.bValidStepDetected = true;

	FVector ForwardSweepDelta;
	// Step up was successful, now we check for depth
	if (StepHandling == EStepHandlingMethod::Extra && !StabilityReport.bValidStepDetected)
	{
		ForwardSweepDelta = -InnerHitDirection * MinRequiredStepDepth;
	}
	else
	{
		//ForwardSweepDel
	}
}

bool UCustomMovementComponent::CheckStepValidity(int numStepHits, FVector CharPosition, FQuat CharRotation, FVector InnerHitDirection, FVector StepCheckStartPost, FHitResult& OutHit)
{
	return false;
}



#pragma endregion Stability Evaluations

#pragma region Collision Adjustments

void UCustomMovementComponent::HandleImpact(const FHitResult& Hit, float TimeSlice, const FVector& MoveDelta)
{
	/* Not really important right now for the movement
	IPathFollowingAgentInterface* PFAgent = GetPathFollowingAgent();
	if (PFAgent)
	{
		// Notify path following
		PFAgent->OnMoveBlockedBy(Hit);
	}

	// Notify other pawn
	APawn* OtherPawn = Cast<APawn>(Hit.GetActor());
	if (OtherPawn)
	{
		NotifyBumpedPawn(OtherPawn);
	}

	if (bEnablePhysicsInteractions)
	{
		const FVector ForceAccel = Acceleration + ; // Stuff here we don't have yet to compute the force
	}
	*/ 
}

void UCustomMovementComponent::InternalHandleVelocityProjection(bool bStableOnHit, FVector HitNormal, FVector ObstructionNormal, FVector OriginalDirection, EMovementSweepState& SweepState, bool bPreviousHitIsStable, FVector PrevVelocity, FVector PrevObstructionNormal, FVector& MoveVelocity, float& RemainingMoveDistance, FVector& RemainingMoveDirection)
{
	/* Don't project if our move velocity for the projection is zero */
	if (MoveVelocity.SizeSquared() <= 0.f) return;

	FVector VelocityBeforeProj = MoveVelocity;

	if (bStableOnHit)
	{
		bLastMovementIterationFoundAnyGround = true;
		HandleVelocityProjection(MoveVelocity, ObstructionNormal, bStableOnHit);
	}
	else
	{
		/* Handle Projection - First hit we only have one HitResult so the most we can do is just project against it. Can't check for creases or anything just yet... */
		if (SweepState == EMovementSweepState::Initial)
		{
			HandleVelocityProjection(MoveVelocity, ObstructionNormal, bStableOnHit);
			SweepState = EMovementSweepState::AfterFirstHit;
		}
		/* When here, we have a lot more information about our geometry that we can infer from two hit results (Previous & Current) */
		else if (SweepState == EMovementSweepState::AfterFirstHit)
		{
			bool bFoundCrease = false;
			FVector CreaseDirection = FVector::ZeroVector;
			
			/* Blocking Crease Handling */
			EvaluateCrease(
						MoveVelocity,
						PrevVelocity,
						ObstructionNormal,
						PrevObstructionNormal,
						bStableOnHit,
						bPreviousHitIsStable,
						GroundingStatus.bIsStableOnGround && !MustUnground(),
						bFoundCrease, CreaseDirection);

			/* If we found a crease, that means its constraining our velocity (we are moving into the crease -v- ) */
			if (bFoundCrease)
			{
				if (GroundingStatus.bIsStableOnGround && !MustUnground())
				{
					MoveVelocity = FVector::ZeroVector;
					SweepState = EMovementSweepState::FoundBlockingCorner;
				}
				else
				{
					// TODO: A bit unsure of this, wouldn't this project our velocity downwards or upwards? Is that the intent?
					MoveVelocity = MoveVelocity.ProjectOnTo(CreaseDirection);
					SweepState = EMovementSweepState::FoundBlockingCrease;
				}
			}
			/* If no creases were found, we just continue to project as usual */
			else
			{
				HandleVelocityProjection(MoveVelocity, ObstructionNormal, bStableOnHit);
			}
		}
		else if (SweepState == EMovementSweepState::FoundBlockingCrease)
		{
			MoveVelocity = FVector::ZeroVector;
			SweepState = EMovementSweepState::FoundBlockingCorner;
		}
	}

	float NewVelFactor = MoveVelocity.Size() / VelocityBeforeProj.Size();
	RemainingMoveDistance *= NewVelFactor;
	RemainingMoveDirection = MoveVelocity.GetSafeNormal();
}

FHitResult UCustomMovementComponent::SinglePeneterationResolution()
{
	const FQuat CurrentRotation = UpdatedComponent->GetComponentQuat();
	const FVector TestDelta = {0.01f, 0.01f, 0.01f};
	FHitResult Hit;

	const auto TestMove = [&](const FVector& Delta) {
		FScopedMovementUpdate ScopedMovement(UpdatedComponent, EScopedUpdate::DeferredUpdates);

		// Check for penetrations by applying a small location delta.
		MoveUpdatedComponent(Delta, CurrentRotation, true, &Hit);

		// Revert the movement again afterwards, we are only interested in the hit result.
		ScopedMovement.RevertMove();
	};

	TestMove(TestDelta);

	if (!Hit.bBlockingHit || Hit.IsValidBlockingHit())
	{
		// Apply the test delta in the opposite direction.
		TestMove(-TestDelta);

		if (!Hit.bBlockingHit || Hit.IsValidBlockingHit())
		{
			// The pawn is not in penetration, no action is needed.
			return Hit;
		}
	}
	
	SafeMoveUpdatedComponent(TestDelta, CurrentRotation, true, Hit);
	SafeMoveUpdatedComponent(-TestDelta, CurrentRotation, true, Hit);

	return Hit;
}

FHitResult UCustomMovementComponent::AutoResolvePenetration()
{
	FHitResult Hit = SinglePeneterationResolution();
	
	return Hit;
}

/* FUNCTIONAL */
void UCustomMovementComponent::HandleVelocityProjection(FVector& MoveVelocity, FVector ObstructionNormal, bool bStableOnHit)
{
	if (GroundingStatus.bIsStableOnGround && !MustUnground())
	{
		if (bStableOnHit)
		{
			MoveVelocity = GetDirectionTangentToSurface(MoveVelocity, ObstructionNormal) * MoveVelocity.Size();
		}
		else
		{
			MoveVelocity = FVector::VectorPlaneProject(MoveVelocity, ObstructionNormal);
		}
	}
	else
	{
		MoveVelocity = FVector::VectorPlaneProject(MoveVelocity, ObstructionNormal);
	}
}


float UCustomMovementComponent::SlideAlongSurface(const FVector& Delta, float Time, const FVector& Normal, FHitResult& Hit, bool bHandleImpact)
{
	if (!Hit.bBlockingHit) return 0.f;

	FVector NewNormal = Normal;

	if (GroundingStatus.bIsStableOnGround)
	{
		if (!IsStableOnNormal(NewNormal) || true)
		{
			// Do not push the pawn up an unwalkable surface
			NewNormal = FVector::VectorPlaneProject(NewNormal, GroundingStatus.GroundNormal).GetSafeNormal();
		}
	}

	return Super::SlideAlongSurface(Delta, Time, Normal, Hit, bHandleImpact);
}

void UCustomMovementComponent::TwoWallAdjust(FVector& Delta, const FHitResult& Hit, const FVector& OldHitNormal) const
{
	const FVector InDelta = Delta;
	Super::TwoWallAdjust(Delta, Hit, OldHitNormal);

	if (GroundingStatus.bIsStableOnGround)
	{
		/* If the super projected our movement upwards, make sure its walkable */
		if ((Delta | UpdatedComponent->GetUpVector()) > 0.f)
		{
			if (!IsStableOnNormal(Hit.Normal))
			{
				Delta = FVector::VectorPlaneProject(Delta, GroundingStatus.GroundNormal).GetSafeNormal();
			}
		}
	}
}



#pragma endregion Collision Adjustments


#pragma region Collision Checks

bool UCustomMovementComponent::GroundSweep(FVector Position, FQuat Rotation, FVector Direction, float Distance, FHitResult& OutHit)
{
	/* Initialize sweep data */
	const TArray<AActor*> ActorsToIgnore = {PawnOwner};
	const FCollisionShape CollisionShape = UpdatedPrimitive->GetCollisionShape();
	const float Radius = CollisionShape.GetCapsuleRadius();
	const float HalfHeight = CollisionShape.GetCapsuleHalfHeight();
	
	return UKismetSystemLibrary::CapsuleTraceSingle(
											GetWorld(),
											Position,
											Position + Direction * (Distance + GROUND_PROBING_BACKSTEP_DISTANCE),
											Radius, HalfHeight,
											UEngineTypes::ConvertToTraceType(UpdatedComponent->GetCollisionObjectType()),
											false,
											ActorsToIgnore,
											bDebugGroundSweep ? EDrawDebugTrace::ForOneFrame : EDrawDebugTrace::None,
											OutHit,
											true,
											GroundSweepDebugColor, GroundSweepHitDebugColor);
}


bool UCustomMovementComponent::CollisionLineCast(FVector StartPoint, FVector Direction, float Distance, FHitResult& OutHit)
{
	const TArray<AActor*> ActorsToIgnore = {PawnOwner};

	return UKismetSystemLibrary::LineTraceSingle(
											GetWorld(),
											StartPoint,
											StartPoint + Direction * Distance,
											UEngineTypes::ConvertToTraceType(UpdatedComponent->GetCollisionObjectType()),
											false,
											ActorsToIgnore,
											bDebugLineTrace ? EDrawDebugTrace::ForOneFrame : EDrawDebugTrace::None,
											OutHit,
											true,
											LineTraceDebugColor, LineTraceHitDebugColor);

}

#pragma endregion Collision Checks

#pragma region Exposed Calls

void UCustomMovementComponent::HaltMovement()
{
	
}

void UCustomMovementComponent::DisableMovement()
{
	
}

void UCustomMovementComponent::EnableMovement()
{
	
}


void UCustomMovementComponent::ForceUnground()
{
	bMustUnground = true;
}

void UCustomMovementComponent::MoveCharacter(FVector ToPosition)
{
	
}

void UCustomMovementComponent::RotateCharacter(FQuat ToRotation)
{
	
}

void UCustomMovementComponent::ApplyState(FMotorState StateToApply)
{
	
}


FVector UCustomMovementComponent::GetVelocityFromMovement(FVector MoveDelta, float DeltaTime)
{
	if (DeltaTime <= 0.f) return FVector::ZeroVector;

	return MoveDelta / DeltaTime;
}







#pragma endregion Exposed Calls

#pragma region Physics Interactions


#pragma endregion Physics Interactions