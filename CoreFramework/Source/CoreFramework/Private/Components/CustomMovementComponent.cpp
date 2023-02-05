// Copyright 2023 Abdulrahmen Almodaimegh. All Rights Reserved.
#include "CustomMovementComponent.h"
#include "CFW_PCH.h"
#include "GameFramework/Character.h"


#pragma region Profiling & CVars
/* Core Update Loop */
DECLARE_CYCLE_STAT(TEXT("Tick Component"), STAT_TickComponent, STATGROUP_RadicalMovementComp)
DECLARE_CYCLE_STAT(TEXT("On Movement Mode Changed"), STAT_OnMovementModeChanged, STATGROUP_RadicalMovementComp)
DECLARE_CYCLE_STAT(TEXT("Perform Movement"), STAT_PerformMovement, STATGROUP_RadicalMovementComp)
DECLARE_CYCLE_STAT(TEXT("Pre Movement Update"), STAT_PreMovementUpdate, STATGROUP_RadicalMovementComp)
DECLARE_CYCLE_STAT(TEXT("Movement Update"), STAT_MovementUpdate, STATGROUP_RadicalMovementComp)

/* Movement Tick */
DECLARE_CYCLE_STAT(TEXT("Slide Along Surface"), STAT_SlideAlongSurface, STATGROUP_RadicalMovementComp)
DECLARE_CYCLE_STAT(TEXT("Two Wall Adjust"), STAT_TwoWallAdjust, STATGROUP_RadicalMovementComp)
DECLARE_CYCLE_STAT(TEXT("Handle Impact"), STAT_HandleImpact, STATGROUP_RadicalMovementComp)
DECLARE_CYCLE_STAT(TEXT("Step Up"), STAT_StepUp, STATGROUP_RadicalMovementComp)

/* Hit Queries */
DECLARE_CYCLE_STAT(TEXT("Resolve Penetration"), STAT_ResolvePenetration, STATGROUP_RadicalMovementComp)
DECLARE_CYCLE_STAT(TEXT("Probe Ground"), STAT_ProbeGround, STATGROUP_RadicalMovementComp)
DECLARE_CYCLE_STAT(TEXT("Ground Sweep"), STAT_GroundSweep, STATGROUP_RadicalMovementComp)
DECLARE_CYCLE_STAT(TEXT("Line Casts"), STAT_LineCasts, STATGROUP_RadicalMovementComp )
DECLARE_CYCLE_STAT(TEXT("Evaluate Hit Stability"), STAT_EvalHitStability, STATGROUP_RadicalMovementComp)

/* Features */
DECLARE_CYCLE_STAT(TEXT("Tick Pose"), STAT_TickPose, STATGROUP_RadicalMovementComp)
DECLARE_CYCLE_STAT(TEXT("Move With Base"), STAT_MoveWithBase, STATGROUP_RadicalMovementComp)
DECLARE_CYCLE_STAT(TEXT("Physics Interaction"), STAT_PhysicsInteraction, STATGROUP_RadicalMovementComp)

namespace RMCCVars
{
#if ALLOW_CONSOLE && !NO_LOGGING
	int32 ShowMovementVectors = 0;
	FAutoConsoleVariableRef CVarShowMovementVectors
	(
		TEXT("rmc.ShowMovementVectors"),
		ShowMovementVectors,
		TEXT("Visualize velocity and acceleration vectors. 0: Disable, 1: Enable"),
		ECVF_Default
	);
	
	int32 StatMovementValues = 0;
	FAutoConsoleVariableRef CVarStatMovementValues
	(
		TEXT("rmc.StatMovementValues"),
		StatMovementValues,
		TEXT("Display Realtime motion values from the movement component. 0: Disable, 1: Enable"),
		ECVF_Default
	);

	int32 LogMovementValues = 0;
	FAutoConsoleVariableRef CVarLogMovementValues
	(
		TEXT("rmc.LogMovementValues"),
		LogMovementValues,
		TEXT("Log Realtime motion values from the movement component. 0: Disable, 1: Enable"),
		ECVF_Default
	);

	int32 ShowGroundSweep = 0;
	FAutoConsoleVariableRef CVarShowGroundSweep
	(
		TEXT("rmc.ShowGroundSweep"),
		ShowGroundSweep,
		TEXT("Visualize the result of the ground sweep. 0: Disable, 1: Enable"),
		ECVF_Default
	);
#endif 
}

#pragma endregion Profiling & CVars

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
void UCustomMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	SCOPE_CYCLE_COUNTER(STAT_TickComponent)

	const FVector InputVector = ConsumeInputVector();

	if (ShouldSkipUpdate(DeltaTime))
	{
		return;
	}

	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	/* Don't update if simulating physics (e.g ragdolls) */
	if (UpdatedComponent->IsSimulatingPhysics())
	{
		/* This is from GMC but idk if its necessary since we're setting root component vel in Perform Movement*/
		if (const auto BodyInstance = UpdatedPrimitive->GetBodyInstance())
		{
			BodyInstance->SetLinearVelocity(GetVelocity(), false);
		}
		ClearAccumulatedForces(); // Maybe
		return;
	}

	/* Update avoidance parameter */
	AvoidanceLockTimer -= DeltaTime;

	/* Perform Move */
	PerformMovement(DeltaTime);

	/* Avoidance update post move */
	if (bUseRVOAvoidance)
	{
		UpdateDefaultAvoidance();
	}

	/* Physics interactions updates */
	if (bEnablePhysicsInteraction)
	{
		SCOPE_CYCLE_COUNTER(STAT_PhysicsInteraction)
		ApplyDownwardForce(DeltaTime);
		ApplyRepulsionForce(DeltaTime);
	}

	/* Nice extra debug visualizer should add eventually from CMC */
	
}


#pragma region Core Update Loop

void UCustomMovementComponent::PerformMovement(float DeltaTime)
{
	SCOPE_CYCLE_COUNTER(STAT_PerformMovement)
	
	// Setup movement, and do not progress if setup fails
	if (!PreMovementUpdate(DeltaTime))
	{
		return;
	}

	FVector OldVelocity;
	FVector OldLocation;
	
	// Internal Character Move - looking at CMC, it applies UpdateVelocity, RootMotion, etc... before the character move...
	{
		FScopedMovementUpdate ScopedMovementUpdate(UpdatedComponent, bEnableScopedMovementUpdates ? EScopedUpdate::DeferredUpdates : EScopedUpdate::ImmediateUpdates);
		
		/* Update Based Movement */
		TryUpdateBasedMovement(DeltaTime);

		// Apply Root Motion (Prepare then apply)

		/* Cache previous values for processing later */
		OldVelocity = Velocity;
		OldLocation = UpdatedComponent->GetComponentLocation();
		
		/* Trigger gameplay event for velocity modification & apply pending impulses and forces*/
		ApplyAccumulatedForces(DeltaTime); // TODO: Here's where we'd also check for bMustUnground
		HandlePendingLaunch(); // TODO: This would auto-unground
		ClearAccumulatedForces();
		
		if (CurrentFloor.bWalkableFloor)
		{
			const float VelMag = Velocity.Size();
			SetVelocity(GetDirectionTangentToSurface(Velocity, CurrentFloor.HitResult.Normal).GetSafeNormal() * VelMag);
		}
		UpdateVelocity(Velocity, DeltaTime);

		// Might also want to make a flag whether we have a skeletal mesh or not in InitializeComponent or something such that we can keep this whole thing general
		/* Apply root motion after any direct velocity modifications have been made because RM would override it */
		if (const bool bIsPlayingRootMotion = static_cast<bool>(GetRootMotionMontageInstance()))
		{
			// Will update vel & rot based on root motion
			TickPose(DeltaTime);

			// TODO: There's a bit more we can do here...

			/* Trigger event to allow for manual adjustment of root motion velocity & rotation */
			if (HasAnimRootMotion())
			{
				PostProcessAnimRootMotionVelocity(Velocity, DeltaTime);
			}
		}

		/* Perform actual move */
		StartMovementTick(DeltaTime, 0);

		if (!HasAnimRootMotion())
		{
			UpdateRotation(TargetRot, DeltaTime);
		}

		/* Consume path following requested velocity */
		LastUpdateRequestedVelocity = bHasRequestedVelocity ? RequestedVelocity : FVector::ZeroVector;
		bHasRequestedVelocity = false;
	}
	
	PostMovementUpdate(DeltaTime);
}

bool UCustomMovementComponent::PreMovementUpdate(float DeltaTime)
{
	SCOPE_CYCLE_COUNTER(STAT_PreMovementUpdate)

	const UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return false;
	}

	bTeleportedSinceLastUpdate = UpdatedComponent->GetComponentLocation() != LastUpdateLocation;
	
	if (!CanMove() || UpdatedComponent->IsSimulatingPhysics())
	{
		/* Consume root motion */

		/* Clear pending physics forces*/

		return false;
	}

	/* Setup */
	bForceNextFloorCHeck |= (IsMovingOnGround() && bTeleportedSinceLastUpdate);
	
	return true;
}

#pragma region Actual Movement Ticks

void UCustomMovementComponent::StartMovementTick(float DeltaTime, uint32 Iterations)
{
	if ((DeltaTime < MIN_TICK_TIME) || (Iterations >= MaxSimulationIterations))
	{
		return;
	}

	if (UpdatedComponent->IsSimulatingPhysics())
	{
		return;
	}

	if (CurrentFloor.bWalkableFloor)
	{
		GroundMovementTick(DeltaTime, Iterations);
	}
	else
	{
		AirMovementTick(DeltaTime, Iterations);
	}
}


void UCustomMovementComponent::GroundMovementTick(float DeltaTime, uint32 Iterations)
{
	SCOPE_CYCLE_COUNTER(STAT_MovementUpdate)

	/* Validate everything before doing anything */
	if (DeltaTime < MIN_TICK_TIME)
	{
		return;
	}

	if (!UpdatedComponent->IsQueryCollisionEnabled())
	{
		// Here they set movement mode to walking? In the physwalking method???
		return;
	}

	bJustTeleported = false;
	bool bCheckedFall = false;
	bool bTriedLedgeMove = false;
	float RemainingTime = DeltaTime;
	

	while ((RemainingTime >= MIN_TICK_TIME) && (Iterations < MaxSimulationIterations))
	{
		/* Setup current move iteration */
		Iterations++;
		bJustTeleported = false;
		const float IterTick = GetSimulationTimeStep(RemainingTime, Iterations);

		/* Cache current values */
		UPrimitiveComponent* const OldBase = GetMovementBase();
		const FVector PreviousBaseLocation = (OldBase != NULL) ? OldBase->GetComponentLocation() : FVector::ZeroVector;
		const FVector OldLocation = UpdatedComponent->GetComponentLocation();
		const FGroundingStatus OldFloor = CurrentFloor;

		/* Rootmotion and gameplay stuff */

		/* Compute move parameters */
		const FVector MoveVelocity = Velocity;
		const FVector Delta = MoveVelocity * IterTick;
		const bool bZeroDelta = Delta.IsNearlyZero();
		FStepDownResult StepDownResult;
		
		/* Exit if the delta is zero, no movement should happen anyways */
		if (bZeroDelta)
		{
			RemainingTime = 0.f;
		} // Zero delta
		else
		{
			MoveAlongFloor(MoveVelocity, IterTick, &StepDownResult);

			/* Worth keeping this here if we wanna put the CalcVelocity event after MoveIteration */
			if (IsFalling())
			{
				StartMovementTick(RemainingTime, Iterations);
				return;
			}
		} // Non-zero delta

		/* Update Floor */
		if (StepDownResult.bComputedFloor)
		{
			CurrentFloor = StepDownResult.FloorResult;
		}
		else
		{
			FindFloor(UpdatedComponent->GetComponentLocation(), CurrentFloor, bZeroDelta, nullptr);
		}

		/* Check for ledges here */
		const bool bCheckLedges = !CanWalkOffLedges();
		if (bCheckLedges && !CurrentFloor.IsWalkableFloor())
		{
			// Calculate possible alternate movement
			
		} // Check for ledge movement, preventing walking off ledges
		else
		{
			/* Validate the floor check */
			if (CurrentFloor.IsWalkableFloor())
			{
				/* Check denivelation angle */
				if (ShouldCatchAir(OldFloor, CurrentFloor))
				{
					// TODO: Events and swtiching to falling
					HandleWalkingOffLedge();
					if (IsMovingOnGround())
					{
						StartFalling();
					}
					return;
				}

				/* Otherwise snap to ground */
				AdjustFloorHeight();
				SetBase(CurrentFloor.HitResult.Component.Get(), CurrentFloor.HitResult.BoneName);
			} // Current floor is walkable
			else if (CurrentFloor.HitResult.bStartPenetrating && RemainingTime <= 0.f)
			{
				// The floor check failed because it started in penetration, we do not want to try to move downward
				// because the downward sweep failed. Try to pop out the floor instead.
				FHitResult Hit(CurrentFloor.HitResult);
				Hit.TraceEnd = Hit.TraceStart + UpdatedComponent->GetUpVector() * MAX_FLOOR_DIST;
				const FVector RequestedAdjustment = GetPenetrationAdjustment(Hit);
				ResolvePenetration(RequestedAdjustment, Hit, UpdatedComponent->GetComponentQuat());
				bForceNextFloorCheck = true; // Force us to sweep for a floor next time
				
			} // Floor penetration or consumed all time

			/* Check if we need to start falling */
			if (!CurrentFloor.IsWalkableFloor() && !CurrentFloor.HitResult.bStartPenetrating)
			{
				
			}
			
		} // Don't check for ledges

		/* Allow overlap events to change physics state and velocity */
		if (IsMovingOnGround())
		{
			/* Make velocity reflect actual move */
			if (!bJustTeleported && !HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity() && IterTick >= MIN_TICK_TIME)
			{
				Velocity = (UpdatedComponent->GetComponentLocation() - OldLocation) / IterTick;
				MaintainHorizontalGroundVelocity();
			}
		}

		/* If we didn't move at all, abort */
		if (UpdatedComponent->GetComponentLocation() == OldLocation)
		{
			RemainingTime = 0.f;
			break;
		}
	}

	if (IsMovingOnGround())
	{
		MaintainHorizontalGroundVelocity(); // TODO: Would we project velocity to the ground here
	}
}


void UCustomMovementComponent::AirMovementTick(float DeltaTime, uint32 Iterations)
{
	SCOPE_CYCLE_COUNTER(STAT_MovementUpdate)

	/* Validate everything before doing anything */
	if (DeltaTime < MIN_TICK_TIME)
	{
		return;	
	}

	if (!UpdatedComponent->IsQueryCollisionEnabled())
	{
		return;
	}

	bJustTeleported = false;
	bool bCheckedFall = false;
	bool bTriedLedgeMove = false;
	float RemainingTime = DeltaTime;

	while ((RemainingTime >= MIN_TICK_TIME) && (Iterations < MaxSimulationIterations))
	{
		/* Setup current move iteration */
		Iterations++;
		bJustTeleported = false;
		const float IterTick = GetSimulationTimeStep(RemainingTime, Iterations);
		RemainingTime -= IterTick;

		/* Cache current values */
		const FVector OldLocation = UpdatedComponent->GetComponentLocation();
		const FQuat PawnRotation = UpdatedComponent->GetComponentQuat();
		const FVector OldVelocity = Velocity;

		/* Compute move parameters */
		const FVector MoveVelocity = Velocity;
		FVector Delta = MoveVelocity * IterTick;
		const bool bZeroDelta = Delta.IsNearlyZero();
		
		/* Gameplay stuff like applying gravity */

		/* Applying root motion to velocity & Decaying former base velocity */

		/* Perform the move */
		FHitResult Hit(1.f);
		SafeMoveUpdatedComponent(Delta, PawnRotation, true, Hit);

		/* Account for time of the move */
		float LastMoveTimeSlice = IterTick;
		float SubTimeTickRemaining = IterTick * (1.f - Hit.Time);

		/* Evaluate the hit */
		if (Hit.bBlockingHit)
		{
			/* Hit could be the floor, so check if we can land */
			if (IsValidLandingSpot(UpdatedComponent->GetComponentLocation(), Hit))
			{
				RemainingTime += SubTimeTickRemaining;
				ProcessLanded(Hit, RemainingTime, Iterations); // Swap to grounded move update
				return;
			} // Valid Landing Spot
			else 
			{
				/* Check if we can convert a normally invalid landing spot to a valid one */
				if (!Hit.bStartPenetrating && ShouldCheckForValidLandingSpot(Hit))
				{
					const FVector PawnLocation = UpdatedComponent->GetComponentLocation();
					FGroundingStatus FloorResult;
					FindFloor(PawnLocation, FloorResult, false);
					if (FloorResult.IsWalkableFloor() && IsValidLandingSpot(PawnLocation, FloorResult.HitResult))
					{
						RemainingTime += SubTimeTickRemaining;
						ProcessLanded(Hit, RemainingTime, Iterations); // Swap to grounded move update
						return;
					}
				}

				/* Hit could not be interpreted as a floor, adjust accordingly */
				HandleImpact(Hit, LastMoveTimeSlice, Delta);

				/* Adjust delta based on hit */
				const FVector OldHitNormal = Hit.Normal;
				const FVector OldHitImpactNormal = Hit.ImpactNormal;
				FVector AdjustedDelta = ComputeSlideVector(Delta, 1.f - Hit.Time, OldHitNormal, Hit);

				/* Compute the velocity after the deflection */
				const UPrimitiveComponent* HitComponent = Hit.GetComponent();
				// TODO: Stuff here regarding recalculating the velocity based on the delta and managing it w/ root motion or a moving base

				/* Perform a move in the deflected direction and solve accordingly */
				if (SubTimeTickRemaining > UE_KINDA_SMALL_NUMBER && (AdjustedDelta | Delta) > 0.f)
				{
					SafeMoveUpdatedComponent(AdjustedDelta, PawnRotation, true, Hit);

					/* Hit a second wall */
					if (Hit.bBlockingHit)
					{
						LastMoveTimeSlice = SubTimeTickRemaining;
						SubTimeTickRemaining *= (1.f - Hit.Time);

						if (IsValidLandingSpot(UpdatedComponent->GetComponentLocation(), Hit))
						{
							RemainingTime += SubTimeTickRemaining;
							ProcessLanded(Hit, RemainingTime, Iterations); // Swap to grounded move update
							return;
						} // Valid Landing Spot

						HandleImpact(Hit, LastMoveTimeSlice, Delta);

						/* Compute new deflection using old velocity */
						if (CONDITION)
						{
							const FVector LastMoveDelta = OldVelocity * LastMoveTimeSlice;
							AdjustedDelta = ComputeSlideVector(LastMoveDelta, 1.f, OldHitNormal, Hit);
						}

						FVector PreTwoWallDelta = AdjustedDelta;
						TwoWallAdjust(AdjustedDelta, Hit, OldHitNormal);

						/* Confused as to how to incorporate the stuff w/ the bHasLimitedAirControl condition */
						// TODO: Figure that out, skipping it for now

						/* Compute velocity after deflection */
						if (SubTimeTickRemaining > UE_KINDA_SMALL_NUMBER && !bJustTeleported)
						{
							const FVector NewVelocity = (AdjustedDelta / SubTimeTickRemaining);
							/* Might move this to an event for PostProcessRootMotion Velocity because we don't want to make assumptions */
							Velocity = HasAnimRootMotion() ? FVector::VectorPlaneProject(Velocity, PawnRotation.GetUpVector()) + NewVelocity.ProjectOnToNormal(PawnRotation.GetUpVector()): NewVelocity;
						}

						/* bDitch = true means the pawn is straddling between two slopes neither of which it can stand on */
						bool bDitch = ((MATH));
						SafeMoveUpdatedComponent(AdjustedDelta, PawnRotation, true, Hit);
						if (Hit.Time == 0.f)
						{
							/* If we're stuck, try to side step */
							FVector SideDelta = (MATH);
							if (SideDelta.IsNearlyZero())
							{
								SideDelta = FVector(MATH);
							}
							SafeMoveUpdatedComponent(SideDelta, PawnRotation, true, Hit);
						}

						bool noBToHoldMeBack = true;

						if (bDitch || IsValidLandingSpot(UpdatedComponent->GetComponentLocation(), Hit) || Hit.Time == 0.f)
						{
							RemainingTime = 0.f;
							ProcessLanded(Hit, RemainingTime, Iterations);
							return;
						}
						/* Final check is for a very rare situation, worth skipping for now */
					}
				}

				
			} // Not a landing spot, solve for hit adjustment
		}
	}
}


void UCustomMovementComponent::MoveAlongFloor(const FVector& InVelocity, float DeltaTime, FStepDownResult* OutStepDownResult)
{
	// TODO: Again, check if we want to separate this. Following CMC closely for now then will revamp accordingly after a stable foundation is set 
	if (!CurrentFloor.IsWalkableFloor())
	{
		return;
	}

	/* Project Velocity Along Floor? */
	MaintainHorizontalGroundVelocity();
	
	/* Move along the current Delta */
	FHitResult Hit(1.f);
	FVector Delta = InVelocity * DeltaTime; // TODO: Maybe project this to the player plane (passed to SlideAlongSurface)
	FVector ProjectedDelta; // TODO: And use this for 3D base
	SafeMoveUpdatedComponent(Delta, UpdatedComponent->GetComponentQuat(), true, Hit);
	float LastMoveTimeSlice = DeltaTime;

	/* Check the hit result of the move and respond appropriately */
	if (Hit.bStartPenetrating)
	{
		// Allow this hit to be used as an impact we can deflect off, otherwise we do nothing the rest of the update and appear to hitch
		HandleImpact(Hit);
		SlideAlongSurface(Delta, 1.f, Hit.Normal, Hit, true);

		/* If still in penetration, we're stuck */
		if (Hit.bStartPenetrating)
		{
			bStuckInGeometry = true;
		}
	} // Was in penetration
	else if (Hit.IsValidBlockingHit())
	{
		float PercentTimeApplied = Hit.Time;
		/* Could be a walkable floor or ramp, unsure if that would be the case because we prep the velocity for this beforehand */
		
		/* Impacted a barrier, check if we can use it as stairs */
		if (CanStepUp(Hit) || IS_MOVEMENT_BASE)
		{
			const FVector PreStepUpLocation = UpdatedComponent->GetComponentLocation();
			if (!StepUp(Hit, Delta * (1.f - PercentTimeApplied), OutStepDownResult))
			{
				/* Step up failed, handle impact */
				HandleImpact(Hit, LastMoveTimeSlice, Delta);
				SlideAlongSurface(Delta, 1.f - PercentTimeApplied, Hit.Normal, Hit, true);
			} // Step up failed
			else
			{
				
			} // Step up succeeded
			
		} // Can step up
		else if (Hit.Component.IsValid() && !Hit.Component.Get()->CanCharacterStepUp(GetPawnOwner()))
		{
			HandleImpact(Hit, LastMoveTimeSlice, Delta);
			SlideAlongSurface(Delta, 1.f - PercentTimeApplied, Hit.Normal, Hit, true);
		} // Can't step up
		
	} // Was valid blocking hit
}

#pragma endregion Actual Movement Ticks

void UCustomMovementComponent::PostMovementUpdate(float DeltaTime)
{
	SaveMovementBaseLocation();
	
	UpdateComponentVelocity();
	
	LastUpdateLocation = UpdatedComponent ? UpdatedComponent->GetComponentLocation() : FVector::ZeroVector;
	LastUpdateRotation = UpdatedComponent ? UpdatedComponent->GetComponentQuat() : FQuat::Identity;
	LastUpdateVelocity = Velocity;
}

void UCustomMovementComponent::StartLanding(float DeltaTime, uint32 Iterations)
{
	if ((DeltaTime < MIN_TICK_TIME) || (Iterations >= MaxSimulationIterations))
	{
		return;
	}
	
	LandedEvent();

	PathFindingManagement();

	GroundMovementTick(DeltaTime, Iterations);
}

void UCustomMovementComponent::StartFalling(float DeltaTime, uint32 Iterations)
{
	if ((DeltaTime < MIN_TICK_TIME) || (Iterations >= MaxSimulationIterations))
	{
		return;
	}
	
	AdjustRemainingTime();

	PIESpecificThing();

	StartMovementTick(DeltaTime, Iterations);
}


#pragma endregion Core Update Loop

#pragma region Stability Evaluations

/* FUNCTIONAL */
bool UCustomMovementComponent::CanMove() const
{
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



/* ===== Steps ===== */

bool UCustomMovementComponent::StepUp(const FHitResult& StepHit, const FVector& Delta, FStepDownResult* OutStepDownResult)
{
	SCOPE_CYCLE_COUNTER(STAT_StepUp)
	
	/* Determine our planar move delta for this iteration */
	const FVector PawnUp = UpdatedComponent->GetUpVector();
	const FVector StepLocationDelta = FVector::VectorPlaneProject(Delta, PawnUp);

	/* Skip negligible deltas or if step height is 0 (or invalid) */
	if (MaxStepHeight <= 0.f || StepLocationDelta.IsNearlyZero())
	{
		//LOG_SCREEN(-1, 1.f, FColor::Red, "Aborting, Step Delta Near Zero...");
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

	if (IsWalkable(StepHit) && !MustUnground())
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

bool UCustomMovementComponent::CanStepUp(const FHitResult& StepHit) const
{
	if (!StepHit.IsValidBlockingHit())
	{
		return false;
	}

	const UPrimitiveComponent* HitComponent = StepHit.Component.Get();
	if (!HitComponent)
	{
		return true;
	}

	if (!HitComponent->CanCharacterStepUp(GetPawnOwner()))
	{
		return false;
	}

	if (!StepHit.HitObjectHandle.IsValid())
	{
		return true;
	}

	const AActor* HitActor = StepHit.HitObjectHandle.GetManagingActor();
	if (!HitActor->CanBeBaseForCharacter(GetPawnOwner()))
	{
		return false;
	}

	return true;
}

#pragma endregion Stability Evaluations

#pragma region Collision Adjustments

void UCustomMovementComponent::HandleImpact(const FHitResult& Hit, float TimeSlice, const FVector& MoveDelta)
{
	SCOPE_CYCLE_COUNTER(STAT_HandleImpact)
	
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

FVector UCustomMovementComponent::GetPenetrationAdjustment(const FHitResult& Hit) const
{
	FVector Result = Super::GetPenetrationAdjustment(Hit);

	const float MaxDepenetrationWithGeometry = 500.f;
	const float MaxDepenetrationWithPawn = 100.f;
	
	float MaxDistance = MaxDepenetrationWithGeometry;
	if (Hit.HitObjectHandle.DoesRepresentClass(APawn::StaticClass()))
	{
		MaxDistance = MaxDepenetrationWithPawn;
	}

	Result = Result.GetClampedToMaxSize(MaxDistance);

	return Result;
}

bool UCustomMovementComponent::ResolvePenetrationImpl(const FVector& Adjustment, const FHitResult& Hit, const FQuat& NewRotation)
{
	// If movement occurs, mark that we teleported so we don't incorrectly adjust velocity based on potentially very different movement than ours
	//bJustTeleported |= Super::ResolvePenetrationImpl(Adjustment, Hit, NewRotation);
	//return bJustTeleported;
	return Super::ResolvePenetrationImpl(Adjustment, Hit, NewRotation);
}


float UCustomMovementComponent::SlideAlongSurface(const FVector& Delta, float Time, const FVector& Normal, FHitResult& Hit, bool bHandleImpact)
{
	SCOPE_CYCLE_COUNTER(STAT_SlideAlongSurface)
	
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
	SCOPE_CYCLE_COUNTER(STAT_TwoWallAdjust)
	
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


#pragma endregion Collision Checks


#pragma region Moving Base


#pragma endregion Moving Base 

#pragma region Animation Interface

void UCustomMovementComponent::TickPose(float DeltaTime)
{
	SCOPE_CYCLE_COUNTER(STAT_TickPose)
	
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
	if (!bEnablePhysicsInteraction)
	{
		return;
	}
	
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

#pragma region CMC Copies

// TODO
bool UCustomMovementComponent::ShouldCatchAir(const FGroundingStatus& OldFloor, const FGroundingStatus& NewFloor)
{
}


// DONE
void UCustomMovementComponent::UpdateFloorFromAdjustment()
{
	if (CurrentFloor.bWalkableFloor)
	{
		FindFloor(UpdatedComponent->GetComponentLocation(), CurrentFloor, false);
	}
	else
	{
		CurrentFloor.Clear();
	}
}


// DONE
void UCustomMovementComponent::AdjustFloorHeight()
{
	// If we have a floor check that hasn't hit anything, don't adjust height.
	if (!CurrentFloor.IsWalkableFloor())
	{
		return;
	}

	float OldFloorDist = CurrentFloor.FloorDist;
	if (CurrentFloor.bLineTrace)
	{
		/* This would cause us to scale unwalkable walls*/
		if (OldFloorDist < MIN_FLOOR_DIST && CurrentFloor.LineDist >= MIN_FLOOR_DIST)
		{
			return;
		}
		
		/* Fall back to a line trace means the sweep was unwalkable. Use the line distance for vertical adjustments */
		OldFloorDist = CurrentFloor.LineDist;
	}

	/* Move up or down to maintain consistent floor height */
	if (OldFloorDist < MIN_FLOOR_DIST || OldFloorDist > MAX_FLOOR_DIST)
	{
		FHitResult AdjustHit(1.f);
		const float InitialHeight = UpdatedComponent->GetComponentLocation() | UpdatedComponent->GetUpVector();
		const float AvgFloorDist = (MIN_FLOOR_DIST + MAX_FLOOR_DIST) * 0.5f;

		/* Move dist such that if we're above MAX, it'll move us down - below MIN, it'll move us up. Relative to the average of the two thresholds */
		const float MoveDist = AvgFloorDist - OldFloorDist;

		SafeMoveUpdatedComponent(UpdatedComponent->GetUpVector() * MoveDist, UpdatedComponent->GetComponentQuat(), true, AdjustHit);

		/* Check if the snapping resulted in a valid hit */
		if (!AdjustHit.IsValidBlockingHit())
		{
			CurrentFloor.FloorDist += MoveDist; 
		} // Invalid blocking hit, adjust our cached floor dist for next next
		else if (MoveDist > 0.f)
		{
			const float CurrentHeight = UpdatedComponent->GetComponentLocation() | UpdatedComponent->GetUpVector();
			CurrentFloor.FloorDist += CurrentHeight - InitialHeight;
		} // Below MIN_FLOOR_DIST
		else
		{
			const float CurrentHeight = UpdatedComponent->GetComponentLocation() | UpdatedComponent->GetUpVector();
			CurrentFloor.FloorDist = CurrentHeight - (AdjustHit.Location | UpdatedComponent->GetUpVector());
			if (IsWalkable(AdjustHit))
			{
				CurrentFloor.SetFromSweep(AdjustHit, CurrentFloor.FloorDist, true);
			}
		} // Above MAX_FLOOR_DIST, could be a new floor value so we set that if its walkable

		/* Don't recalculate velocity based on snapping, also avoid if we moved out of penetration */
		bJustTeleported |= (OldFloorDist < 0.f); // TODO: This could affect projection if we don't do it manually

		/* If something caused us to adjust our height (especially a depenetration), we should ensure another check next frame or we will keep a stale result */
		bForceNextFloorCheck = true;
	}
}

// DONE
bool UCustomMovementComponent::IsWithinEdgeTolerance(const FVector& CapsuleLocation, const FVector& TestImpactPoint,
	const float CapsuleRadius) const
{
	const float DistFromCenterSq = FVector::VectorPlaneProject(TestImpactPoint - CapsuleLocation, UpdatedComponent->GetUpVector()).SizeSquared();
	const float ReducedRadiusSq = FMath::Square(FMath::Max(SWEEP_EDGE_REJECT_DISTANCE + UE_KINDA_SMALL_NUMBER, CapsuleRadius - SWEEP_EDGE_REJECT_DISTANCE));
	return DistFromCenterSq < ReducedRadiusSq;
}

// DONE
void UCustomMovementComponent::ComputeFloorDist(const FVector& CapsuleLocation, float LineDistance, float SweepDistance,
	FGroundingStatus& OutFloorResult, float SweepRadius, const FHitResult* DownwardSweepResult) const
{
	OutFloorResult.Clear();

	float PawnRadius = UpdatedPrimitive->GetCollisionShape().GetCapsuleRadius();
	float PawnHalfHeight = UpdatedPrimitive->GetCollisionShape().GetCapsuleHalfHeight();
	
	bool bSkipSweep = false;
	if (DownwardSweepResult != nullptr && DownwardSweepResult->IsValidBlockingHit())
	{
		/* Accept it only if the supplied sweep was vertical and downwards relative to character orientation */
		float TraceStartHeight = DownwardSweepResult->TraceStart | UpdatedComponent->GetUpVector();
		float TraceEndHeight = DownwardSweepResult->TraceEnd | UpdatedComponent->GetUpVector();
		float TracePlaneProjection = FVector::VectorPlaneProject(DownwardSweepResult->TraceStart - DownwardSweepResult->TraceStart, UpdatedComponent->GetUpVector()).SizeSquared();

		if ((TraceStartHeight > TraceEndHeight) && (TracePlaneProjection <= UE_KINDA_SMALL_NUMBER))
		{
			/* Reject hits that are barely on the cusp of the radius of the capsule */
			if (IsWithinEdgeTolerance(DownwardSweepResult->Location, DownwardSweepResult->ImpactPoint, PawnRadius))
			{
				/* Don't try a redundant sweep, regardless of whether this sweep is usable */
				bSkipSweep = true;

				const bool bIsWalkable = IsWalkable(DownwardSweepResult);
				const float FloorDist = (CapsuleLocation - DownwardSweepResult->Location) | UpdatedComponent->GetUpVector();
				OutFloorResult.SetFromSweep(*DownwardSweepResult, FloorDist, bIsWalkable);

				if (bIsWalkable)
				{
					/* Use the supplied downward sweep as the floor hit result */
					return;
				}
			}
		}
	}

	/* Require sweep distance >= line distance, otherwise the HitResult can't be interpreted as the sweep result */
	if (SweepDistance < LineDistance)
	{
		ensure(SweepDistance >= LineDistance);
		return;
	}

	/* Prep for sweep */
	bool bBlockingHit = false;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ComputeFloorDist), false, GetPawnOwner());
	FCollisionResponseParams ResponseParam;
	InitCollisionParams(QueryParams, ResponseParam);
	const ECollisionChannel CollisionChannel = UpdatedComponent->GetCollisionObjectType();

	/* Perform sweep */
	if (!bSkipSweep && SweepDistance > 0.f && SweepRadius > 0.f)
	{
		/* Use a shorter height to avoid weeps giving weird results if we start on a surface. Allows us to also adjust out of penetrations */
		// Basically, sweeping with a shrunk capsule will let us "catch' penetrations, in this case within the last 10% of our capsule size
		const float ShrinkScale = 0.9f;
		const float ShrinkScaleOverlap = 0.1f;
		float ShrinkHeight = (PawnHalfHeight - PawnRadius) * (1.f - ShrinkScale);
		float TraceDist = SweepDistance + ShrinkHeight;
		FCollisionShape CapsuleShape = FCollisionShape::MakeCapsule(SweepRadius, PawnHalfHeight - ShrinkHeight);

		/* ~~~~~~~~~~~~~~~~~~~ */
		/* Perform Shape Trace */
		FHitResult Hit(1.f);
		bBlockingHit = FloorSweepTest(Hit, CapsuleLocation, CapsuleLocation - UpdatedComponent->GetUpVector() * TraceDist, CollisionChannel, CapsuleShape, QueryParams, ResponseParam);

		if (bBlockingHit)
		{
			/* Reject hits that are adjacent to us. We only care about hits on the bottom hemisphere of the capsule so check the 2D distance to impact point */
			if (Hit.bStartPenetrating || !IsWithinEdgeTolerance(CapsuleLocation, Hit.ImpactPoint, CapsuleShape.Capsule.Radius))
			{
				/* Use a capsule with a slightly smaller radius and shorter height to avoid the adjacent object */
				CapsuleShape.Capsule.Radius = FMath::Max(0.f, CapsuleShape.Capsule.Radius - SWEEP_EDGE_REJECT_DISTANCE - UE_KINDA_SMALL_NUMBER);
				if (!CapsuleShape.IsNearlyZero())
				{
					ShrinkHeight = (PawnHalfHeight - PawnRadius) * (1.f - ShrinkScaleOverlap);
					TraceDist = SweepDistance + ShrinkHeight;
					CapsuleShape.Capsule.HalfHeight = FMath::Max(PawnHalfHeight - ShrinkHeight, CapsuleShape.Capsule.Radius);
					Hit.Reset(1.f, false);

					bBlockingHit = FloorSweepTest(Hit, CapsuleLocation, CapsuleLocation - UpdatedComponent->GetUpVector() * TraceDist, CollisionChannel, CapsuleShape, QueryParams, ResponseParam);
				}
			}

			/* Reduce the hit distance by ShrinkHeight because we shrank the capsule for the trace */
			/* Also allow negative distances here, because this allows us to pull out of penetration */
			const float MaxPenetrationAdjust = FMath::Max(MAX_FLOOR_DIST, PawnRadius);
			const float SweepResult = FMath::Max(-MaxPenetrationAdjust, Hit.Time * TraceDist - ShrinkHeight);

			OutFloorResult.SetFromSweep(Hit, SweepResult, false);
			if (Hit.IsValidBlockingHit() && IsWalkable(Hit))
			{
				if (SweepResult <= SweepDistance)
				{
					OutFloorResult.bWalkableFloor = true;
					return;
				}
			}
		}
	}
	
	/* Since we require a longer sweep than line trace, we don't want to run the line trace if the sweep missed everything */
	/* We do however want to try a line trace if the sweep was stuck in penetration */
	if (!OutFloorResult.bBlockingHit && !OutFloorResult.HitResult.bStartPenetrating)
	{
		OutFloorResult.FloorDist = SweepDistance;
		return;
	}
	
	/* ~~~~~~~~~~~~~~~~~~~ */

	/* Perform Line Trace*/
	if (LineDistance > 0.f)
	{
		const float ShrinkHeight = PawnHalfHeight;
		const FVector LineTraceStart = CapsuleLocation;
		const float TraceDist = LineDistance + ShrinkHeight;
		const FVector Down = -UpdatedComponent->GetUpVector() * TraceDist;
		QueryParams.TraceTag = SCENE_QUERY_STAT_NAME_ONLY(FloorLineTrace);

		FHitResult Hit(1.f);
		bBlockingHit = GetWorld()->LineTraceSingleByChannel(Hit, LineTraceStart, LineTraceStart + Down, CollisionChannel, QueryParams, ResponseParam);

		if (bBlockingHit)
		{
			if (Hit.Time > 0.f)
			{
				/* Reduce the hit distance by the shrink height because we started the trace higher than the base */
				/* Also allow negative distances here, because this allows us to pull out of penetration */
				const float MaxPenetrationAdjust = FMath::Max(MAX_FLOOR_DIST, PawnRadius);
				const float LineResult = FMath::Max(-MaxPenetrationAdjust, Hit.Time * TraceDist - ShrinkHeight);

				OutFloorResult.bBlockingHit = true;
				if (LineResult <= LineDistance && IsWalkable(Hit))
				{
					OutFloorResult.SetFromLineTrace(Hit, OutFloorResult.FloorDist, LineResult, true);
					return;
				}
			}
		}
	}

	/* If we're here, that means no hits were acceptable */
	OutFloorResult.bWalkableFloor = false;
}

// DONE
void UCustomMovementComponent::FindFloor(const FVector& CapsuleLocation, FGroundingStatus& OutFloorResult,
	bool bCanUseCachedLocation, const FHitResult* DownwardSweepResult) const
{
	SCOPE_CYCLE_COUNTER(STAT_ProbeGround)

	/* If collision is not enabled, or bSolveGrounding is not set, we don't care */
	if (!bSolveGrounding || !UpdatedComponent->IsQueryCollisionEnabled())
	{
		OutFloorResult.Clear();
		return;
	}

	/* Increase height check slightly if currently groudned to prevent ground snapping height from later invalidating the floor result */
	const float HeightCheckAdjust = (IsMovingOnGround() ? MAX_FLOOR_DIST + UE_KINDA_SMALL_NUMBER : -MAX_FLOOR_DIST);

	/* Setup sweep distances */
	float FloorSweepTraceDist = FMath::Max(MAX_FLOOR_DIST, MaxStepHeight + HeightCheckAdjust);
	float FloorLineTraceDist = FloorSweepTraceDist;
	bool bNeedToValidateFloor =  true;

	/* Perform sweep */
	if (FloorLineTraceDist > 0.f || FloorSweepTraceDist > 0.f)
	{
		if (bAlwaysCheckFloor || !bCanUseCachedLocation || bForceNextFloorCheck || bJustTeleported)
		{
			ComputeFloorDist(CapsuleLocation, FloorLineTraceDist, FloorSweepTraceDist, OutFloorResult, CapsuleRadius, DownwardSweepResult);
		} // Force floor sweep
		else
		{
			/* Force Floor Check if base has collision disabled or if it does not block us*/
			UPrimitiveComponent* MovementBase = GetMovementBase();
			const AActor* BaseActor = MovementBase ? MovementBase->GetOwner() : nullptr;
			const ECollisionChannel CollisionChannel = UpdatedComponent->GetCollisionObjectType();

			if (MovementBase != nullptr)
			{
				bForceNextFloorCheck = !MovementBase->IsQueryCollisionEnabled()
				|| MovementBase->GetCollisionResponseToChannel(CollisionChannel) != ECR_BLOCK
				|| MovementBaseUtility::IsDynamicBase(MovementBase);
			}

			const bool bActorBasePendingKill = BaseActor && !IsValid(BaseActor);

			if (!bForceNextFloorCheck && !bActorBasePendingKill && MovementBase)
			{
				OutFloorResult = CurrentFloor;
				bNeedToValidateFloor = false;
			} // Don't need to perform a sweep, continue with current floor value
			else
			{
				ComputeFloorDist(CapsuleLocation, FloorLineTraceDist, FloorSweepTraceDist, OutFloorResult, CapsuleRadius, DownwardSweepResult);
			} // Explicitly perform a sweep test
		} // Check conditions before performing a floor sweep
	}

	/* Check ledge perching result on the compute floor (only if floor computed from shape trace) */
	if (bNeedToValidateFloor && OutFloorResult.bBlockingHit && !OutFloorResult.bLineTrace)
	{
		if (ShouldComputePerchResult(OutFloorResult.HitResult))
		{
			float MaxPerchFloorDist = FMath::Max(MAX_FLOOR_DIST, MaxStepHeight + HeightCheckAdjust);
			if (IsMovingOnGround())
			{
				MaxPerchFloorDist += FMath::Max(0.f, PerchAdditionalHeight);
			}

			FGroundingStatus PerchFloorResult;
			if (ComputePerchResult(GetValidPerchRadius(), OutFloorResult.HitResult, MaxPerchFloorDist, PerchFloorResult))
			{
				/* Don't allow the floor distance adjustment to push us up too high or we'll move beyond the perch distance and fall next time */
				const float AvgFloorDist = (MIN_FLOOR_DIST  + MAX_FLOOR_DIST) * 0.5f;
				const float MoveUpDist = (AvgFloorDist - OutFloorResult.FloorDist);
				if (MoveUpDist + PerchFloorResult.FloorDist >= MaxPerchFloorDist)
				{
					OutFloorResult.FloorDist = AvgFloorDist;
				}

				/* If the regular capsule is on an unwalkable surface, but the perched one would allow us to stand, override the normal with the walkable one  */
				if (!OutFloorResult.bWalkableFloor)
				{
					OutFloorResult.SetFromLineTrace(PerchFloorResult.HitResult, OutFloorResult.FloorDist, FMath::Max(OutFloorResult.FloorDist, MIN_FLOOR_DIST), true);
				}
			}
			else
			{
				/* Had no floor or an unwalkable floor and couldn't perch there. So invalidate it to move us to falling */
				OutFloorResult.bWalkableFloor = false;
			}
		}
	}
}

// DONE
bool UCustomMovementComponent::FloorSweepTest(FHitResult& OutHit, const FVector& Start, const FVector& End,
	ECollisionChannel TraceChannel, const FCollisionShape& CollisionShape, const FCollisionQueryParams& Params,
	const FCollisionResponseParams& ResponseParams) const
{
	bool bBlockingHit = false;

	if (!bUseFlatBaseForFloorChecks)
	{
		bBlockingHit = GetWorld()->SweepSingleByChannel(OutHit, Start, End, UpdatedComponent->GetComponentQuat(), TraceChannel, CollisionShape, Params, ResponseParams);
	} // Don't use flat base for sweep
	else
	{
		/* Test with a box that is enclosed by the capsule */
		const float CapsuleRadius = CollisionShape.GetCapsuleRadius();
		const float CapsuleHeight = CollisionShape.GetCapsuleHalfHeight();
		const FCollisionShape BoxShape = FCollisionShape::MakeBox(FVector(CapsuleRadius * 0.707f, CapsuleRadius * 0.707f, CapsuleHeight));

		/* First test with a box rotates so the corners are along the major axes (ie rotated 45 Degrees) */
		bBlockingHit = GetWorld()->SweepSingleByChannel(OutHit, Start, End, FQuat(-1.f * UpdatedComponent->GetUpVector(), UE_PI * 0.25f), TraceChannel, BoxShape, Params, ResponseParams);

		/* If no hit, check again but without a rotated capsule */
		if (!bBlockingHit)
		{
			OutHit.Reset(1.f, false);
			bBlockingHit = GetWorld()->SweepSingleByChannel(OutHit, Start, End, UpdatedComponent->GetComponentQuat(), TraceChannel, BoxShape, Params, ResponseParams);
		}
	} // Use flat base for sweep

	return bBlockingHit;
}

// TODO
bool UCustomMovementComponent::IsValidLandingSpot(const FVector& CapsuleLocation, const FHitResult& Hit) const
{
	if (!Hit.bBlockingHit)
	{
		return false;
	}

	/* Skip some checks if penetrating. Penetration will be handled by FindFloor call (using a smaller capsule) */
	if (!Hit.bStartPenetrating)
	{
		/* Reject unwalkable floor normals */
		if (!IsWalkable(Hit))
		{
			return false;
		}

		float PawnRadius = UpdatedPrimitive->GetCollisionShape().GetCapsuleRadius();
		float PawnHalfHeight = UpdatedPrimitive->GetCollisionShape().GetCapsuleHalfHeight();

		/* Reject hits that are above our lower hemisphere (can happen when sliding down a vertical surface) */
		const float HitImpactHeight = (Hit.ImpactPoint | UpdatedComponent->GetUpVector());
		const float LowerHemisphereHeight = (Hit.Location | UpdatedComponent->GetUpVector()) - PawnHalfHeight + PawnRadius;
		if (HitImpactHeight >= LowerHemisphereHeight)
		{
			return false;
		}

		/* Reject hits that are barely on the cusp of the radius of the capsule */
		if (!IsWithinEdgeTolerance(Hit.Location, Hit.ImpactPoint, PawnRadius))
		{
			return false;
		}
	} // Not in penetration
	else
	{
		// TODO: Verify the below isn't contradictory. We'd never really need a max slope angle of >= 90 relative to the pawn
		/* Normal is nearly horizontal or downward, that's a penetration adjustment next to a vertical or overhanging wall. Don't pop to the floor */
		if ((Hit.Normal | UpdatedComponent->GetUpVector()) < UE_KINDA_SMALL_NUMBER)
		{
			return false;
		}
	} // In Penetration

	FGroundingStatus FloorResult;
	FindFloor(CapsuleLocation, FloorResult, false, &Hit);

	if (!FloorResult.IsWalkableFloor())
	{
		return false;
	}

	return true;
}

// TODO
bool UCustomMovementComponent::ShouldCheckForValidLandingSpot(const FHitResult& Hit) const
{
	return false;
}

// Done
float UCustomMovementComponent::GetPerchRadiusThreshold() const
{
	return FMath::Max(0.f, PerchRadiusThreshold);
}

// Done
float UCustomMovementComponent::GetValidPerchRadius() const
{
	const float PawnRadius = UpdatedPrimitive->GetCollisionShape().GetCapsuleRadius();
	return FMath::Clamp(PawnRadius - GetPerchRadiusThreshold(), 0.11f, PawnRadius);
}

// Done
bool UCustomMovementComponent::ShouldComputePerchResult(const FHitResult& InHit, bool bCheckRadius) const
{
	if (!InHit.IsValidBlockingHit())
	{
		return false;
	}

	/* Don't attempt perch if the edge radius is very small */
	if (GetPerchRadiusThreshold() <= SWEEP_EDGE_REJECT_DISTANCE)
	{
		return false;
	}

	if (bCheckRadius)
	{
		const float DistFromCenterSq = FVector::VectorPlaneProject(InHit.ImpactPoint - InHit.Location, UpdatedComponent->GetUpVector()).SizeSquared();
		const float StandOnEdgeRadius = GetValidPerchRadius();
		if (DistFromCenterSq <= FMath::Square(StandOnEdgeRadius))
		{
			/* Already within perch radius */
			return false;
		}
	}

	return true;
}

// TODO
bool UCustomMovementComponent::ComputePerchResult(const float TestRadius, const FHitResult& InHit,
	const float InMaxFloorDist, FGroundingStatus& OutPerchFloorResult) const
{
	if (InMaxFloorDist <= 0.f)
	{
		return false;
	}

	/* Sweep further than the actual requested distance, because a reduced capsule radius means we could miss some hits the normal radius would catch */
	float PawnRadius = UpdatedPrimitive->GetCollisionShape().GetCapsuleRadius();
	float PawnHalfHeight = UpdatedPrimitive->GetCollisionShape().GetCapsuleHalfHeight();

	const FVector CapsuleLocation = (bUseFlatBaseForFloorChecks ? InHit.TraceStart : InHit.Location);

	// TODO: Maths
	const float InHitAboveBase;
	const float PerchLineDist;
	const float PerchSweepDist;

	const float ActualSweepDist = PerchSweepDist + PawnRadius;
	ComputeFloorDist(CapsuleLocation, PerchLineDist, ActualSweepDist, OutPerchFloorResult, TestRadius);

	if (!OutPerchFloorResult.IsWalkableFloor())
	{
		return false;
	}
	else if (InHitAboveBase + OutPerchFloorResult.FloorDist > InMaxFloorDist)
	{
		/* Hit something past max distance */
		OutPerchFloorResult.bWalkableFloor = false;
		return false;
	}

	return false;
}

#pragma endregion CMC Copies
