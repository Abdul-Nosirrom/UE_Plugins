// Fill out your copyright notice in the Description page of Project Settings.
#include "CustomMovementComponent.h"
#include "Components/ShapeComponent.h"
#include "Engine/ScopedMovementUpdate.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/KismetSystemLibrary.h"
#include "MovementTesting/MovementTestingCharacter.h"

#define DRAW_LINE(Start, End, Color) DrawDebugLine(GetWorld(), Start, End, Color, false, 3.f, 0, 3.f);
#define DRAW_POINT(Pos, Color) DrawDebugPoint(GetWorld(), Pos, 3.f, Color, false, 3.f, 0.f);
#define DRAW_SPHERE(Pos, Color) DrawDebugSphere(GetWorld(), Pos, 10.f, 5, Color, false, 3.f);
#define DRAW_CAPSULE(Pos, Rot, HalfHeight, Radius, Color) DrawDebugCapsule(GetWorld(), Pos, HalfHeight, Radius, Rot, Color, false, 0.f, 0, 3.f);

#define LOG_SCREEN(Key, Time, Color, Msg) GEngine->AddOnScreenDebugMessage(Key, Time, Color, Msg, false);
#define BOOL_STR(Val) (Val ? FString("True") : FString("False"))
#define VECTOR_STR(Val) FString("(" + FString::SanitizeFloat(Val.X) + ", " + FString::SanitizeFloat(Val.Y) + ", " + FString::SanitizeFloat(Val.Z) + ")")

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
		//FLog(EMessageSeverity::Warning, "New updated component must be the root component");
		PawnOwner->SetRootComponent(UpdatedComponent);
	}

	if (bEnablePhysicsInteraction)
	{
		UpdatedPrimitive->OnComponentBeginOverlap.AddUniqueDynamic(this, &UCustomMovementComponent::RootCollisionTouched);
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

	if (bEnablePhysicsInteraction)
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

	/* LOG SIMULATION STATE */

	LOG_SCREEN(0, 2.f, FColor::Yellow, "-----Locomotion Status-----");
	LOG_SCREEN(1, 2.f, GroundingStatus.bIsStableOnGround ? FColor::Green: FColor::Red, "Grounding Status: " + (GroundingStatus.bFoundAnyGround ? (GroundingStatus.bIsStableOnGround ? FString("(STABLE)") : FString("(NOT STABLE)")) : FString("(NONE)")));
	LOG_SCREEN(2, 2.f, DebugSimulationState.bValidatedSteps ? FColor::Green : FColor::Red, "Valid Steps: " + (DebugSimulationState.bValidatedSteps ? FString("(VALID)") : FString("(INVALID)")));
	LOG_SCREEN(3, 2.f, DebugSimulationState.bIsMovingWithBase ? FColor::Green : FColor::Red, "Moving Base: " + (DebugSimulationState.bIsMovingWithBase ? FString("(VALID)") : FString("(INVALID)")));
	LOG_SCREEN(4, 2.f, DebugSimulationState.bIsMovingWithBase ? FColor::Green : FColor::Red, "Base Velocity = " + (DebugSimulationState.bIsMovingWithBase ? VECTOR_STR(DebugSimulationState.MovingBaseVelocity) : FString("X")));
	if (DebugSimulationState.bFoundLedge)
	{
		LOG_SCREEN(5, 2.f, FColor::Green, "Ledge Detected, Drawing Debug Point...");
		DRAW_SPHERE(GroundingStatus.GroundPoint, FColor::Purple);
		DebugSimulationState.bFoundLedge = false;
	}
	else
	{
		LOG_SCREEN(5, 2.f, FColor::Red, "No Ledge Detected...");
	}
	
	LOG_SCREEN(6, 2.f, FColor::White, "Ground Angle = " + FString::SanitizeFloat(DebugSimulationState.GroundAngle));

	LOG_SCREEN(7, 2.f, FColor::White, "Speed = " + FString::SanitizeFloat(Velocity.Length() / 100.f) + " m/s");

	LOG_SCREEN(10, 2.f, DebugSimulationState.bSnappingPrevented ? FColor::Green : FColor::Red, "Snapping Prevented? " + BOOL_STR(DebugSimulationState.bSnappingPrevented));
	LOG_SCREEN(11, 2.f, FColor::White, "Last Ground Snapping Distance = " + FString::SanitizeFloat(DebugSimulationState.LastGroundSnappingDistance))
	LOG_SCREEN(12, 2.f, FColor::White, "Last Exceeded Denivelation Angle = " + FString::SanitizeFloat(DebugSimulationState.LastExceededDenivelationAngle));
	LOG_SCREEN(13, 2.f, FColor::White, "Last Successful Step Up Height = " + FString::SanitizeFloat(DebugSimulationState.LastSuccessfulStepHeight));

	LOG_SCREEN(14, 2.f, FColor::Blue, "===== ANIMATION =====");
	LOG_SCREEN(15, 2.f, DebugSimulationState.bIsPlayingRM ? FColor::Green : FColor::Red, "Playing Root Motion: " + BOOL_STR(DebugSimulationState.bIsPlayingRM));
	LOG_SCREEN(16, 2.f, SkeletalMesh ? FColor::Green : FColor::Red, "Valid Skeletal Mesh: " + (SkeletalMesh ? FString("(VALID)") : FString("(INVALID)")));
	if (DebugSimulationState.bIsPlayingRM)
	{
		LOG_SCREEN(17, 2.f, FColor::White, "Root Motion Velocity = " + VECTOR_STR(DebugSimulationState.RootMotionVelocity));
		LOG_SCREEN(18, 2.f, FColor::White, "Root Motion Position = " + FString::SanitizeFloat(DebugSimulationState.MontagePosition));
	}
	
	LOG_SCREEN(19, 2.f, FColor::Yellow, "---------------------------");
}


#pragma region Core Update Loop

void UCustomMovementComponent::PerformMovement(float DeltaTime)
{
	// Perform our ground probing and all that here, get setup for the move
	PreMovementUpdate(DeltaTime);

	if (!CanMove())
	{
		BlockSkeletalMeshPoseTick();
		HaltMovement();
		return;
	}

	// Store our input vector, we might want to do this earlier however. So long as we do it before the UpdateVelocity call we're good
	//InputVector = ConsumeInputVector();

	if (GroundingStatus.bIsStableOnGround)
	{
		const float VelMag = Velocity.Size();
		SetVelocity(GetDirectionTangentToSurface(Velocity, GroundingStatus.GroundNormal).GetSafeNormal() * VelMag);
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
	const bool bIsPlayingRootMotionMontage = static_cast<bool>(GetRootMotionMontageInstance());
	/* REGISTER DEBUG FIELD */
	DebugSimulationState.bIsPlayingRM = bIsPlayingRootMotionMontage;
	if (bIsPlayingRootMotionMontage && SkeletalMesh->ShouldTickPose())
	{
		// This will update velocity and rotation based on the root motion
		TickPose(DeltaTime);
	}

	PostMovementUpdate(DeltaTime);
}

void UCustomMovementComponent::PreMovementUpdate(float DeltaTime)
{

	// TODO: Note, dirty moves/rotations might not be necessary since we don't need to cache them. It does solve for slopes and such though.
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
				FVector LandedVelocity = FVector::VectorPlaneProject(Velocity, GroundingStatus.GroundNormal);
				LandedVelocity = GetDirectionTangentToSurface(LandedVelocity, GroundingStatus.GroundNormal) * LandedVelocity.Size();
				SetVelocity(LandedVelocity);
			}
		}

		// Handle Leaving/Landing On Moving Base
		// We can maybe place it outside of bSolveGrounding. It would implicitly imply that this whole thing wouldn't run
		// if bSolveGrounding is false since we wouldn't be able to get the components of the ground in that case
		// TODO: Also worth considering where this would fit in with root motion. Since we're not adding velocity and just
		// doing a MovementUpdate with the base velocity, I'm assuming it could fit fine either before or after
		if (bMoveWithBase)
		{
			MovementBaseUpdate(DeltaTime);
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

	int SweepsMade = 0;
	bool bHitSomethingThisSweepIteration = true;
	FHitResult CurrentHitResult(NoInit);

	// TODO: In KCC, they first project the move against all current overlaps before doing the sweeps. I'm unsure if we need to do that. If we store them in RootCollisionTouched though then maybe?

	while (RemainingMoveDistance > 0.f && (SweepsMade < MaxMovementIterations) && bHitSomethingThisSweepIteration)
	{
		/* First sweep with the full current delta which will give us the first blocking hit */
		SafeMoveUpdatedComponent(RemainingMoveDirection * RemainingMoveDistance, UpdatedComponent->GetComponentQuat(), true, CurrentHitResult);
		RemainingMoveDistance *= 1.f - CurrentHitResult.Time;
		FVector RemainingMoveDelta = RemainingMoveDistance * RemainingMoveDirection;
		/* Update movement by the movement amount */

		if (CurrentHitResult.bStartPenetrating)
		{
			HandleImpact(CurrentHitResult);
			SlideAlongSurface(RemainingMoveDelta, 1.f, CurrentHitResult.ImpactNormal, CurrentHitResult, true);
		}
		else if (CurrentHitResult.IsValidBlockingHit()) 
		{
			/* Setup hit stability evaluation */
			FHitStabilityReport MoveHitStabilityReport;
			EvaluateHitStability(CurrentHitResult, MoveVelocity, MoveHitStabilityReport); // Steps movement is done here
			FVector HitNormal = CurrentHitResult.ImpactNormal;

			/* First we check for steps as that could be what we hit, so the move is valid and needs manual override */
			bool bFoundValidStepHit = false;
			if (bSolveGrounding && StepHandling != EStepHandlingMethod::None) //&& MoveHitStabilityReport.bValidStepDetected)
			{
				float ObstructionCorrelation = FMath::Abs(HitNormal | UpdatedComponent->GetUpVector());

				if (ObstructionCorrelation <= CORRELATION_FOR_VERTICAL_OBSTRUCTION)
				{
					FVector StepForwardDirection = FVector::VectorPlaneProject(-HitNormal, UpdatedComponent->GetUpVector()).GetSafeNormal();
					FHitResult OutForwardHit;
					bFoundValidStepHit = StepUp(CurrentHitResult, StepForwardDirection * RemainingMoveDistance, &OutForwardHit);
					MoveHitStabilityReport.bValidStepDetected = bFoundValidStepHit;

					if (bFoundValidStepHit)
					{
						RemainingMoveDistance *= (1.f - OutForwardHit.Time);
					}

					/* REGISTER DEBUG FIELD */
					DebugSimulationState.bValidatedSteps = bFoundValidStepHit;
				}
			}

			/* If no steps were found, this is just a blocking hit so project against it and handle impact */
			if (!bFoundValidStepHit)
			{
				/* Were we stable when we hit this? */
				// For a crease, this would be false (I think), HitStabilityReport would give us false earlier for bStableOnHit
				bool bStableOnHit = MoveHitStabilityReport.bIsStable && !MustUnground();
				FVector ObstructionNormal = GetObstructionNormal(CurrentHitResult.ImpactNormal, bStableOnHit);
				HandleImpact(CurrentHitResult);
				HandleVelocityProjection(MoveVelocity, ObstructionNormal, bStableOnHit);

				if (!bStableOnHit)
				{
					float t = 1.f - Super::SlideAlongSurface(RemainingMoveDelta, 1.f - CurrentHitResult.Time, CurrentHitResult.Normal, CurrentHitResult, true);
					RemainingMoveDistance *= t;
				}

				RemainingMoveDirection = MoveVelocity.GetSafeNormal();
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

	MoveUpdatedComponent(RemainingMoveDirection * (RemainingMoveDistance + COLLISION_OFFSET), UpdatedComponent->GetComponentQuat(), false);

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
					if (!LastGroundingStatus.bFoundAnyGround)
					{
						DRAW_SPHERE(UpdatedPrimitive->GetComponentLocation(), FColor::Red);
						DRAW_SPHERE(UpdatedPrimitive->GetComponentLocation() + GroundSweepDirection * (GroundSweepHit.Distance - COLLISION_OFFSET), FColor::Green);
					}
					SafeMoveUpdatedComponent(GroundSweepDirection * (GroundSweepHit.Distance - COLLISION_OFFSET), UpdatedComponent->GetComponentQuat(), true, DummyHit);
					// Log snapping distance if we were aerial previously
					/* REGISTER DEBUG FIELD */
					if (!LastGroundingStatus.bIsStableOnGround && !DebugSimulationState.bFoundLedge)
						DebugSimulationState.LastGroundSnappingDistance = GroundSweepHit.Distance - COLLISION_OFFSET;
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
	FVector InnerHitDirection = FVector::VectorPlaneProject(-HitNormal, PawnUp);

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

		/* REGISTER DEBUG FIELD */
		DebugSimulationState.bFoundLedge = OutStabilityReport.bLedgeDetected;
		DebugSimulationState.GroundAngle = FMath::RadiansToDegrees(FMath::Acos(FVector::UpVector | HitNormal));

		if (OutStabilityReport.bLedgeDetected)
		{
			// fill out information, leaving for later because i wanna understand the math
			OutStabilityReport.bIsOnEmptySideOfLedge = bStableLedgeOuter && !bStableLedgeInner;
			OutStabilityReport.LedgeGroundNormal = bStableLedgeOuter ? OutStabilityReport.OuterNormal : OutStabilityReport.InnerNormal;
			OutStabilityReport.LedgeRightDirection = (HitNormal ^ OutStabilityReport.LedgeGroundNormal).GetSafeNormal();
			OutStabilityReport.LedgeFacingDirection = FVector::VectorPlaneProject((OutStabilityReport.LedgeGroundNormal ^ OutStabilityReport.LedgeFacingDirection).GetSafeNormal(), PawnUp).GetSafeNormal();
			OutStabilityReport.DistanceFromLedge = FVector::VectorPlaneProject(Hit.ImpactPoint + (UpdatedComponent->GetComponentLocation() - UpdatedComponent->GetUpVector() * UpdatedPrimitive->GetCollisionShape().GetCapsuleHalfHeight()), UpdatedComponent->GetUpVector()).Length();
			OutStabilityReport.bIsMovingTowardsEmptySideOfLedge = (MoveDelta.GetSafeNormal() | OutStabilityReport.LedgeFacingDirection) > 0.f;
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

/* FUNCTIONAL */
bool UCustomMovementComponent::IsStableOnNormal(FVector Normal) const
{
	float angle = FMath::RadiansToDegrees(FMath::Acos(UpdatedComponent->GetUpVector() | Normal));
	
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
				/* REGISTER DEBUG FIELD */
				DebugSimulationState.LastExceededDenivelationAngle = DenivelationAngle;
				
				return false;
			}
			else
			{
				DenivelationAngle = FMath::RadiansToDegrees(FMath::Acos(LastGroundingStatus.InnerGroundNormal.GetSafeNormal() | StabilityReport.OuterNormal.GetSafeNormal()));
				if (DenivelationAngle > MaxStableDenivelationAngle)
				{
					/* REGISTER DEBUG FIELD */
					DebugSimulationState.LastExceededDenivelationAngle = DenivelationAngle;
					
					return false;
				}
			}
		}
	}

	return true;
}

/* ===== Steps ===== */

bool UCustomMovementComponent::StepUp(FHitResult StepHit, FVector MoveDelta, FHitResult* OutForwardHit)
{
	/* Determine our planar move delta for this iteration */
	const FVector PawnUp = UpdatedComponent->GetUpVector();
	const FVector StepLocationDelta = FVector::VectorPlaneProject(MoveDelta, PawnUp);

	/* Skip negligible deltas or if step height is 0 (or invalid) */
	if (MaxStepHeight <= 0.f || StepLocationDelta.IsNearlyZero())
	{
		LOG_SCREEN(-1, 1.f, FColor::Red, "Aborting, Step Delta Near Zero...");
		return false;
	}
	
	/* Get start location and accurate floor distance */
	const FVector Extent = UpdatedPrimitive->GetCollisionShape().GetExtent();  // XY Correspond to Radius, Z Corresponds to Half-Height
	const FVector StartLocation = UpdatedComponent->GetComponentLocation();
	float PawnHalfHeight = Extent.Z;
	const FVector UpperBound = (StartLocation + PawnUp * (PawnHalfHeight- Extent.X)); // Lets treat the player center as Z=0, so this would be right before the upper hemispehre

	const FVector BarrierVerticalImpactPoint = StepHit.ImpactPoint;

	// TODO: Work out the maths again using the below
	// (Point | PawnUp) - (StartLocation | PawnUp) == (Point - StartLocation) | PawnUp
	// This gives us the height of a point from our current StartLocation (or height from any point but relative to our basis)
	
	/* Top of the collision is hitting something so we can't go up */
	if (((BarrierVerticalImpactPoint - UpperBound) | PawnUp) > 0) 
	{
		LOG_SCREEN(-1, 5.f, FColor::Red, "Aborting, Barrier Impact Point Higher...");
		return false;
	}

	float TravelUpHeight = MaxStepHeight;
	float TravelDownHeight = TravelUpHeight;
	FVector StartFloorPoint = StartLocation - PawnUp * PawnHalfHeight; // Character lower hemisphere center (Center is Z=0 again...)
	float StartDistanceToFloor = 0.f;

	if (IsStableOnNormal(StepHit.Normal) && !MustUnground())
	{
		StartDistanceToFloor = FMath::Abs((GroundingStatus.GroundHit.Location - StartLocation) | PawnUp);
		// Double because we go up first, then we go down(?)
		TravelDownHeight += 2.f * GROUND_PROBING_REBOUND_DISTANCE;
	}

	TravelUpHeight = FMath::Max(MaxStepHeight - StartDistanceToFloor, 0.f);
	StartFloorPoint -= PawnUp * StartDistanceToFloor;

	/* Impact point under lower bound of collision */
	if (((BarrierVerticalImpactPoint - StartFloorPoint) | PawnUp) <= 0)
	{
		LOG_SCREEN(-1, 5.f, FColor::Red, "Aborting, Barrier Impact Point Lower...");
		return false;
	}

	FScopedMovementUpdate ScopedMovement(UpdatedComponent, EScopedUpdate::DeferredUpdates);

	constexpr float MIN_STEP_UP_DELTA = 5.0f;

	// @attention Barriers are always treated as vertical walls. The procedure is sweeping upward by "TravelUpHeight" (along positive Up),
	// sweeping forward by "StepLocationDelta" (laterally) and sweeping downward by "TravelDownHeight" (along negative Up).
	/* Perform upwards sweep */
	FHitResult UpHit;
	const FQuat PawnRotation = UpdatedComponent->GetComponentQuat();
	MoveUpdatedComponent(PawnUp * TravelUpHeight, PawnRotation, true, &UpHit);
	if (UpHit.bStartPenetrating)
	{
		LOG_SCREEN(-1, 0.1f, FColor::Red, "Aborting Step Up, Upward Sweep Started In Penetration...");
		ScopedMovement.RevertMove();
		return false;
	}

	/* Validate forwards sweep */
	FVector ClampedStepLocationDelta = StepLocationDelta;

	if (StepLocationDelta.Size() < MIN_STEP_UP_DELTA) //TODO: Make this a macro or const expr
	{
		// We need to enforce a minimal location delta for the forward sweep, otherwise we could fail a step-up that should actually be feasible
		// when hitting a non-walkable surface (because the forward step would have been very short).
		ClampedStepLocationDelta = StepLocationDelta.GetClampedToSize(MIN_STEP_UP_DELTA, BIG_NUMBER);
	}

	/* Perform forward sweep */
	FHitResult ForwardHit;
	MoveUpdatedComponent(ClampedStepLocationDelta, PawnRotation, true, &ForwardHit);
	if (OutForwardHit) *OutForwardHit = ForwardHit;
	if (ForwardHit.bBlockingHit)
	{
		if (ForwardHit.bStartPenetrating)
		{
			LOG_SCREEN(-1, 1.f, FColor::Red, "Aborting Step Up, Forward Sweep Started In Penetration...");
			ScopedMovement.RevertMove();
			return false;
		}
		
		if (UpHit.bBlockingHit) HandleImpact(UpHit);
		HandleImpact(ForwardHit);

		// Slide along hit surface
		const float ForwardHitTime = ForwardHit.Time;
		const float SlideTime = SlideAlongSurface(StepLocationDelta, 1.f - ForwardHit.Time, ForwardHit.Normal, ForwardHit, true);

		if (ForwardHitTime == 0.f && SlideTime == 0.f)
		{
			// No movement occured, abort
			LOG_SCREEN(-1, 0.3f, FColor::Red, "Aborting Step Up, Pawn Stuck After Forward Sweep...");
			ScopedMovement.RevertMove();
			return false;
		}
		else
		{
			//StabilityReport.DistanceFromLedge = (1.f - ForwardHitTime) * (1.f - SlideTime);
		}
	}

	/* Perform down sweep */
	FHitResult DownHit;
	MoveUpdatedComponent(-PawnUp * TravelDownHeight, PawnRotation, true, &DownHit);
	if (DownHit.bStartPenetrating)
	{
		LOG_SCREEN(-1, 1.f, FColor::Red, "Aborting Step Up, Downward Sweep Started In Penetration...");
		ScopedMovement.RevertMove();
		return false;
	}
	if (DownHit.bBlockingHit)
	{
		/* Verify the delta we moved is smaller than max step height */
		DRAW_SPHERE(StartFloorPoint, FColor::Green);
		DRAW_SPHERE(DownHit.ImpactPoint, FColor::Blue);
		const float StepDelta = (DownHit.ImpactPoint - StartFloorPoint) | PawnUp;

		if (StepDelta > MaxStepHeight)
		{
			LOG_SCREEN(-1, 1.f, FColor::Red, "Downward Delta Exceeded Maximum Step Height... Step Delta = " + FString::SanitizeFloat(StepDelta));
			DRAW_SPHERE(DownHit.ImpactPoint, FColor::Purple);
			ScopedMovement.RevertMove();
			return false;
		}

		/* Reject unwalkable floor */
		if (!IsStableOnNormal(DownHit.ImpactNormal)) // TODO: This check is borked
		{
			LOG_SCREEN(-1, 1.f, FColor::Red, "Aborting, Step Normal Not Stable...")
			ScopedMovement.RevertMove();
			return false;
		}

		/* Check if the floor below us is valid in terms of its type (i.e reject physics simulations) */
		if (!CheckStepValidity(DownHit, FVector::ZeroVector))
		{
			LOG_SCREEN(-1, 1.f, FColor::Red, "Aborting, Step Normal Not Stable...")
			ScopedMovement.RevertMove();
			return false;
		}
		
		/* REGISTER DEBUG FIELD */
		DebugSimulationState.LastSuccessfulStepHeight = StepDelta;
	}
	else
	{
		/* W*/
		LOG_SCREEN(-1, 0.1f, FColor::Green, "Downward Sweep has no blocking hit...");
	}

	LOG_SCREEN(-1, 0.1f, FColor::Green, "Finished Step Up Successfuly...");
	return true;
}


bool UCustomMovementComponent::CheckStepValidity(FHitResult& StepHit, FVector InnerHitDirection)
{
	return true;
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
	*/
	
	// Notify other pawn
	if (const auto OtherPawn = Cast<APawn>(Hit.GetActor()))
	{
		NotifyBumpedPawn(OtherPawn);
	}
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
	if (bStableOnHit) bLastMovementIterationFoundAnyGround = true;
	
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

	if (IsStableOnNormal(Normal) && !MustUnground()) return 0.f;
	
	FVector NewNormal = Normal;

	if (GroundingStatus.bIsStableOnGround)
	{
		if (!IsStableOnNormal(NewNormal))
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
	const UWorld* World = GetWorld();
	const FCollisionShape CollisionShape = UpdatedPrimitive->GetCollisionShape();
	const FCollisionQueryParams CollisionQueryParams(FName(__func__), false, GetOwner());
	const ECollisionChannel CollisionChannel = UpdatedComponent->GetCollisionObjectType();
	
	const FVector TraceStart = Position;
	const FVector TraceEnd = Position + Direction * (Distance + GROUND_PROBING_BACKSTEP_DISTANCE);
	const FQuat TraceRotation = UpdatedComponent->GetComponentQuat();
	
	const bool bHit = World->SweepSingleByChannel(
											OutHit,
											TraceStart,
											TraceEnd,
											TraceRotation,
											CollisionChannel,
											CollisionShape,
											CollisionQueryParams);
	
	if (bDebugGroundSweep)
	{
		/* ===== Draw Debug ===== */
		DRAW_CAPSULE(TraceStart, TraceRotation, CollisionShape.GetCapsuleHalfHeight(), CollisionShape.GetCapsuleRadius(), GroundSweepDebugColor);

		if (bHit)
		{
			DRAW_CAPSULE(OutHit.Location, TraceRotation, CollisionShape.GetCapsuleHalfHeight(), CollisionShape.GetCapsuleRadius(), GroundSweepHitDebugColor);
		}
		else
		{
			DRAW_CAPSULE(TraceEnd, TraceRotation, CollisionShape.GetCapsuleHalfHeight(), CollisionShape.GetCapsuleRadius(), FColor::Purple);
		}
	}

	return bHit;
}


bool UCustomMovementComponent::CollisionLineCast(FVector StartPoint, FVector Direction, float Distance, FHitResult& OutHit)
{
	const UWorld* World = GetWorld();
	const TArray<AActor*> ActorsToIgnore = {PawnOwner};
	const FCollisionQueryParams CollisionQueryParams(FName(__func__), false, GetOwner());
	const ECollisionChannel CollisionChannel = UpdatedComponent->GetCollisionObjectType();
	
	const FVector TraceStart = StartPoint;
	const FVector TraceEnd = StartPoint + Direction * Distance;

	const bool bHit = World->LineTraceSingleByChannel(
											OutHit,
											TraceStart,
											TraceEnd,
											CollisionChannel,
											CollisionQueryParams);

	if (bDebugLineTrace)
	{
		//Draw Line
		DRAW_LINE(TraceStart, bHit ? OutHit.ImpactPoint : TraceEnd, bHit ? LineTraceHitDebugColor : LineTraceDebugColor);
	}

	return bHit;
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


void UCustomMovementComponent::SetVelocity(const FVector& NewVelocity)
{
	Velocity = NewVelocity;
}

void UCustomMovementComponent::AddVelocity(const FVector& AddVelocity)
{
	Velocity += AddVelocity;
}

void UCustomMovementComponent::AddImpulse(const FVector& Impulse, const bool bVelChange)
{
	if (!Impulse.IsZero())
	{
		if (!bVelChange)
		{
			AddVelocity(Impulse / FMath::Max(Mass, KINDA_SMALL_NUMBER));
		}
		else
		{
			// No scaling by mass i.e. mass is assumed to be 1 kg.
			AddVelocity(Impulse);
		}
	}
}

#pragma endregion Exposed Calls

#pragma region Moving Base
// TODO (1): Figure out comparisons between two UPrimitiveComponents
// TODO (2): Figure out why there's a centrifugal force on rotating bases
// TODO (3): Examine CMCs implementation and make use of MovingBaseUtility static class
void UCustomMovementComponent::MovementBaseUpdate(float DeltaTime)
{
	/* REGISTER DEBUG FIELD */
	DebugSimulationState.bIsMovingWithBase = false;
	
	// Get last moving base
	const auto LastMovingBase = LastGroundingStatus.GroundHit.GetComponent();
	const auto CurrentMovingBase = MovementBaseOverride != nullptr ? MovementBaseOverride : GroundingStatus.GroundHit.GetComponent();
	
	const FVector TmpVelFromCurrentBase = ComputeBaseVelocity(CurrentMovingBase);

	const bool bBaseChanged = !LastGroundingStatus.bFoundAnyGround && (CurrentMovingBase || !IsValidMovementBase(CurrentMovingBase));
	
	/* Conserve momentum when de-stabilized from a moving base */
	if (bImpartBaseVelocity && bBaseChanged && IsValidMovementBase(LastMovingBase))
	{
		// TODO: Finish this, I don't think we want to focus on our current new moving base, instead we probably
		// want to just add velocity from leaving it. Leaving the moving with base to MovementUpdate (might not matter actually)
		const FVector VelocityFromPrevBase = ComputeBaseVelocity(LastMovingBase);
		AddVelocity(VelocityFromPrevBase - TmpVelFromCurrentBase);
		
	}

	/* Cancel out planar velocity upon landing on a valid moving base */
	if (bBaseChanged)
	{
		LOG_SCREEN(-1, 2.f, FColor::Blue, "LANDED ON MOVING BASE")
		AddVelocity(-FVector::VectorPlaneProject(TmpVelFromCurrentBase, UpdatedComponent->GetUpVector()));
	}


	if (CurrentMovingBase)
	{
		MovementBaseUtility::AddTickDependency(PrimaryComponentTick, CurrentMovingBase);
	}	
	/* Move iteration with base velocity, don't add it to our velocity just move via its delta */
	if (!TmpVelFromCurrentBase.IsZero())
	{
		// Don't sweep because the base may have moved inside the pawn
		MoveUpdatedComponent(TmpVelFromCurrentBase * DeltaTime, ComputeBaseRotation(CurrentMovingBase, DeltaTime), false);
		ProbeGround(MINIMUM_GROUND_PROBING_DISTANCE + FMath::Max(UpdatedPrimitive->GetCollisionShape().GetCapsuleRadius(), MaxStepHeight), GroundingStatus);
		/* REGISTER DEBUG FIELD */
		DebugSimulationState.bIsMovingWithBase = true;
		DebugSimulationState.MovingBaseVelocity = TmpVelFromCurrentBase;
	}

}

FVector UCustomMovementComponent::ComputeBaseVelocity(UPrimitiveComponent* MovementBase) const
{
	/* We assume, if here, we've already checked the validity of the moving base */
	
	if (!MovementBase) return FVector{0}; // Null base

	/*===== Linear Velocity =====*/
	FVector BaseVelocity;

	if (const auto PawnMovementBase = Cast<APawn>(MovementBase->GetOwner()))
	{
		// Current moving base is another pawn
		BaseVelocity = PawnMovementBase->GetVelocity();
	}
	else
	{
		// Current moving base is some form of geometry
		const auto Owner = MovementBase->GetOwner();

		// Check if component has a velocity
		BaseVelocity = MovementBase->GetComponentVelocity();
		if (!BaseVelocity.IsZero()) return BaseVelocity;

		// Check if owners root component has a velocity
		if (Owner)
		{
			BaseVelocity = Owner->GetVelocity();
			if (!BaseVelocity.IsZero()) return BaseVelocity;
		}

		// Check if component has a physics velocity
		if (const auto BodyInstance = MovementBase->GetBodyInstance())
		{
			BaseVelocity = BodyInstance->GetUnrealWorldVelocity();

			/*===== Tangent Velocity =====*/
			const FVector AngularVelocity = BodyInstance->GetUnrealWorldAngularVelocityInRadians();
			if (!AngularVelocity.IsNearlyZero())
			{
				const FVector Location = MovementBase->GetComponentLocation();
				const FVector PawnLowerBound = UpdatedComponent->GetComponentLocation() - UpdatedComponent->GetUpVector() * UpdatedPrimitive->GetCollisionShape().GetCapsuleHalfHeight();
				const FVector RadialDistanceToComponent = PawnLowerBound - Location;
				const FVector TangentialVelocity = AngularVelocity ^ RadialDistanceToComponent;
				BaseVelocity += TangentialVelocity;
			}
		}
	}
	
	return BaseVelocity;
}

FQuat UCustomMovementComponent::ComputeBaseRotation(UPrimitiveComponent* MovementBase, float DeltaTime) const
{
	const FQuat PlayerRotation = UpdatedComponent->GetComponentQuat();
	
	if (!MovementBase) return PlayerRotation;
	
	if (const auto BodyInstance = MovementBase->GetBodyInstance())
	{
		const FVector AngularVelocity = BodyInstance->GetUnrealWorldAngularVelocityInRadians();

		if (!AngularVelocity.IsNearlyZero())
		{
			const FQuat DeltaRot = FQuat::MakeFromEuler(FMath::RadiansToDegrees(AngularVelocity * DeltaTime));
			const FVector NewForward = FVector::VectorPlaneProject(DeltaRot * UpdatedComponent->GetForwardVector(), UpdatedComponent->GetUpVector()).GetSafeNormal();
			const FQuat TargetRotation = UKismetMathLibrary::MakeRotFromXZ(NewForward, UpdatedComponent->GetUpVector()).Quaternion();
			return TargetRotation;
		}
	}
	return PlayerRotation;
}



bool UCustomMovementComponent::IsValidMovementBase(UPrimitiveComponent* MovementBase) const
{
	return MovementBase && MovementBase->Mobility == EComponentMobility::Movable;
}

bool UCustomMovementComponent::ShouldImpartVelocityFromBase(UPrimitiveComponent* MovementBase) const
{
	return bMoveWithBase && IsValidMovementBase(MovementBase) && !MovementBase->IsSimulatingPhysics();
}


#pragma endregion Moving Base 

#pragma region Animation Interface

void UCustomMovementComponent::TickPose(float DeltaTime)
{
	UAnimMontage* RootMotionMontage = nullptr;
	float RootMotionMontagePosition = -1.f;
	if (const auto RootMotionMontageInstance = GetRootMotionMontageInstance())
	{
		RootMotionMontage = RootMotionMontageInstance->Montage;
		RootMotionMontagePosition = RootMotionMontageInstance->GetPosition();
	}

	/* REGISTER DEBUG FIELD */
	DebugSimulationState.MontagePosition = RootMotionMontagePosition;
	
	const bool bWasPlayingRootMotion = IsPlayingRootMotion();
	SkeletalMesh->TickPose(DeltaTime, true);
	const bool bIsPlayingRootMotion = IsPlayingRootMotion();

	if (bWasPlayingRootMotion || bIsPlayingRootMotion)
	{
		RootMotionParams = SkeletalMesh->ConsumeRootMotion();
	}

	if (RootMotionParams.bHasRootMotion)
	{
		if (ShouldDiscardRootMotion(RootMotionMontage, RootMotionMontagePosition))
		{
			RootMotionParams = FRootMotionMovementParams();
			return;
		}
		bHasAnimRootMotion = true;

		// Scale root motion translation by user value
		RootMotionParams.ScaleRootMotionTranslation(GetAnimRootMotionTranslationScale());
		// Save root motion transform in world space
		RootMotionParams.Set(SkeletalMesh->ConvertLocalRootMotionToWorld(RootMotionParams.GetRootMotionTransform()));
		// Calculate the root motion velocity from world space root motion translation
		CalculateAnimRootMotionVelocity(DeltaTime);
		// Apply the root motion rotation now. Translation is applied in the next tick from calculated velocity
		ApplyAnimRootMotionRotation(DeltaTime);
	}
}

void UCustomMovementComponent::BlockSkeletalMeshPoseTick() const
{
	if (!SkeletalMesh) return;
	SkeletalMesh->bIsAutonomousTickPose = false;
	SkeletalMesh->bOnlyAllowAutonomousTickPose = true;
}

void UCustomMovementComponent::ApplyAnimRootMotionRotation(float DeltaTime)
{
	
}

void UCustomMovementComponent::CalculateAnimRootMotionVelocity(float DeltaTime)
{
	FVector RootMotionDelta = RootMotionParams.GetRootMotionTransform().GetTranslation();

	// Ignore components with very small delta values
	RootMotionDelta.X = FMath::IsNearlyEqual(RootMotionDelta.X, 0.f, 0.01f) ? 0.f : RootMotionDelta.X;
	RootMotionDelta.Y = FMath::IsNearlyEqual(RootMotionDelta.Y, 0.f, 0.01f) ? 0.f : RootMotionDelta.Y;
	RootMotionDelta.Z = FMath::IsNearlyEqual(RootMotionDelta.Z, 0.f, 0.01f) ? 0.f : RootMotionDelta.Z;

	const FVector RootMotionVelocity = RootMotionDelta / DeltaTime;
	/* REGISTER DEBUG FIELD */
	DebugSimulationState.RootMotionVelocity = RootMotionVelocity;
	SetVelocity(PostProcessAnimRootMotionVelocity(RootMotionVelocity, DeltaTime));
}

bool UCustomMovementComponent::ShouldDiscardRootMotion(UAnimMontage* RootMotionMontage, float RootMotionMontagePosition) const
{
	if (!RootMotionMontage || RootMotionMontagePosition < 0.f)
	{
		return true;
	}

	const float MontageLength = RootMotionMontage->GetPlayLength();
	if (RootMotionMontagePosition >= MontageLength)
	{
		return true;
	}

	if (!bApplyRootMotionDuringBlendIn)
	{
		if (RootMotionMontagePosition <= RootMotionMontage->BlendIn.GetBlendTime())
		{
			return true;
		}
	}

	if (!bApplyRootMotionDuringBlendOut)
	{
		if (RootMotionMontagePosition >= MontageLength - RootMotionMontage->BlendOut.GetBlendTime())
		{
			return true;
		}
	}

	return false;
}


void UCustomMovementComponent::SetSkeletalMeshReference(USkeletalMeshComponent* Mesh)
{
	if (SkeletalMesh && SkeletalMesh != Mesh)
	{
		SkeletalMesh->RemoveTickPrerequisiteComponent(this);
	}

	SkeletalMesh = Mesh;

	if (SkeletalMesh)
	{
		SkeletalMesh->AddTickPrerequisiteComponent(this);
	}
}


FAnimMontageInstance* UCustomMovementComponent::GetRootMotionMontageInstance() const
{
	if (!SkeletalMesh) return nullptr;

	
	const auto AnimInstance = SkeletalMesh->GetAnimInstance();
	if (!AnimInstance) return nullptr;
	
	return AnimInstance->GetRootMotionMontageInstance();
}

bool UCustomMovementComponent::IsPlayingMontage() const
{
	if (!SkeletalMesh) return false;

	const auto AnimInstance = SkeletalMesh->GetAnimInstance();
	if (!AnimInstance) return false;

	return AnimInstance->MontageInstances.Num() > 0;
}

bool UCustomMovementComponent::IsPlayingRootMotion() const
{
	if (SkeletalMesh)
	{
		return SkeletalMesh->IsPlayingRootMotion();
	}
	return false;
}

#pragma endregion Animation Interface

#pragma region Physics Interactions

void UCustomMovementComponent::RootCollisionTouched(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	/* Ensure the component we've touched is a physics body */
	if (!OtherComponent || !OtherComponent->IsAnySimulatingPhysics())
	{
		return;
	}

	/* Retrieve skinned mesh bone name if it exists (e.g interacting with ragdolls) */
	FName BoneName = NAME_None;
	if (OtherBodyIndex != INDEX_NONE)
	{
		const auto OtherSkinnedMeshComponent = Cast<USkinnedMeshComponent>(OtherComponent);
		BoneName = OtherSkinnedMeshComponent->GetBoneName(OtherBodyIndex);
	}

	/* Scale the impact force to body mass if applicable */
	float TouchForceFactorModified = TouchForceScale;
	if (bScaleTouchForceToMass)
	{
		const auto BodyInstance = OtherComponent->GetBodyInstance(BoneName);
		TouchForceFactorModified *= BodyInstance ? BodyInstance->GetBodyMass() : 1.f;
	}

	/* Clamp the impulse strength determined by our velocity and touch force modifier */
	float ImpulseStrength = FMath::Clamp(GetVelocity().Size() * TouchForceFactorModified,
										MinTouchForce > 0.f ? MinTouchForce : -FLT_MAX,
										MaxTouchForce > 0.f ? MaxTouchForce : FLT_MAX);

	const FVector OtherComponentLocation = OtherComponent->GetComponentLocation();
	const FVector ShapeLocation = UpdatedComponent->GetComponentLocation();

	// TODO: It's fine keeping impulse direction planar right?
	FVector ImpulseDirection = FVector(OtherComponentLocation.X - ShapeLocation.X, OtherComponentLocation.Y - ShapeLocation.Y, 0.25f).GetSafeNormal();
	ImpulseDirection = (ImpulseDirection + GetVelocity().GetSafeNormal()) * 0.5f;
	ImpulseDirection.Normalize();

	/* Apply the touch force on retrieved bone if applicable */
	OtherComponent->AddImpulse(ImpulseDirection * ImpulseStrength, BoneName);
	LOG_SCREEN(-1, 2.f, FColor::Purple, "Impulse Strength = " + FString::SanitizeFloat(ImpulseStrength));
}

#pragma endregion Physics Interactions