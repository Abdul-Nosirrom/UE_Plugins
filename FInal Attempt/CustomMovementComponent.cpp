// Fill out your copyright notice in the Description page of Project Settings.


#include "CustomMovementComponent.h"

#include "Prototyping/Gameplay/GenCustomMovementComponent.h"


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
	const auto Mesh = PawnOwner->FindComponentByClass(USkeletalMeshComponent::StaticClass());
	if (Mesh)
	{
		SetSkeletalMeshReference(Cast<USkeletalMeshComponent>(Mesh));
	}

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
		UpdatedPrimitive->OnComponentBeginOverlap.RemoveDynamic(this, &UCustomMovementComponent::RootCollisionTouched);
	}

	Super::SetUpdatedComponent(NewUpdatedComponent);
	
	if (!IsValid(UpdatedComponent) || !IsValid(UpdatedPrimitive))
	{
		DisableMovement();
		return;
	}

	if (PawnOwner->GetRootComponent() != UpdatedComponent)
	{
		FLog(EMessageSeverity::Warning, "New updated component must be the root component");
		PawnOwner->SetRootComponent(UpdatedComponent);
	}

	if (bEnablePhysicsInteractions)
	{
		UpdatedPrimitive->OnComponentBeginOverlap.AddUniqueDynamic(this, &UCustomMovementComponent::RootCollisionTouched);
	}
}


// Called every frame
void UCustomMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                             FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (UpdatedComponent->IsSimulatingPhysics() || ShouldSkipUpdate(DeltaTime))
	{
		DisableMovement();
		return;
	}
	
	if (IsAIControlled())
	{
		CheckAvoidance();
	}

	/* Resolve penetrations that could've been caused from our previous movements or other actor movements */
	//AutoResolvePenetration();

	/* Perform our move */
	PerformMovement(DeltaTime);

	if (ShouldComputeAvoidance())
	{
		AvoidanceLockTimer = FMath::Clamp(AvoidanceLockTimer - DeltaTime, 0.f, BIG_NUMBER);
	}

	if (bEnablePhysicsInteractions)
	{
		ApplyDownwardForce(DeltaTime);
		ApplyRepulsionForce(DeltaTime);
	}

	if (ShouldComputeAvoidance())
	{
		UpdateAvoidance();
	}

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
	InputVector = ConsumeInputVector();

	// Internal Character Move - looking at CMC, it applies UpdateVelocity, RootMotion, etc... before the character move...
	// TODO: CMC mentions scoped is good for performance when there are multiple MoveUpdatedComp calls...
	MovementUpdate(DeltaTime);

	// Tell the updated component what velocity it should have stored after the velocity has been adjusted for sweeps
	UpdatedComponent->ComponentVelocity = Velocity;
	
	// We call these events before getting root motion, as root motion should override them. A separate event to override root motion is provided.
	UpdateRotation(DeltaTime);
	UpdateVelocity(DeltaTime);

	

	// Might also want to make a flag whether we have a skeletal mesh or not in InitializeComponent or something such that we can keep this whole thing general
	const bool bIsPlayingRootMotionMontage = static_cast<bool>(GetRootMotionMontageInstance(SkeletalMesh));
	if (bIsPlayingRootMotionMontage && SkeletalMesh->ShouldTickPose())
	{
		// This will update velocity and rotation based on the root motion
		TickPose(DeltaTime);
	}

	PostMovementUpdate(DeltaTime);
}

void UCustomMovementComponent::PreMovementUpdate(float DeltaTime)
{
#pragma region UpdatePhase1

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
					SelectedGroundProbingDistance = FMath::Max(CapsuleRadius, MaxStepHeight);
				}
				else
				{
					SelectedGroundProbingDistance = GetCapsuleRadius;
				}

				SelectedGroundProbingDistance += GroundDetectionExtraDistance;
			}

			// Now that we've setup our ground probing distance, we're ready to actually do the probing (and snapping, there's one call
			// to MoveUpdatedComp to handle the snapping)

			ProbeGround(UpdatedComponent->GetComponentLocation(), UpdatedComponent->GetComponentQuat(), SelectedGroundProbingDistance, GroundingStatus);

			// Handle projecting our velocity if we just landed this frame
			if (!LastGroundingStatus.bIsStableOnGround && GroundingStatus.bIsStableOnGround)
			{
				/* EVENT: On Landed */
				Velocity = FVector::VectorPlaneProject(Velocity, PawnUp);
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

#pragma endregion UpdatePhase1

	
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

		/* Update movement by the movement amount */
		RemainingMoveDistance *= (1.f - CurrentHitResult.Time);
		
		if (CurrentHitResult.bStartPenetrating)
		{
			/* Get information about the hit to prepare for projection */
			FVector OverlapNormal = CurrentHitResult.ImpactNormal;
			bool bStableOnHit = IsStableOnNormal(OverlapNormal);
			FVector VelBeforeProj = MoveVelocity;
			FVector ObstructionNormal = GetObstructionNormal(OverlapNormal, bStableOnHit);
			
			HandleImpact(CurrentHitResult);
			InternalHandleVelocityProjection(
											bStableOnHit,
											OverlapNormal,
											ObstructionNormal,
											OriginalVelDirection, // eh
											SweepState,
											false,
											FVector::ZeroVector,
											FVector::ZeroVector,
											MoveVelocity,
											RemainingMoveDistance,
											RemainingMoveDirection);
		}
		else if (CurrentHitResult.IsValidBlockingHit())
		{
			/* Update the move distance/time with this sweep iteration */
			RemainingMoveDistance *= (1 - CurrentHitResult.Time);

			/* Setup hit stability evaluation */
			FHitStabilityReport MoveHitStabilityReport;
			EvaluateHitStability(CurrentHitResult, MoveVelocity, MoveHitStabilityReport);

			/* First we check for steps as that could be what we hit, so the move is valid and needs manual override */
			bool bFoundValidStepHit = false;
			if (bSolveGrounding && StepHandling != EStepHandlingMethod::None && MoveHitStabilityReport.bValidStepDetected)
			{
				
			}

			/* If no steps were found, this is just a blocking hit so project against it and handle impact */
			if (!bFoundValidStepHit)
			{
				FVector HitNormal = CurrentHitResult.ImpactNormal;
				// TODO: NOTE, A hit result from a sweep - the impact normal is always the most obstructing normal/most opposed to sweep direction!!!!!!!!!
				// Reorients the obstruction normal based on pawn rotation depending on grounding status
				FVector ObstructionNormal = GetObstructionNormal(HitNormal, MoveHitStabilityReport.bIsStable);

				/* Were we stable when we hit this? */
				// For a crease, this would be false (I think), HitStabilityReport would give us false earlier for bStableOnHit
				bool bStableOnHit = MoveHitStabilityReport.bIsStable && !MustUnground();
				FVector VelocityBeforeProj = MoveVelocity;
			
				HandleImpact(CurrentHitResult);
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

		SweepsMade++;
		if (SweepsMade > MaxMovementIterations)
		{
			if (bKillRemainingMovementWhenExceedMovementIterations) RemainingMoveDistance = 0.f;
			if (bKillVelocityWhenExceedMaxMovementIterations) MoveVelocity = FVector::ZeroVector;
		}
	}
}


void UCustomMovementComponent::PostMovementUpdate(float DeltaTime)
{
	
}

void UCustomMovementComponent::InternalMove(FVector& MoveVelocity, float& DeltaTime)
{
	const FVector MoveDelta = MoveVelocity * DeltaTime;
	FHitResult SweepHit(1.f);
	SafeMoveUpdatedComponent(MoveDelta, UpdatedComponent->GetComponentQuat(), true, SweepHit);
	
	if (SweepHit.bStartPenetrating)
	{
		// Internal velocity project
		HandleImpact(SweepHit);
		SlideAlongSurface(MoveDelta, 1.f, SweepHit.Normal, SweepHit, true);

		if (SweepHit.bStartPenetrating)
		{
			OnStuckInGeometry(&SweepHit);
		}
	}
	else if (SweepHit.IsValidBlockingHit())
	{
		float PercentTimeApplied = SweepHit.Time;
		// The check CMC does right here we don't need to do. Our movement is always projected to the ground so the sweep
		// wouldn't register our current slope as a blocking hit as our movement is always parallel to the slope.
		
		bool bFoundValidStepHit = false;
		FHitStabilityReport MoveHitStabilityReport;
		EvaluateHitStability(SweepHit, MoveVelocity, MoveHitStabilityReport);
		
		// Check if we can step up
		if (bSolveGrounding && StepHandling != EStepHandlingMethod::None && MoveHitStabilityReport.bValidStepDetected)
		{
			// Check Steps

			// If not a step, handle impact and slide along the surface
			if (!bFoundValidStepHit)
			{
				HandleImpact(SweepHit, DeltaTime, MoveDelta);
				SlideAlongSurface(MoveDelta, 1.f - PercentTimeApplied, SweepHit.Normal, SweepHit, true);
			}
		}
		else if (SweepHit.Component.IsValid() && !SweepHit.GetComponent()->CanCharacterStepUp(PawnOwner))
		{
			HandleImpact(SweepHit, DeltaTime, MoveDelta);
			SlideAlongSurface(MoveDelta, 1.f - PercentTimeApplied, SweepHit.Normal, SweepHit, true);
		}
	}
	
}

#pragma endregion Core Update Loop

#pragma region Stability Evaluations

// TODO: Could be inlined?
bool UCustomMovementComponent::MustUnground() const
{
	return bMustUnground;
}



void UCustomMovementComponent::ProbeGround(FVector ProbingPosition, FQuat AtRotation, float ProbingDistance, FGroundingReport& OutGroundingReport)
{
	
}


void UCustomMovementComponent::EvaluateHitStability(FHitResult Hit, FVector MoveVelocity, FHitStabilityReport& OutStabilityReport)
{
	if (bSolveGrounding)
	{
		OutStabilityReport.bIsStable = false;
		return;
	}

	/* Doing everything relative to character up so get that */
	FVector PawnUp = UpdatedComponent->GetUpVector(); // This will be up to date I think so no need to get the AtCharRotation (unless we're evaluating stability of a rotation?)
	FVector InnerHitDirection = FVector::VectorPlaneProject(Hit.ImpactNormal, PawnUp);

	/* Initialize the stability report */
	OutStabilityReport.bIsStable = IsStableOnNormal(Hit.ImpactNormal);
	OutStabilityReport.bFoundInnerNormal = false;
	OutStabilityReport.bFoundOuterNormal = false;
	OutStabilityReport.InnerNormal = Hit.ImpactNormal;
	OutStabilityReport.OuterNormal = Hit.ImpactNormal;

	/* Only really useful for Ledge and Denivelation Handling*/
	if (bLedgeAndDenivelationHandling)
	{
		// TODO: Leaving this for later	
	}

	/* Step Handling */
	if (StepHandling != EStepHandlingMethod::None && !OutStabilityReport.bIsStable)
	{
		/* How important is a check for whether the hit component is simulating physics? Should be fine for now */
		DetectSteps();

		if (OutStabilityReport.bValidStepDetected)
		{
			OutStabilityReport.bIsStable = true;
		}
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

// TODO: Could be inlined
bool UCustomMovementComponent::IsStableOnNormal(FVector Normal) const
{
	return FMath::RadiansToDegrees(FMath::Acos(UpdatedComponent->GetUpVector() | Normal)) <= MaxStableSlopeAngle;
}

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

void UCustomMovementComponent::DetectSteps(FVector CharPosition, FQuat CharRotation, FVector HitPoint, FVector InnerHitDirection, FHitStabilityReport& StabilityReport)
{
	
}

bool UCustomMovementComponent::CheckStepValidity(int numStepHits, FVector CharPosition, FQuat CharRotation, FVector InnerHitDirection, FVector StepCheckStartPost, FHitResult& OutHit)
{
	
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


float UCustomMovementComponent::SlideAlongSurface(const FVector& Delta, float Time, const FVector& InNormal, FHitResult& OutHit, bool bHandleImpact)
{
	if (!OutHit.bBlockingHit) return 0.f;

	FVector Normal(InNormal);
	
	if (GroundingStatus.bIsStableOnGround && !MustUnground())
	{
		const float VerticalNormalFactor = OutHit.Normal | UpdatedComponent->GetUpVector();
		

		// We don't want to be pushed up an unwalkable surface
		if (!IsStableOnNormal(Normal))
		{
			// If the hit normal is pointing down from us, so like a roof we don't want to be pushed down
			if (VerticalNormalFactor < 0.f)
			{
				if (FVector::PointPlaneDist(UpdatedComponent->GetComponentLocation(), GroundingStatus.GroundPoint, GroundingStatus.GroundNormal) < MINIMUM_GROUND_PROBING_DISTANCE && GroundingStatus.GroundHit.bBlockingHit)
				{
					const FVector FloorNormal = GroundingStatus.GroundNormal;
					const bool bFloorOpposedToMovement = (Delta | FloorNormal) < 0.f && (UpdatedComponent->GetUpVector() | FloorNormal) < (1.f - DELTA);

					if (bFloorOpposedToMovement)
					{
						Normal = FloorNormal;
					}

					Normal = FVector::VectorPlaneProject(Normal, UpdatedComponent->GetUpVector()).GetSafeNormal();
				}
			}
			else
			{
				Normal = FVector::VectorPlaneProject(Normal, UpdatedComponent->GetUpVector()).GetSafeNormal();
			}
		}

	}

	return Super::SlideAlongSurface(Delta, Time, Normal, OutHit, bHandleImpact);
}


// Handles creases!
// TODO: Figure out the maths
void UCustomMovementComponent::TwoWallAdjust(FVector& Delta, const FHitResult& Hit, const FVector& OldHitNormal) const
{
	const FVector InDelta = Delta;
	Super::TwoWallAdjust(Delta, Hit, OldHitNormal);

	// This is here to ensure a crease doesn't move us up or doesn't move us down further into the ground if we're grounded along the CreaseDirection in EvaluateCrease
	if (GroundingStatus.bIsStableOnGround && !MustUnground())
	{
		float VerticalMoveFactor = Delta.GetSafeNormal() | UpdatedComponent->GetUpVector();

		// Allow slides up walkable surfaces but not unwalkable ones, treating those as vertical barriers
		if (VerticalMoveFactor > 0)
		{
			const bool bHitStable = IsStableOnNormal(Hit.ImpactNormal);
			
			if (bHitStable)
			{
				// Maintain planar velocity
				const float Time = (1.f - Hit.Time);
				// Adjust mightve changed our magnitude, we want to maintain it
				const FVector ScaledDelta = Delta.GetSafeNormal() * InDelta.Size();

				const FVector InDeltaOnPlane = FVector::VectorPlaneProject(InDelta, UpdatedComponent->GetUpVector()).GetSafeNormal();
				FVector ScaledDeltaOnUpPlane = FVector::VectorPlaneProject(ScaledDelta, UpdatedComponent->GetUpVector());
				const FVector HitNormalOnUpPlane = FVector::VectorPlaneProject(Hit.Normal, UpdatedComponent->GetUpVector());

				ScaledDeltaOnUpPlane = {ScaledDeltaOnUpPlane.X / HitNormalOnUpPlane.X, ScaledDeltaOnUpPlane.Y / HitNormalOnUpPlane.Y, ScaledDeltaOnUpPlane.Z / HitNormalOnUpPlane.Z};
			}
			else
			{
				// If the hit was not stable, we do not project along it
				Delta -= VerticalMoveFactor * UpdatedComponent->GetUpVector();
			}
		}
		else if (VerticalMoveFactor < 0)
		{
			// Don't push down into the floor
			// TODO: Is our grounding status up to date here? If not that explains the multiple ProbeGround calls in CMC. Assuming it is...
			if (FVector::PointPlaneDist(UpdatedComponent->GetComponentLocation(), GroundingStatus.GroundPoint, GroundingStatus.GroundNormal) < MINIMUM_GROUND_PROBING_DISTANCE && GroundingStatus.GroundHit.bBlockingHit)
			{
				Delta -= VerticalMoveFactor * UpdatedComponent->GetUpVector();
			}
		}
	}
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


#pragma endregion Collision Adjustments

#pragma region Physics Interactions

void UCustomMovementComponent::RootCollisionTouched(
								UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
								UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex,
								bool bFromSweep, const FHitResult& SweepResult
								)
{
	if (!OtherComponent || !OtherComponent->IsAnySimulatingPhysics()) return;

	FName BoneName = NAME_None;
	if (OtherBodyIndex != INDEX_NONE)
	{
		const auto OtherSkinnedMeshComponent = Cast<USkinnedMeshComponent>(OtherComponent);
		BoneName = OtherSkinnedMeshComponent->GetBoneName(OtherBodyIndex);
	}

	float TouchForceFactorModified = TouchForceScale;
	// I might want this to always be true
	if (bScaleTouchForceToMass)
	{
		const auto BodyInstance = OtherComponent->GetBodyInstance(BoneName);
		TouchForceFactorModified *= BodyInstance ? BodyInstance->GetBodyMass() : 1.f;
	}

	// GMC Uses 2D, I think we're fine with taking the whole velocity
	float ImpulseStrength = FMath::Clamp(
		Velocity.Size() * TouchForceFactorModified,
		MinTouchForce > 0.f ? MinTouchForce : -FLT_MAX,
		MaxTouchForce > 0.f ? MaxTouchForce : FLT_MAX);

	const FVector OtherComponentLocation = OtherComponent->GetComponentLocation();
	const FVector ShapeLocation = UpdatedComponent->GetComponentLocation();
	// Should we keep this constrained to the 2D plane? Check later on
	FVector ImpulseDirection = FVector(OtherComponentLocation.X - ShapeLocation.X, OtherComponentLocation.Y - ShapeLocation.Y, 0.2f).GetSafeNormal();

	ImpulseDirection = (ImpulseDirection + Velocity) * 0.5f;
	ImpulseDirection.Normalize();

	OtherComponent->AddImpulse(ImpulseDirection * ImpulseStrength, BoneName);
}

#pragma endregion Physics Interactions

#pragma region UNUSED BUT KEEPING FOR SAFETY
void UCustomMovementComponent::MovementUpdate(FVector& MoveVelocity, float DeltaTime)
{
	if (UpdatedPrimitive->GetCollisionEnabled() == ECollisionEnabled::NoCollision)
	{
		MoveUpdatedComponent(MoveVelocity * DeltaTime, UpdatedComponent->GetComponentQuat(), false);
		return;
	}
	
	/* Ensure a valid delta time*/
	if (DeltaTime <= 0.f) return;

	int SweepsMade = 0;

	float RemainingTime = DeltaTime;

	while ((RemainingTime > 0.f) && (SweepsMade < MaxMovementIterations))
	{
		SweepsMade++;
		// TODO: CMC Updates the RemainingTime in a weird way I don't understand it yet, is it better?

		/* Compute Move Parameters */
		const FVector MoveDelta = MoveVelocity * RemainingTime;
		const bool bZeroDelta = MoveDelta.IsNearlyZero();

		if (bZeroDelta) break;
		
		
		/* Save current values */
		const FVector PrevVelocity = MoveVelocity;

		InternalMove(MoveVelocity, RemainingTime);
		
	}
}



#pragma endregion UNUSED BUT KEEPING FOR SAFETY