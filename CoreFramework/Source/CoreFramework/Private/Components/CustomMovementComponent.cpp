// Copyright 2023 Abdulrahmen Almodaimegh. All Rights Reserved.
#include "CustomMovementComponent.h"
#include "Debug/CFW_LOG.h"
#include "CFW_PCH.h"

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
	
	
	int32 VisualizeMovement = 0;
	FAutoConsoleVariableRef CVarShowMovementVectors
	(
		TEXT("rmc.VisualizeMovement"),
		VisualizeMovement,
		TEXT("Visualize Movement. 0: Disable, 1: Enable"),
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

/* Log Cateogry */
DEFINE_LOG_CATEGORY(LogRMCMovement)

#pragma endregion Profiling & CVars

// Sets default values for this component's properties
UCustomMovementComponent::UCustomMovementComponent()
{
	PostPhysicsTickFunction.bCanEverTick = true;
	PostPhysicsTickFunction.bStartWithTickEnabled = false;
	PostPhysicsTickFunction.SetTickFunctionEnable(false);
	PostPhysicsTickFunction.TickGroup = TG_PostPhysics;

	MaxSimulationTimeStep = 0.05f;
	MaxSimulationIterations = 8;

	MaxDepenetrationWithGeometry = 500.f;
	MaxDepenetrationWithPawn = 100.f;

	Mass = 100.0f;
	bJustTeleported = true;

	LastUpdateRotation = FQuat::Identity;
	LastUpdateVelocity = FVector::ZeroVector;
	PendingImpulseToApply = FVector::ZeroVector;
	PendingLaunchVelocity = FVector::ZeroVector;

	PhysicsState = STATE_Grounded;
	bForceNextFloorCheck = true;
	bAlwaysCheckFloor = true;

	bEnablePhysicsInteraction = true;
	bPushForceUsingVerticalOffset = false;
	bPushForceScaledToMass = false;
	bScalePushForceToVelocity = true;

	bEnableScopedMovementUpdates = true;

	OldBaseQuat = FQuat::Identity;
	OldBaseLocation = FVector::ZeroVector;
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
	PhysicsState = STATE_Grounded;
}


/* FUNCTIONAL */
void UCustomMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	SCOPE_CYCLE_COUNTER(STAT_TickComponent)
	
	InputVector = ConsumeInputVector();

	if (ShouldSkipUpdate(DeltaTime))
	{
		return;
	}

	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const bool bIsSimulatingPhysics = UpdatedComponent->IsSimulatingPhysics();

	/* Check if we fell out of the world*/
	if (bIsSimulatingPhysics && !PawnOwner->CheckStillInWorld())
	{
		return;
	}
	
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
	
	/* Perform Move */
	PerformMovement(DeltaTime);
	
	/* Physics interactions updates */
	if (bEnablePhysicsInteraction)
	{
		SCOPE_CYCLE_COUNTER(STAT_PhysicsInteraction)
		ApplyDownwardForce(DeltaTime);
		ApplyRepulsionForce(DeltaTime);
	}

	/* Nice extra debug visualizer should add eventually from CMC */
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	const bool bVisualizeMovement = RMCCVars::VisualizeMovement > 0;
	if (bVisualizeMovement)
	{
		VisualizeMovement();
	}
#endif 
}

void FCustomMovementComponentPostPhysicsTickFunction::ExecuteTick(float DeltaTime, ELevelTick TickType, ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
{
	FActorComponentTickFunction::ExecuteTickHelper(Target, /*bTickInEditor=*/ false, DeltaTime, TickType, [this](float DilatedTime)
	{
		Target->PostPhysicsTickComponent(DilatedTime, *this);
	});
}


void UCustomMovementComponent::PostPhysicsTickComponent(float DeltaTime, FCustomMovementComponentPostPhysicsTickFunction& ThisTickFunction)
{
	if (bDeferUpdateBasedMovement)
	{
		FScopedMovementUpdate ScopedMovementUpdate(UpdatedComponent, bEnableScopedMovementUpdates ? EScopedUpdate::DeferredUpdates : EScopedUpdate::ImmediateUpdates);
		UpdateBasedMovement(DeltaTime);
		SaveBaseLocation();
		bDeferUpdateBasedMovement = false;
	}
}


#pragma region Movement Component Overrides
// BEGIN UMovementComponent

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

void UCustomMovementComponent::AddRadialForce(const FVector& Origin, float Radius, float Strength,
	ERadialImpulseFalloff Falloff)
{
	FVector Delta = UpdatedComponent->GetComponentLocation() - Origin;
	const float DeltaMagnitude = Delta.Size();

	// Do nothing if outside radius
	if (DeltaMagnitude > Radius) return;

	Delta = Delta.GetSafeNormal();

	float ForceMagnitude = Strength;
	if (Falloff == RIF_Linear && Radius > 0.f)
	{
		ForceMagnitude *= (1.f - (DeltaMagnitude / Radius));
	}

	AddForce(Delta * ForceMagnitude);
}

void UCustomMovementComponent::AddRadialImpulse(const FVector& Origin, float Radius, float Strength,
	ERadialImpulseFalloff Falloff, bool bVelChange)
{
	FVector Delta = UpdatedComponent->GetComponentLocation() - Origin;
	const float DeltaMagnitude = Delta.Size();

	// Do nothing if outside radius
	if(DeltaMagnitude > Radius)
	{
		return;
	}

	Delta = Delta.GetSafeNormal();

	float ImpulseMagnitude = Strength;
	if (Falloff == RIF_Linear && Radius > 0.0f)
	{
		ImpulseMagnitude *= (1.0f - (DeltaMagnitude / Radius));
	}

	AddImpulse(Delta * ImpulseMagnitude, bVelChange);
}

void UCustomMovementComponent::StopActiveMovement()
{
	Super::StopActiveMovement();

	Acceleration = FVector::ZeroVector;
}

// END UMovementComponent

// BEGIN UPawnMovementComponent

void UCustomMovementComponent::NotifyBumpedPawn(APawn* BumpedPawn)
{
	Super::NotifyBumpedPawn(BumpedPawn);
}

void UCustomMovementComponent::OnTeleported()
{
	Super::OnTeleported();

	bJustTeleported = true;

	UpdateFloorFromAdjustment();

	UPrimitiveComponent* OldBase = GetMovementBase();
	UPrimitiveComponent* NewBase = nullptr;

	// TODO: Maybe an extra vertical velocity check against floor normal
	if (OldBase && CurrentFloor.IsWalkableFloor() && CurrentFloor.FloorDist <= MAX_FLOOR_DIST)
	{
		NewBase = CurrentFloor.HitResult.Component.Get();
	}
	else
	{
		CurrentFloor.Clear();
	}

	const bool bWasFalling = (PhysicsState == STATE_Falling);
	
	if (!CurrentFloor.IsWalkableFloor() || (OldBase && !NewBase))
	{
		SetMovementState(STATE_Falling);
	}
	else if (NewBase && bWasFalling)
	{
		ProcessLanded(CurrentFloor.HitResult, 0.f, 0);
	}

	SaveBaseLocation();
}

// END UPawnMovementComponent
#pragma endregion Movement Component Overrides

#pragma region Movement State & Interface

void UCustomMovementComponent::SetMovementState(EMovementState NewMovementState)
{
	if (PhysicsState == NewMovementState)
	{
		return;
	}

	const EMovementState PrevMovementState = PhysicsState;
	PhysicsState = NewMovementState;
	
	OnMovementStateChanged(PrevMovementState);
}


void UCustomMovementComponent::OnMovementStateChanged(EMovementState PreviousMovementState)
{
	if (PhysicsState == STATE_Grounded)
	{
		// TODO: Project velocity in a way that makes sense
		/* Update floor/base on initial entry of the walking physics */
		FindFloor(UpdatedComponent->GetComponentLocation(), CurrentFloor, false);
		AdjustFloorHeight();
		SetBaseFromFloor(CurrentFloor);

		// DEBUG:
		LOG_HIT(CurrentFloor.HitResult, 2.f);
		
		if (CurrentFloor.bWalkableFloor && PreviousMovementState == STATE_Falling)
		{
			Velocity = FVector::VectorPlaneProject(Velocity, CurrentFloor.HitResult.Normal); // HACK: Was prev Normal
		}
	}
	else if (PhysicsState == STATE_Falling)
	{
		// DEBUG:
		LOG_HIT(CurrentFloor.HitResult, 2.f);

		CurrentFloor.Clear();
		
		DecayingFormerBaseVelocity = GetImpartedMovementBaseVelocity();
		Velocity += DecayingFormerBaseVelocity;

		SetBase(nullptr);
	}
	else
	{
		ClearAccumulatedForces();
	}
	
	if (PhysicsState == STATE_Falling && PreviousMovementState != STATE_Falling)
	{
		IPathFollowingAgentInterface* PFAgent = GetPathFollowingAgent();
		if (PFAgent) PFAgent->OnStartedFalling();
	}
}

void UCustomMovementComponent::DisableMovement()
{
	SetMovementState(STATE_None);
}

void UCustomMovementComponent::AddImpulse(FVector Impulse, bool bVelocityChange)
{
	if (!Impulse.IsZero() && (PhysicsState != STATE_None) && IsActive())
	{
		// handle scaling by mass
		FVector FinalImpulse = Impulse;
		if ( !bVelocityChange )
		{
			if (Mass > UE_SMALL_NUMBER)
			{
				FinalImpulse = FinalImpulse / Mass;
			}
		}

		PendingImpulseToApply += FinalImpulse;
	}
}

void UCustomMovementComponent::AddForce(FVector Force)
{
	if (!Force.IsZero() && (PhysicsState != STATE_None) && IsActive())
	{
		if (Mass > UE_SMALL_NUMBER)
		{
			PendingForceToApply += Force / Mass;
		}
	}
}

void UCustomMovementComponent::ClearAccumulatedForces()
{
	PendingImpulseToApply = FVector::ZeroVector;
	PendingForceToApply = FVector::ZeroVector;
	PendingLaunchVelocity = FVector::ZeroVector;
}

// TODO:
void UCustomMovementComponent::ApplyAccumulatedForces(float DeltaTime)
{
	if (PendingImpulseToApply.Z != 0.f || PendingForceToApply.Z != 0.f)
	{
		// check to see if applied momentum is enough to overcome gravity
		if ( IsMovingOnGround() && (PendingImpulseToApply.Z + (PendingForceToApply.Z * DeltaTime) + (GetGravityZ() * DeltaTime) > UE_SMALL_NUMBER))
		{
			SetMovementState(STATE_Falling);
		}
	}

	Velocity += PendingImpulseToApply + (PendingForceToApply * DeltaTime);
	
	// Don't call ClearAccumulatedForces() because it could affect launch velocity
	PendingImpulseToApply = FVector::ZeroVector;
	PendingForceToApply = FVector::ZeroVector;
}

// TODO:
bool UCustomMovementComponent::HandlePendingLaunch()
{
	if (!PendingLaunchVelocity.IsZero())
	{
		Velocity = PendingLaunchVelocity;
		PendingLaunchVelocity = FVector::ZeroVector;
		if (Velocity.Z > 0)
			SetMovementState(STATE_Falling);
		return true;
	}
	return false;
}


#pragma endregion Movement State & Interface

#pragma region Core Simulation Handling

/* FUNCTIONAL */
bool UCustomMovementComponent::CanMove() const
{
	if (!UpdatedComponent || !PawnOwner) return false;
	if (UpdatedComponent->Mobility != EComponentMobility::Movable) return false;
	if (bStuckInGeometry) return false;

	return true;
}

float UCustomMovementComponent::GetSimulationTimeStep(float RemainingTime, uint32 Iterations) const
{
	if (RemainingTime > MaxSimulationTimeStep)
	{
		if (Iterations < MaxSimulationIterations)
		{
			RemainingTime = FMath::Min(MaxSimulationTimeStep, RemainingTime * 0.5f);
		}
	}

	return FMath::Max(MIN_TICK_TIME, RemainingTime);
}


void UCustomMovementComponent::PerformMovement(float DeltaTime)
{
	SCOPE_CYCLE_COUNTER(STAT_PerformMovement)
	
	// Setup movement, and do not progress if setup fails
	// TODO: Looking at CMC in UE4.10, there's no need to consume root motion and all that. It just returns...
	if (!PreMovementUpdate(DeltaTime))
	{
		return;
	}

	// TODO: Temp removed the OldVelocity/Location stuff because it doesn't really have any relevance atm
	
	// Internal Character Move - looking at CMC, it applies UpdateVelocity, RootMotion, etc... before the character move...
	{
		// Scoped updates can improve performance of multiple MoveComponent calls - CMC
		FScopedMovementUpdate ScopedMovementUpdate(UpdatedComponent, bEnableScopedMovementUpdates ? EScopedUpdate::DeferredUpdates : EScopedUpdate::ImmediateUpdates);
		
		/* Update Based Movement */
		TryUpdateBasedMovement(DeltaTime);
		
		/* Trigger gameplay event for velocity modification & apply pending impulses and forces*/
		ApplyAccumulatedForces(DeltaTime); // TODO: Here's where we'd also check for bMustUnground
		HandlePendingLaunch(); // TODO: This would auto-unground
		ClearAccumulatedForces();
		

		UpdateVelocity(Velocity, DeltaTime);

		/* Apply root motion after velocity modifications */
		if (SkeletalMesh && SkeletalMesh->IsPlayingRootMotion())
		{
			TickPose(DeltaTime);
		}
		
		
		/* Perform actual move */
		StartMovementTick(DeltaTime, 0);

		if (!HasAnimRootMotion())
		{
			FQuat CurrentRotation = UpdatedComponent->GetComponentQuat();
			UpdateRotation(CurrentRotation, DeltaTime);
			//MoveUpdatedComponent(FVector::ZeroVector, CurrentRotation, true);
		}
		else // Apply physics rotation
		{
			const FQuat OldActorRotationQuat = UpdatedComponent->GetComponentQuat();
			const FQuat RootMotionRotationQuat = RootMotionParams.GetRootMotionTransform().GetRotation();
			if (!RootMotionRotationQuat.Equals(FQuat::Identity))
			{
				const FQuat NewActorRotationQuat = RootMotionRotationQuat * OldActorRotationQuat;
				MoveUpdatedComponent(FVector::ZeroVector, NewActorRotationQuat, true);
			}

			RootMotionParams.Clear();
		} // Apply root motion rotation
		
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

	// Differs from bJustTeleported which is for within a movement tick. This is in between component ticks
	// to check if we've moved outside of the movement component. If true, then we should force a floor check.
	const bool bTeleportedSinceLastUpdate = UpdatedComponent->GetComponentLocation() != LastUpdateLocation;
	
	if (!CanMove() || UpdatedComponent->IsSimulatingPhysics())
	{
		/* Consume root motion */

		/* Clear pending physics forces*/

		return false;
	}

	/* Setup */
	bForceNextFloorCheck |= (IsMovingOnGround() && bTeleportedSinceLastUpdate);
	
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

	switch (PhysicsState)
	{
		case STATE_None:
			break;
		case STATE_Grounded:
			GroundMovementTick(DeltaTime, Iterations);
			break;
		case STATE_Falling:
			AirMovementTick(DeltaTime, Iterations);
			break;
		default:
			SetMovementState(STATE_None);
			break;
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
		SubsteppedTick(Velocity, IterTick);

		/* Cache current values */
		UPrimitiveComponent* const OldBase = GetMovementBase();
		const FVector PreviousBaseLocation = (OldBase != NULL) ? OldBase->GetComponentLocation() : FVector::ZeroVector;
		const FVector OldLocation = UpdatedComponent->GetComponentLocation();
		const FGroundingStatus OldFloor = CurrentFloor;

		/* Project Velocity To Ground Before Computing Move Deltas */
		MaintainHorizontalGroundVelocity();
		const FVector OldVelocity = Velocity;

		/* Compute move parameters */
		const FVector MoveVelocity = Velocity;
		const FVector Delta = MoveVelocity * IterTick;
		const bool bZeroDelta = Delta.IsNearlyZero();
		FStepDownFloorResult StepDownResult;
		
		/* Exit if the delta is zero, no movement should happen anyways */
		if (bZeroDelta)
		{
			RemainingTime = 0.f;
		} // Zero delta
		else
		{
			// Perform the actual move
			MoveAlongFloor(MoveVelocity, IterTick, &StepDownResult);

			/* Worth keeping this here if we wanna put the CalcVelocity event after MoveAlongFloor */
			if (IsFalling())
			{
				const float DesiredDist = Delta.Size();
				if (DesiredDist > UE_KINDA_SMALL_NUMBER)
				{
					// TODO: Perhaps project this onto pawns up plane? CMC uses Size2D so just for math consistency...
					const float ActualDist = FVector::VectorPlaneProject(UpdatedComponent->GetComponentLocation() - OldLocation, GetUpOrientation(MODE_Gravity)).Size();
					RemainingTime += IterTick * (1.f - FMath::Min(1.f, ActualDist / DesiredDist));
				}
				
				StartMovementTick(RemainingTime, Iterations);
				return;
			}
		} // Non-zero delta

		/* Update Floor (Step down might've computed a floor) */
		if (StepDownResult.bComputedFloor)
		{
			CurrentFloor = StepDownResult.FloorResult;
		}
		else
		{
			FindFloor(UpdatedComponent->GetComponentLocation(), CurrentFloor, bZeroDelta, nullptr);
		}

		/* Check for ledges here */
		const bool bCheckLedges = !CanWalkOffLedge();
		if (bCheckLedges && !CurrentFloor.IsWalkableFloor())
		{
			// Calculate possible alternate movement
			const FVector DownDir = -GetUpOrientation(MODE_Gravity); // TODO: We're on ground, so assume our rotation is oriented correctly
			const FVector NewDelta = bTriedLedgeMove ? FVector::ZeroVector : GetLedgeMove(OldLocation, Delta, DownDir);
			if (!NewDelta.IsZero())
			{
				/* First revert current move (Want to avoid walking off ledges in this scope) */
				RevertMove(OldLocation, OldBase, PreviousBaseLocation, OldFloor, false);

				/* Avoid repeated ledge moves if the first one fails */
				bTriedLedgeMove = true;

				/* Try new movement direction */
				Velocity = NewDelta/IterTick;
				RemainingTime += IterTick;
				continue;
			} // Adjusted move delta non-zero
			else
			{
				/* Possible transition to a fall */
				bool bMustJump = bZeroDelta || (OldBase == nullptr || (!OldBase->IsQueryCollisionEnabled() && MovementBaseUtility::IsDynamicBase(OldBase)));
				if ((bMustJump || !bCheckedFall) && CheckFall(OldFloor, CurrentFloor.HitResult, Delta, OldLocation, RemainingTime, IterTick, Iterations, bMustJump))
				{
					return;
				}
				bCheckedFall = true;

				/* Revert current move, zero delta for ledge adjustment */
				RevertMove(OldLocation, OldBase, PreviousBaseLocation, OldFloor, true);
				RemainingTime = 0.f;
				break;
			} // Adjusted move delta zero
			
		} // Check for ledge movement, in this scope we're trying to prevent walking off a ledge
		else
		{
			/* Validate the floor check */
			if (CurrentFloor.IsWalkableFloor())
			{
				/* Check denivelation angle */
				if (ShouldCatchAir(OldFloor, CurrentFloor))
				{
					// TODO: Events and swtiching to falling
					HandleWalkingOffLedge(OldFloor.HitResult.ImpactNormal, OldFloor.HitResult.Normal, OldLocation, IterTick);
					if (IsMovingOnGround())
					{
						StartFalling(Iterations, RemainingTime, IterTick, Delta, OldLocation);
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
				Hit.TraceEnd = Hit.TraceStart + GetUpOrientation(MODE_PawnUp) * MAX_FLOOR_DIST; // TODO: Maybe gravity orientation?
				const FVector RequestedAdjustment = GetPenetrationAdjustment(Hit);
				ResolvePenetration(RequestedAdjustment, Hit, UpdatedComponent->GetComponentQuat());
				bForceNextFloorCheck = true; // Force us to sweep for a floor next time
				
			} // Floor penetration or consumed all time

			/* Check if we need to start falling */
			if (!CurrentFloor.IsWalkableFloor() && !CurrentFloor.HitResult.bStartPenetrating)
			{
				const bool bMustJump = bJustTeleported || bZeroDelta || (OldBase == nullptr || (!OldBase->IsCollisionEnabled() && MovementBaseUtility::IsDynamicBase(OldBase)));
				if ((bMustJump || !bCheckedFall) && CheckFall(OldFloor, CurrentFloor.HitResult, Delta, OldLocation, RemainingTime, IterTick, Iterations, bMustJump))
				{
					return;
				}
				bCheckedFall = true;
			}
			
		} // Don't check for ledges

		/* Allow overlap events to change physics state and velocity */
		if (IsMovingOnGround())
		{
			/* Make velocity reflect actual move */
			if (!bJustTeleported && IterTick >= MIN_TICK_TIME)
			{
				const FVector NewVelocity = (UpdatedComponent->GetComponentLocation() - OldLocation) / IterTick;
				if ((NewVelocity.GetSafeNormal() | Velocity.GetSafeNormal()) < 0.97f)
				{
					if (!(NewVelocity.IsNearlyZero() || Velocity.IsNearlyZero()))
					{
						FLog(Error, "1. New Velocity = %s", *NewVelocity.ToCompactString());
						FLog(Error, "2. Old Velocity = %s", *Velocity.ToCompactString());
						LOG_HIT(CurrentFloor.HitResult, 1.f);
					}
				}
				MaintainHorizontalGroundVelocity(); //BUG: Could this be it?
			}
		}

		/* If we didn't move at all, abort since future iterations will also be stuck */
		if (UpdatedComponent->GetComponentLocation() == OldLocation)
		{
			RemainingTime = 0.f;
			break;
		}
	}

	if (IsMovingOnGround())
	{
		MaintainHorizontalGroundVelocity(); 
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
	
	float RemainingTime = DeltaTime;

	while ((RemainingTime >= MIN_TICK_TIME) && (Iterations < MaxSimulationIterations))
	{
		/* Setup current move iteration */
		Iterations++;
		bJustTeleported = false;
		const float IterTick = GetSimulationTimeStep(RemainingTime, Iterations);
		RemainingTime -= IterTick;
		SubsteppedTick(Velocity, IterTick);
		
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
		DecayFormerBaseVelocity(IterTick);

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
				// TODO: Can only do this on Planar, then re-add vertical to ensure gravity is in full effect? But that makes an assumption about gravity being used here which it may not be...
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
						if ((Hit.Normal | GetUpOrientation(MODE_Gravity)) > 0.001f) // TODO:  If normal of hit is slightly vertically upwards 
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
							Velocity = HasAnimRootMotion() ? FVector::VectorPlaneProject(Velocity, GetUpOrientation(MODE_Gravity)) + NewVelocity.ProjectOnToNormal(GetUpOrientation(MODE_Gravity)): NewVelocity;
						}

						/* bDitch = true means the pawn is straddling between two slopes neither of which it can stand on */
						const FVector PawnUp = GetUpOrientation(MODE_Gravity);
						bool bDitch = (((OldHitImpactNormal | PawnUp) > 0.f) && ((Hit.ImpactNormal | PawnUp) > 0.f) && (FMath::Abs(AdjustedDelta | PawnUp) <= UE_KINDA_SMALL_NUMBER) && ((Hit.ImpactNormal | OldHitImpactNormal) < 0.f));
						SafeMoveUpdatedComponent(AdjustedDelta, PawnRotation, true, Hit);
						if (Hit.Time == 0.f)
						{
							/* If we're stuck, try to side step */
							FVector SideDelta = FVector::VectorPlaneProject(OldHitNormal + Hit.ImpactNormal, PawnUp).GetSafeNormal();
							if (SideDelta.IsNearlyZero())
							{
								SideDelta = FVector::ZeroVector;//FVector(OldHitNormal.Y, -OldHitNormal.X, 0).GetSafeNormal(); // HACK: TEMP CHANGE THIS LATER?
							}
							SafeMoveUpdatedComponent(SideDelta, PawnRotation, true, Hit);
						}
						
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


void UCustomMovementComponent::MoveAlongFloor(const FVector& InVelocity, float DeltaTime, FStepDownFloorResult* OutStepDownResult)
{
	if (!CurrentFloor.IsWalkableFloor())
	{
		return;
	}

	/* Project Velocity Along Floor? */
	//MaintainHorizontalGroundVelocity(); // DEBUG: This does nothing here...
	
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
			OnStuckInGeometry();
		}
	} // Was in penetration
	else if (Hit.IsValidBlockingHit())
	{
		LOG_HIT(Hit, 2.f);
		
		float PercentTimeApplied = Hit.Time;
		/* Could be a walkable floor or ramp, unsure if that would be the case because we prep the velocity for this beforehand */
		// TODO: May be helpful in the case of using a flat base because of the issues cropping up with that w/ zero step height...
		
		/* Impacted a barrier, check if we can use it as stairs */
		if (CanStepUp(Hit) || (GetMovementBase() != nullptr && Hit.HitObjectHandle == GetMovementBase()->GetOwner()))
		{
			const FVector Orientation = GetUpOrientation(MODE_Gravity);
			const FVector PreStepUpLocation = UpdatedComponent->GetComponentLocation();
			if (!StepUp(Orientation, Hit, Delta * (1.f - PercentTimeApplied), OutStepDownResult))
			{
				/* Step up failed, handle impact */
				HandleImpact(Hit, LastMoveTimeSlice, Delta);
				SlideAlongSurface(Delta, 1.f - PercentTimeApplied, Hit.Normal, Hit, true);
			} // Step up failed
			else
			{
				// Perhaps don't recalculate velocity based on StepUp adjustment?
				const float StepUpTimeSlice = (1.f - PercentTimeApplied) * DeltaTime;
				if (!HasAnimRootMotion() && StepUpTimeSlice > UE_KINDA_SMALL_NUMBER)
				{
					// BUG: This whole velocity recalculation is kind of broken
					//Velocity = (UpdatedComponent->GetComponentLocation() - PreStepUpLocation) / StepUpTimeSlice;
					//Velocity.Z = 0;
					//Velocity = FVector::VectorPlaneProject(Velocity, Orientation); // TODO: TEMPORARY
					//MaintainHorizontalGroundVelocity();
				}
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
	SaveBaseLocation();
	
	UpdateComponentVelocity();
	
	LastUpdateLocation = UpdatedComponent ? UpdatedComponent->GetComponentLocation() : FVector::ZeroVector;
	LastUpdateRotation = UpdatedComponent ? UpdatedComponent->GetComponentQuat() : FQuat::Identity;
	LastUpdateVelocity = Velocity;
}

void UCustomMovementComponent::ProcessLanded(FHitResult& Hit, float DeltaTime, uint32 Iterations)
{
	if ((DeltaTime < MIN_TICK_TIME) || (Iterations >= MaxSimulationIterations))
	{
		return;
	}

	LOG_HIT(Hit, 2.f);

	const FVector PreImpactAccel = Acceleration + (IsFalling() ? GetUpOrientation(MODE_Gravity) * GetGravityZ() : FVector::ZeroVector) ;
	const FVector PreImpactVelocity = Velocity;
	ApplyImpactPhysicsForces(Hit, PreImpactAccel, PreImpactVelocity);

	if (IsFalling())
	{
		SetMovementState(STATE_Grounded);
	}

	IPathFollowingAgentInterface* PFAgent = GetPathFollowingAgent();
	if (PFAgent)
	{
		PFAgent->OnLanded();
	}

	StartMovementTick(DeltaTime, Iterations);
}

void UCustomMovementComponent::StartFalling(uint32 Iterations, float RemainingTime, float IterTick, const FVector& Delta, const FVector SubLoc)
{
	const float DesiredDist = Delta.Size();
	const float ActualDist = FVector::VectorPlaneProject(UpdatedComponent->GetComponentLocation() - SubLoc, GetUpOrientation(MODE_PawnUp)).Size(); // TODO: Doesn't respect arbitrary rotation, but we're transitioning to a fall
	RemainingTime = (DesiredDist < UE_KINDA_SMALL_NUMBER ? 0.f : RemainingTime + IterTick * (1.f - FMath::Min(1.f, ActualDist/DesiredDist)));

	if (IsMovingOnGround())
	{
		// Catch cases for PIE, following CMC instructions
		if (!GIsEditor || (GetWorld()->HasBegunPlay() && (GetWorld()->GetTimeSeconds() >= 1.f)))
		{
			SetMovementState(STATE_Falling);
		}
		else
		{
			// to catch the floor
			bForceNextFloorCheck = true;
		}
	}

	StartMovementTick(RemainingTime, Iterations);
}

void UCustomMovementComponent::RevertMove(const FVector& OldLocation, UPrimitiveComponent* OldBase, const FVector& InOldBaseLocation, const FGroundingStatus& OldFloor, bool bFailMove)
{
	UpdatedComponent->SetWorldLocation(OldLocation, false, nullptr, bJustTeleported ? ETeleportType::TeleportPhysics : ETeleportType::None);

	bJustTeleported = false;

	if (IsValid(OldBase) &&
		(!MovementBaseUtility::IsDynamicBase(OldBase) || OldBase->Mobility == EComponentMobility::Static || OldBase->GetComponentLocation() == InOldBaseLocation))
	{
		CurrentFloor = OldFloor;
		SetBase(OldBase, OldFloor.HitResult.BoneName);
	}
	else
	{
		SetBase(nullptr);
	}

	if (bFailMove)
	{
		// End movement now
		Velocity = FVector::ZeroVector;
		Acceleration = FVector::ZeroVector;
	}
}


#pragma endregion Core Simulation Handling

#pragma region Ground Stability Handling

bool UCustomMovementComponent::IsFloorStable(const FHitResult& Hit) const
{
	if (!Hit.IsValidBlockingHit()) return false;

	float TestStableAngle = MaxStableSlopeAngle;
	
	// See if the component overrides floor angle
	const UPrimitiveComponent* HitComponent = Hit.Component.Get();
	if (HitComponent)
	{
		const FWalkableSlopeOverride& SlopeOverride = HitComponent->GetWalkableSlopeOverride();
		TestStableAngle = SlopeOverride.ModifyWalkableFloorZ(TestStableAngle);
		if (TestStableAngle != MaxStableSlopeAngle) TestStableAngle = SlopeOverride.GetWalkableSlopeAngle();
	}

	const float Angle = FMath::RadiansToDegrees(FMath::Acos(Hit.ImpactNormal | GetUpOrientation(MODE_PawnUp)));
	
	if (Angle > TestStableAngle)
	{
		return false;
	}

	return true;
}

bool UCustomMovementComponent::CheckFall(const FGroundingStatus& OldFloor, const FHitResult& Hit, const FVector& Delta,
	const FVector& OldLocation, float RemainingTime, float IterTick, uint32 Iterations, bool bMustUnground)
{
	if (bMustUnground || CanWalkOffLedge())
	{
		HandleWalkingOffLedge(OldFloor.HitResult.ImpactNormal, OldFloor.HitResult.Normal, OldLocation, IterTick);
		if (IsMovingOnGround())
		{
			StartFalling(Iterations, RemainingTime, IterTick, Delta, OldLocation);
		}
		return true;
	}
	return false;
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
		const float InitialHeight = UpdatedComponent->GetComponentLocation() | GetUpOrientation(MODE_PawnUp);
		const float AvgFloorDist = (MIN_FLOOR_DIST + MAX_FLOOR_DIST) * 0.5f;

		/* Move dist such that if we're above MAX, it'll move us down - below MIN, it'll move us up. Relative to the average of the two thresholds */
		const float MoveDist = AvgFloorDist - OldFloorDist;

		SafeMoveUpdatedComponent(GetUpOrientation(MODE_PawnUp) * MoveDist, UpdatedComponent->GetComponentQuat(), true, AdjustHit);

		/* Check if the snapping resulted in a valid hit */
		if (!AdjustHit.IsValidBlockingHit())
		{
			CurrentFloor.FloorDist += MoveDist; 
		} // Invalid blocking hit, adjust our cached floor dist for next next
		else if (MoveDist > 0.f)
		{
			const float CurrentHeight = UpdatedComponent->GetComponentLocation() | GetUpOrientation(MODE_PawnUp);
			CurrentFloor.FloorDist += CurrentHeight - InitialHeight;
		} // Below MIN_FLOOR_DIST
		else
		{
			const float CurrentHeight = UpdatedComponent->GetComponentLocation() | GetUpOrientation(MODE_PawnUp);
			CurrentFloor.FloorDist = CurrentHeight - (AdjustHit.Location | GetUpOrientation(MODE_PawnUp));
			if (IsFloorStable(AdjustHit))
			{
				CurrentFloor.SetFromSweep(AdjustHit, CurrentFloor.FloorDist, true);
			}
		} // Above MAX_FLOOR_DIST, could be a new floor value so we set that if its walkable

		/* Don't recalculate velocity based on snapping, also avoid if we moved out of penetration */
		bJustTeleported |= (OldFloorDist < 0.f); // TODO: This could affect projection if we don't do it manually (Old TODO, irrelevant maybe)

		/* If something caused us to adjust our height (especially a depenetration), we should ensure another check next frame or we will keep a stale result */
		bForceNextFloorCheck = true;
	}
}

// DEBUG: Seems right?
bool UCustomMovementComponent::IsWithinEdgeTolerance(const FVector& CapsuleLocation, const FVector& TestImpactPoint,
	const float CapsuleRadius) const
{
	const float DistFromCenterSq = FVector::VectorPlaneProject(TestImpactPoint - CapsuleLocation, GetUpOrientation(MODE_PawnUp)).SizeSquared();
	const float ReducedRadiusSq = FMath::Square(FMath::Max(SWEEP_EDGE_REJECT_DISTANCE + UE_KINDA_SMALL_NUMBER, CapsuleRadius - SWEEP_EDGE_REJECT_DISTANCE));
	return DistFromCenterSq < ReducedRadiusSq;
}


void UCustomMovementComponent::ComputeFloorDist(const FVector& CapsuleLocation, float LineDistance, float SweepDistance,
	FGroundingStatus& OutFloorResult, float SweepRadius, const FHitResult* DownwardSweepResult) const
{
	OutFloorResult.Clear();

	float PawnRadius = GetCapsuleRadius();
	float PawnHalfHeight = GetCapsuleHalfHeight();
	
	bool bSkipSweep = false;
	if (DownwardSweepResult != nullptr && DownwardSweepResult->IsValidBlockingHit())
	{
		/* Accept it only if the supplied sweep was vertical and downwards relative to character orientation */
		float TraceStartHeight = DownwardSweepResult->TraceStart | GetUpOrientation(MODE_PawnUp); // TODO: Gravity Orientation if ungrounded?
		float TraceEndHeight = DownwardSweepResult->TraceEnd | GetUpOrientation(MODE_PawnUp);
		float TracePlaneProjection = FVector::VectorPlaneProject(DownwardSweepResult->TraceStart - DownwardSweepResult->TraceStart, GetUpOrientation(MODE_PawnUp)).SizeSquared();

		if ((TraceStartHeight > TraceEndHeight) && (TracePlaneProjection <= UE_KINDA_SMALL_NUMBER))
		{
			/* Reject hits that are barely on the cusp of the radius of the capsule */
			if (IsWithinEdgeTolerance(DownwardSweepResult->Location, DownwardSweepResult->ImpactPoint, PawnRadius))
			{
				/* Don't try a redundant sweep, regardless of whether this sweep is usable */
				bSkipSweep = true;

				DEBUG_PRINT_MSG(1, "Skipping Sweet, using down hit result")

				const bool bIsWalkable = IsFloorStable(*DownwardSweepResult);
				const float FloorDist = (CapsuleLocation - DownwardSweepResult->Location) | GetUpOrientation(MODE_PawnUp);
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
		bBlockingHit = FloorSweepTest(Hit, CapsuleLocation, CapsuleLocation - GetUpOrientation(MODE_PawnUp) * TraceDist, CollisionChannel, CapsuleShape, QueryParams, ResponseParam);
		
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

					bBlockingHit = FloorSweepTest(Hit, CapsuleLocation, CapsuleLocation - GetUpOrientation(MODE_PawnUp) * TraceDist, CollisionChannel, CapsuleShape, QueryParams, ResponseParam);
				}
			}

			/* Reduce the hit distance by ShrinkHeight because we shrank the capsule for the trace */
			/* Also allow negative distances here, because this allows us to pull out of penetration */
			const float MaxPenetrationAdjust = FMath::Max(MAX_FLOOR_DIST, PawnRadius);
			const float SweepResult = FMath::Max(-MaxPenetrationAdjust, Hit.Time * TraceDist - ShrinkHeight);

			OutFloorResult.SetFromSweep(Hit, SweepResult, false);
			if (Hit.IsValidBlockingHit() && IsFloorStable(Hit))
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
		const FVector Down = -GetUpOrientation(MODE_PawnUp) * TraceDist;
		QueryParams.TraceTag = SCENE_QUERY_STAT_NAME_ONLY(FloorLineTrace);

		FHitResult Hit(1.f);
		bBlockingHit = GetWorld()->LineTraceSingleByChannel(Hit, LineTraceStart, LineTraceStart + Down, CollisionChannel, QueryParams, ResponseParam);
		LOG_HIT(Hit, 2.f);
		
		if (bBlockingHit)
		{
			if (Hit.Time > 0.f)
			{
				/* Reduce the hit distance by the shrink height because we started the trace higher than the base */
				/* Also allow negative distances here, because this allows us to pull out of penetration */
				const float MaxPenetrationAdjust = FMath::Max(MAX_FLOOR_DIST, PawnRadius);
				const float LineResult = FMath::Max(-MaxPenetrationAdjust, Hit.Time * TraceDist - ShrinkHeight);

				OutFloorResult.bBlockingHit = true;
				if (LineResult <= LineDistance && IsFloorStable(Hit))
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

// TODO
void UCustomMovementComponent::FindFloor(const FVector& CapsuleLocation, FGroundingStatus& OutFloorResult,
	bool bCanUseCachedLocation, const FHitResult* DownwardSweepResult) const
{
	SCOPE_CYCLE_COUNTER(STAT_ProbeGround)
	
	/* If collision is not enabled, or bSolveGrounding is not set, we don't care */
	if (!UpdatedComponent->IsQueryCollisionEnabled())
	{
		OutFloorResult.Clear();
		return;
	}

	if (DownwardSweepResult)
	{
		DEBUG_PRINT_MSG(2, "Find Floor Downward Sweep Hit")
		LOG_HIT((*DownwardSweepResult), 2);
	}

	FLog(Display, "At Location [%s]", *CapsuleLocation.ToString())
	
	const float CapsuleRadius = GetCapsuleRadius();

	/* Increase height check slightly if currently groudned to prevent ground snapping height from later invalidating the floor result */
	const float HeightCheckAdjust = (IsMovingOnGround() ? MAX_FLOOR_DIST + UE_KINDA_SMALL_NUMBER : -MAX_FLOOR_DIST);

	/* Setup sweep distances */
	float FloorSweepTraceDist = FMath::Max(MAX_FLOOR_DIST, FMath::Max(ExtraFloorProbingDistance, MaxStepHeight) + HeightCheckAdjust);
	float FloorLineTraceDist = FloorSweepTraceDist;
	bool bNeedToValidateFloor =  true;

	/* Perform sweep */
	if (FloorLineTraceDist > 0.f || FloorSweepTraceDist > 0.f)
	{
		UCustomMovementComponent* MutableThis = const_cast<UCustomMovementComponent*>(this);
		
		if (bAlwaysCheckFloor || !bCanUseCachedLocation || bForceNextFloorCheck || bJustTeleported)
		{
			MutableThis->bForceNextFloorCheck = false;
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
				MutableThis->bForceNextFloorCheck = !MovementBase->IsQueryCollisionEnabled()
				|| (MovementBase->GetCollisionResponseToChannel(CollisionChannel) != ECollisionResponse::ECR_Block)
				|| MovementBaseUtility::IsDynamicBase(MovementBase);
			}

			const bool bActorBasePendingKill = BaseActor && !IsValid(BaseActor);

			if (!bForceNextFloorCheck && !bActorBasePendingKill && MovementBase)
			{
				FLog(Display, "Skipping. Floor Sweep not required")
				
				OutFloorResult = CurrentFloor;
				bNeedToValidateFloor = false;
			} // Don't need to perform a sweep, continue with current floor value
			else
			{
				MutableThis->bForceNextFloorCheck = false;
				ComputeFloorDist(CapsuleLocation, FloorLineTraceDist, FloorSweepTraceDist, OutFloorResult, CapsuleRadius, DownwardSweepResult);
			} // Explicitly perform a sweep test
		} // Check conditions before performing a floor sweep
	}
	
	/* Check ledge perching result on the compute floor (only if floor computed from shape trace) */
	if (bNeedToValidateFloor && OutFloorResult.bBlockingHit && !OutFloorResult.bLineTrace)
	{
		if (ShouldComputePerchResult(OutFloorResult.HitResult))
		{
			float MaxPerchFloorDist = FMath::Max(MAX_FLOOR_DIST, FMath::Max(ExtraFloorProbingDistance, MaxStepHeight) + HeightCheckAdjust);
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
		bBlockingHit = GetWorld()->SweepSingleByChannel(OutHit, Start, End, FQuat(-1.f * GetUpOrientation(MODE_PawnUp), UE_PI * 0.25f), TraceChannel, BoxShape, Params, ResponseParams);
		
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
		LOG_HIT(Hit, 2);
		
		/* Reject unwalkable floor normals */
		if (!IsFloorStable(Hit))
		{
			return false;
		}

		const float PawnRadius = GetCapsuleRadius();
		const float PawnHalfHeight = GetCapsuleHalfHeight();

		/* Reject hits that are above our lower hemisphere (can happen when sliding down a vertical surface) */
		const float HitImpactHeight = (Hit.ImpactPoint | GetUpOrientation(MODE_PawnUp));
		const float LowerHemisphereHeight = (Hit.Location | GetUpOrientation(MODE_PawnUp)) - PawnHalfHeight + PawnRadius;
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
		// TODO: Right orientation? This is for penetration anyways...
		/* Normal is nearly horizontal or downward, that's a penetration adjustment next to a vertical or overhanging wall. Don't pop to the floor */
		if ((Hit.ImpactNormal | GetUpOrientation(MODE_Gravity)) < UE_KINDA_SMALL_NUMBER) 
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

// DONE
bool UCustomMovementComponent::ShouldCheckForValidLandingSpot(const FHitResult& Hit) const
{
	LOG_HIT(Hit, 2);
	/* See if we hit an edge of a surface on the lower portion of the capsule.
	 * In this case the normal will not equal the impact normal and a downward sweep may find a walkable surface on top of the edge
	 */
	const FVector PawnUp = GetUpOrientation(MODE_PawnUp); // DEBUG: Pawn up should be the right thing here
	if ((Hit.Normal | PawnUp) > UE_KINDA_SMALL_NUMBER && !Hit.Normal.Equals(Hit.ImpactNormal))
	{
		const FVector PawnLocation = UpdatedComponent->GetComponentLocation();
		if (IsWithinEdgeTolerance(PawnLocation, Hit.ImpactPoint, GetCapsuleRadius()))
		{
			return true;
		}
	}
	return false;
}

// DEBUG:
void UCustomMovementComponent::MaintainHorizontalGroundVelocity()
{
	// BUG: Causes a significant increase in velocity when landing at times, very significant especially with stairs (behavior resolved when just setting V.z = 0 (THIS IS RIGHT, SO LOOK FOR THE PROBLEM ELSEWHERE)
	const float VelMag = Velocity.Size();//FVector::VectorPlaneProject(Velocity, CurrentFloor.HitResult.Normal).Size();
	Velocity = GetDirectionTangentToSurface(Velocity, CurrentFloor.HitResult.Normal).GetSafeNormal() * VelMag;
}

#pragma endregion Ground Stability Handling

#pragma region Step Handling

// DEBUG:
bool UCustomMovementComponent::StepUp(const FVector& Orientation, const FHitResult& StepHit, const FVector& Delta, FStepDownFloorResult* OutStepDownResult)
{
	SCOPE_CYCLE_COUNTER(STAT_StepUp)

	/* Ensure we can step up */
	if (!CanStepUp(StepHit) || MaxStepHeight <= 0.f) return false;

	/* Get Pawn Location And Properties */
	const FVector OldLocation = UpdatedComponent->GetComponentLocation();
	const float PawnRadius = GetCapsuleRadius();
	const float PawnHalfHeight = GetCapsuleHalfHeight();

	/* Don't step up if top of the capsule is what recieved the hit */
	const FVector InitialImpactPoint = StepHit.ImpactPoint;
	if (DistanceAlongAxis(OldLocation, InitialImpactPoint, Orientation) > (PawnHalfHeight - PawnRadius)) // -Radius, want center of top hemisphere
	{
		return false;
	}

	/* Step Parameter Setup */
	float StepTravelUpHeight = MaxStepHeight;
	float StepTravelDownHeight = StepTravelUpHeight;
	const float StepSideP = StepHit.ImpactNormal | Orientation;
	float PawnInitialFloorBaseP = (OldLocation | Orientation) - PawnHalfHeight;
	float PawnFloorPointP = PawnInitialFloorBaseP;

	/**/
	if (IsMovingOnGround() && CurrentFloor.IsWalkableFloor())
	{
		const float FloorDist = FMath::Max(0.f, CurrentFloor.GetDistanceToFloor()); // TODO: Check if DistToFloor is valid...
		PawnInitialFloorBaseP -= FloorDist;
		StepTravelUpHeight = FMath::Max(StepTravelUpHeight - FloorDist, 0.f);
		StepTravelDownHeight = (MaxStepHeight + 2.f * MAX_FLOOR_DIST);

		// TODO: Check IsWithinEdgeTolerance...
		const bool bHitVerticalFace = !IsWithinEdgeTolerance(StepHit.Location, StepHit.ImpactPoint, PawnRadius);
		if (!CurrentFloor.bLineTrace && !bHitVerticalFace)
		{
			PawnFloorPointP = CurrentFloor.HitResult.ImpactPoint | Orientation;
		}
		else
		{
			PawnFloorPointP -= CurrentFloor.FloorDist;
		}
	}

	/* Don't step up if impact is below us */
	if (DistanceAlongAxis(PawnInitialFloorBaseP * Orientation, InitialImpactPoint, Orientation) <= 0.f)
	{
		return false;
	}

	/* Scope our move updates */
	FScopedMovementUpdate ScopedStepUpMovement(UpdatedComponent, EScopedUpdate::DeferredUpdates);

	// ~~~~~ Actual Move ~~~~~ //
	
	/* Step Up*/
	FHitResult SweepUpHit(1.f);
	const FQuat PawnRotation = UpdatedComponent->GetComponentQuat();
	MoveUpdatedComponent(Orientation * StepTravelUpHeight, PawnRotation, true, &SweepUpHit);

	if (SweepUpHit.bStartPenetrating)
	{
		ScopedStepUpMovement.RevertMove();
		return false;
	}

	/* Step Forward */
	FHitResult Hit(1.f);
	MoveUpdatedComponent(Delta, PawnRotation, true, &Hit);

	/* Eval results */
	if (Hit.bBlockingHit)
	{
		
		if (Hit.bStartPenetrating)
		{
			ScopedStepUpMovement.RevertMove();
			return false;
		}

		// Notify if we hit something above and ahead of us
		if (SweepUpHit.bBlockingHit && Hit.bBlockingHit)
		{
			HandleImpact(SweepUpHit);
		}

		// Pawn ran into a wall
		HandleImpact(Hit);
		if (IsFalling()) return true;

		// Adjust and try again
		const float ForwardHitTime = Hit.Time;
		const float ForwardSlideAmount = SlideAlongSurface(Delta, 1.f-Hit.Time, Hit.Normal, Hit, true);

		if (IsFalling())
		{
			ScopedStepUpMovement.RevertMove();
			return false; 
		}

		// If both forward and deflection hits go us nowhere, no point in this step up
		if (ForwardHitTime == 0.f && ForwardSlideAmount == 0.f)
		{
			ScopedStepUpMovement.RevertMove();
			return false;
		}
	}

	/* Step Down */
	MoveUpdatedComponent(-Orientation * StepTravelDownHeight, PawnRotation, true, &Hit);

	/* Eval results */

	// If in penetration, abort
	if (Hit.bStartPenetrating)
	{
		ScopedStepUpMovement.RevertMove();
		return false;
	}

	FStepDownFloorResult StepDownResult;
	if (Hit.IsValidBlockingHit())
	{
		// Check if step down results in a step height larger than the max
		const float DeltaH = (Hit.ImpactPoint | Orientation) - PawnFloorPointP;

		// DEBUG:
		if (DeltaH > MaxStepHeight)
		{
			FLog(Warning, "Failed Step Up w/ Height %.3f", DeltaH);
			ScopedStepUpMovement.RevertMove();
			return false;
		}
		else if (FMath::Abs(DeltaH) < 1.f)
		{
			FLog(Error, "Extremely Small Delta w/ Height %.3f", DeltaH);

		}
		else
		{
			FLog(Warning, "Succesful Step Up w/ Height %.3f", DeltaH)
		}
		
		// Reject unstable surface normals
		if (!IsFloorStable(Hit))
		{
			// Reject if normals oppose move direction
			const bool bNormalTowardsMe = (Delta | Hit.ImpactNormal) < 0.f;
			if (bNormalTowardsMe)
			{
				ScopedStepUpMovement.RevertMove();
				return false;
			}

			// Also reject if we would end up being higher than our starting location by stepping down
			if (DistanceAlongAxis(OldLocation, Hit.Location, Orientation) > 0.f)
			{
				ScopedStepUpMovement.RevertMove();
				return false;
			}
		}

		// Reject moves where downward sweep hit something close to edge of capsule
		if (!IsWithinEdgeTolerance(Hit.Location, Hit.ImpactPoint, PawnRadius))
		{
			ScopedStepUpMovement.RevertMove();
			return false;
		}

		// Don't step up onto invalid surfaces if traveling higher
		if (DeltaH > 0.f && !CanStepUp(Hit))
		{
			ScopedStepUpMovement.RevertMove();
			return false;
		}

		// See if we can validate the floor as a result of the step down
		if (OutStepDownResult != nullptr) // DEBUG: Gonna try to throw away the step down result for now
		{
			FindFloor(UpdatedComponent->GetComponentLocation(), StepDownResult.FloorResult, false, &Hit);

			// Reject unwalkable normals if we end up higher than our initial height (ok if we end up lower tho)
			if (DistanceAlongAxis(OldLocation, Hit.Location, Orientation) > 0.f)
			{
				const float MAX_STEP_SIDE_H = 0.08f; // DEBUG: Oh they're referring to normals of the step hit here...
				if (!StepDownResult.FloorResult.bBlockingHit)// && StepSideP < MAX_STEP_SIDE_H)
				{
					FLog(Warning, "Failed Step Up Due To Normal w/ Height  %.3f", DeltaH);
					ScopedStepUpMovement.RevertMove();
					return false;
				}
			}
			
			StepDownResult.bComputedFloor = true;
		}
	}

	// Copy step down result
	if (OutStepDownResult != nullptr)
	{
		*OutStepDownResult = StepDownResult;
	}

	// Don't recalculate velocity based on this height adjustment if considering vertical adjustment
	bJustTeleported = true;

	LOG_HIT(Hit, 3.f);
	
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


#pragma endregion Step Handling

#pragma region Ledge Handling

// TODO: Add 2 denivelation angles, one for when going "down", one for when going "up"
bool UCustomMovementComponent::ShouldCatchAir(const FGroundingStatus& OldFloor, const FGroundingStatus& NewFloor)
{
	const float Angle = FMath::RadiansToDegrees(FMath::Acos(OldFloor.HitResult.ImpactNormal | NewFloor.HitResult.ImpactNormal));
	if (Angle >= MaxStableDenivelationAngle)
	{
		return true;
	}
	return false;
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
		const float DistFromCenterSq = FVector::VectorPlaneProject(InHit.ImpactPoint - InHit.Location, GetUpOrientation(MODE_PawnUp)).SizeSquared();
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
	const float PawnRadius = GetCapsuleRadius();
	const float PawnHalfHeight = GetCapsuleHalfHeight();

	const FVector PawnUp = GetUpOrientation(MODE_PawnUp);
	const FVector CapsuleLocation = (bUseFlatBaseForFloorChecks ? InHit.TraceStart : InHit.Location);

	const float InHitAboveBase = FMath::Max(0.f, (InHit.ImpactPoint - (CapsuleLocation - PawnHalfHeight * PawnUp)) | PawnUp);
	const float PerchLineDist = FMath::Max(0.f, InMaxFloorDist - InHitAboveBase);
	const float PerchSweepDist = FMath::Max(0.f, InMaxFloorDist);

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
	
	return true;
}


bool UCustomMovementComponent::CheckLedgeDirection(const FVector& OldLocation, const FVector& SideStep,
	const FVector& GravDir)
{
	const FVector SideDest = OldLocation + SideStep;
	FCollisionQueryParams CapsuleParams(SCENE_QUERY_STAT(CheckLedgeDirection), false, PawnOwner);
	FCollisionResponseParams ResponseParams;
	InitCollisionParams(CapsuleParams, ResponseParams);
	const FCollisionShape CapsuleShape = UpdatedPrimitive->GetCollisionShape();
	const ECollisionChannel CollisionChannel = UpdatedComponent->GetCollisionObjectType();
	FHitResult Result(1.f);
	GetWorld()->SweepSingleByChannel(Result, OldLocation, SideDest, FQuat::Identity, CollisionChannel, CapsuleShape, CapsuleParams, ResponseParams);
	
	if (!Result.bBlockingHit || IsFloorStable(Result))
	{
	    if (!Result.bBlockingHit)
	    {
			GetWorld()->SweepSingleByChannel(Result, SideDest, SideDest + GravDir * (MaxStepHeight + LedgeCheckThreshold), FQuat::Identity, CollisionChannel, CapsuleShape, CapsuleParams, ResponseParams);
	    }
	    if ((Result.Time < 1.f) && IsFloorStable(Result))
	    {
			return true;
	    }
	}

	return false;
}

FVector UCustomMovementComponent::GetLedgeMove(const FVector& OldLocation, const FVector& Delta,
	const FVector& GravityDir)
{
	if (Delta.IsZero()) return FVector::ZeroVector;

	// Establish a basis based on GravityDir & Pawn Forward
	const FVector PawnForward = UpdatedComponent->GetForwardVector();
	
	const FVector BasisUp = -GravityDir.GetSafeNormal();
	const FVector BasisRight = (Delta ^ (-GravityDir)).GetSafeNormal();
	const FVector BasisForward = (BasisUp ^ BasisRight).GetSafeNormal();
	
	FVector SideDir = Delta.ProjectOnToNormal(BasisRight) - Delta.ProjectOnToNormal(BasisForward);

	// try left
	if (CheckLedgeDirection(OldLocation, SideDir, GravityDir)) return SideDir;

	// Try right
	SideDir *= -1.f;
	if (CheckLedgeDirection(OldLocation, SideDir, GravityDir)) return SideDir;

	return FVector::ZeroVector;
}

void UCustomMovementComponent::HandleWalkingOffLedge(const FVector& PreviousFloorImpactNormal,
	const FVector& PreviousFloorContactNormal, const FVector& PreviousLocation, float TimeDelta)
{
}


#pragma endregion Ledge Handling

#pragma region Collision Adjustments

FVector UCustomMovementComponent::GetPenetrationAdjustment(const FHitResult& Hit) const
{
	FVector Result = Super::GetPenetrationAdjustment(Hit);

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
	bJustTeleported |= Super::ResolvePenetrationImpl(Adjustment, Hit, NewRotation);
	return bJustTeleported;
}

void UCustomMovementComponent::HandleImpact(const FHitResult& Hit, float TimeSlice, const FVector& MoveDelta)
{
	SCOPE_CYCLE_COUNTER(STAT_HandleImpact)
	
	// Notify other pawn
	if (Hit.HitObjectHandle.DoesRepresentClass(APawn::StaticClass()))
	{
		APawn* OtherPawn = Hit.HitObjectHandle.FetchActor<APawn>();
		NotifyBumpedPawn(OtherPawn);
	}

	if (bEnablePhysicsInteraction)
	{
		const FVector ForceAccel = Acceleration + (IsFalling() ? Gravity * GetGravityZ() : FVector::ZeroVector);
		ApplyImpactPhysicsForces(Hit, ForceAccel, Velocity);
	}
}

float UCustomMovementComponent::SlideAlongSurface(const FVector& Delta, float Time, const FVector& InNormal, FHitResult& Hit, bool bHandleImpact)
{
	SCOPE_CYCLE_COUNTER(STAT_SlideAlongSurface)
	
	if (!Hit.bBlockingHit) return 0.f;
	
	FVector Normal(InNormal);
	if (IsMovingOnGround())
	{
		int VerticalNormal = FMath::Sign(Normal | CurrentFloor.HitResult.ImpactNormal);

		if (VerticalNormal > 0) // Don't push up an unwalkable surface (Might not be necessary since we expect gravity to handle this and don't wanna treat them as barriers)
		{
			
		}
		else if (VerticalNormal < 0) // Don't push down into the floor when impact is on upper portion of the capsule
		{
			if (CurrentFloor.FloorDist < MIN_FLOOR_DIST && CurrentFloor.bBlockingHit)
			{
				const FVector FloorNormal = CurrentFloor.HitResult.Normal;
				const bool bFloorOpposedToMovement = (Delta | FloorNormal) < 0.f;
				// There's a second check here for if the floor normal isn't perfectly vertical, which makes sense as in CMC case that could be
				// considered a barrier, but in our case we project our deltas to the normals (&& bFloorOpposedToMovement)
				if (bFloorOpposedToMovement)
				{
					Normal  = FloorNormal;
				}

				Normal = FVector::VectorPlaneProject(Normal, FloorNormal).GetSafeNormal();
			}
		}
	}
	

	return Super::SlideAlongSurface(Delta, Time, Normal, Hit, bHandleImpact);
}

void UCustomMovementComponent::TwoWallAdjust(FVector& Delta, const FHitResult& Hit, const FVector& OldHitNormal) const
{
	SCOPE_CYCLE_COUNTER(STAT_TwoWallAdjust)
	
	const FVector InDelta = Delta;
	Super::TwoWallAdjust(Delta, Hit, OldHitNormal);

	if (IsMovingOnGround())
	{
		/* Allow slides up walkable surfaces, but not unwalkable ones (Treat those as vertical barriers) */

		const FVector FloorNormal = CurrentFloor.HitResult.Normal;
		const int VerticalDelta = FMath::Sign(Delta | FloorNormal);
		
		if (VerticalDelta > 0) // Adjusted Delta Pushing Up (Might not be necessary for the same reason as SlideAlongSurfaces
		{
			
		}
		else if (VerticalDelta < 0) // Adjusted Delta Pushing Down, don't wanna push down into the floor
		{
			if (CurrentFloor.FloorDist < MIN_FLOOR_DIST && CurrentFloor.bBlockingHit)
			{
				Delta = FVector::VectorPlaneProject(Delta, FloorNormal);
			}
		}
	}
}


#pragma endregion Collision Adjustments

#pragma region Root Motion
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

void UCustomMovementComponent::TickPose(float DeltaTime)
{
	SCOPE_CYCLE_COUNTER(STAT_TickPose)

	/*
	UAnimMontage* RootMotionMontage = nullptr;
	float RootMotionMontagePosition = -1.f;
	if (const auto RootMotionMontageInstance = GetRootMotionMontageInstance())
	{
		RootMotionMontage = RootMotionMontageInstance->Montage;
		RootMotionMontagePosition = RootMotionMontageInstance->GetPosition();
	}
	
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
*/

	UAnimMontage* RootMotionMontage = nullptr;
	float RootMotionMontagePosition = -1.f;
	if (const auto RootMotionMontageInstance = GetRootMotionMontageInstance())
	{
		RootMotionMontage = RootMotionMontageInstance->Montage;
		RootMotionMontagePosition = RootMotionMontageInstance->GetPosition();
	}
	
	bool bWasPlayingRootMotion = SkeletalMesh->IsPlayingRootMotion();

	SkeletalMesh->TickPose(DeltaTime, true);

	// Grab Root Motion Now That We Have Ticked The Pose
	if (SkeletalMesh->IsPlayingRootMotion() && bWasPlayingRootMotion)
	{
		FRootMotionMovementParams RootMotion = SkeletalMesh->ConsumeRootMotion();
		if (RootMotion.bHasRootMotion)
		{
			RootMotionParams.Accumulate(RootMotion);
		}
	}
	
	if (HasAnimRootMotion())
	{
		if (ShouldDiscardRootMotion(RootMotionMontage, RootMotionMontagePosition))
		{
			RootMotionParams = FRootMotionMovementParams();
			return;
		}
		
		// Convert local space root motion to world space before physics
		RootMotionParams.Set(SkeletalMesh->ConvertLocalRootMotionToWorld(RootMotionParams.GetRootMotionTransform()));

		if (DeltaTime > 0.f)
		{
			Velocity = CalcRootMotionVelocity(RootMotionParams.GetRootMotionTransform().GetTranslation(), DeltaTime, Velocity);
		}
	}
}

FVector UCustomMovementComponent::CalcRootMotionVelocity(FVector RootMotionDeltaMove, float DeltaTime, const FVector& CurrentVelocity) const
{
	// Ignore components with very small delta values
	RootMotionDeltaMove.X = FMath::IsNearlyEqual(RootMotionDeltaMove.X, 0.f, 0.01f) ? 0.f : RootMotionDeltaMove.X;
	RootMotionDeltaMove.Y = FMath::IsNearlyEqual(RootMotionDeltaMove.Y, 0.f, 0.01f) ? 0.f : RootMotionDeltaMove.Y;
	RootMotionDeltaMove.Z = FMath::IsNearlyEqual(RootMotionDeltaMove.Z, 0.f, 0.01f) ? 0.f : RootMotionDeltaMove.Z;

	const FVector RootMotionVelocity = RootMotionDeltaMove / DeltaTime;
	return RootMotionVelocity; // TODO: Post process event
}

FAnimMontageInstance* UCustomMovementComponent::GetRootMotionMontageInstance() const
{
	if (!SkeletalMesh) return nullptr;

	
	const auto AnimInstance = SkeletalMesh->GetAnimInstance();
	if (!AnimInstance) return nullptr;
	
	return AnimInstance->GetRootMotionMontageInstance();
}

bool UCustomMovementComponent::ShouldDiscardRootMotion(UAnimMontage* RootMotionMontage, float RootMotionMontagePosition) const
{
	const float MontageLength = RootMotionMontage->GetPlayLength();

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


/*
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

*/
#pragma endregion Root Motion

#pragma region Physics Interactions

// TODO: Review this, its pretty old by now
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
	
	const FVector UpOrientation = GetUpOrientation(MODE_Gravity);
	
	const FVector OtherLoc = OtherComponent->GetComponentLocation();
	const FVector Loc = UpdatedComponent->GetComponentLocation();
	
	FVector ImpulseDir = (FVector::VectorPlaneProject(OtherLoc - Loc, UpOrientation) + 0.25f*UpOrientation).GetSafeNormal();
	ImpulseDir = (ImpulseDir + FVector::VectorPlaneProject(Velocity, UpOrientation).GetSafeNormal()) * 0.5f;
	ImpulseDir.Normalize();

	FName BoneName = NAME_None;
	if (OtherBodyIndex != INDEX_NONE)
	{
		BoneName = Cast<USkinnedMeshComponent>(OtherComponent)->GetBoneName(OtherBodyIndex);
	}

	float TouchForceFactorModified = TouchForceFactor;

	if (bTouchForceScaledToMass)
	{
		FBodyInstance* BI = OtherComponent->GetBodyInstance(BoneName);
		TouchForceFactorModified *= BI ? BI->GetBodyMass() : 1.f;
	}

	float ImpulseStrength = FMath::Clamp<FVector::FReal>(FVector::VectorPlaneProject(Velocity, UpOrientation).Size() * TouchForceFactorModified,
														MinTouchForce > 0.f ? MinTouchForce : -FLT_MAX,
														MaxTouchForce > 0.f ? MaxTouchForce : FLT_MAX);

	FVector Impulse = ImpulseDir * ImpulseStrength;
	OtherComponent->AddImpulse(Impulse, BoneName);
}

void UCustomMovementComponent::ApplyImpactPhysicsForces(const FHitResult& Impact, const FVector& ImpactAcceleration, const FVector& ImpactVelocity)
{
	if (bEnablePhysicsInteraction && Impact.bBlockingHit)
	{
		if (UPrimitiveComponent* ImpactComponent = Impact.GetComponent())
		{
			FVector ForcePoint = Impact.ImpactPoint;
			float BodyMass = 1.f; // Set to 1 as this is used as a multiplier

			bool bCanBePushed = false;
			FBodyInstance* BI = ImpactComponent->GetBodyInstance(Impact.BoneName);
			if (BI != nullptr && BI->IsInstanceSimulatingPhysics())
			{
				BodyMass = FMath::Max(BI->GetBodyMass(), 1.f);

				if (bPushForceUsingVerticalOffset)
				{
					FBox Bounds = BI->GetBodyBounds();

					FVector Center, Extents;
					Bounds.GetCenterAndExtents(Center, Extents);

					if (!Extents.IsNearlyZero())
					{
						const FVector ForcePointPlanar = FVector::VectorPlaneProject(ForcePoint, GetUpOrientation(MODE_Gravity));
						const FVector ForcePointVertical = (Center + Extents * PushForcePointVerticalOffsetFactor).ProjectOnToNormal(GetUpOrientation(MODE_Gravity));
						ForcePoint = ForcePointPlanar + ForcePointVertical;
					}
				}

				bCanBePushed = true;
			}

			if (bCanBePushed)
			{
				FVector Force = Impact.ImpactNormal * -1.f;

				float PushForceModificator = 1.f;

				const FVector ComponentVelocity = ImpactComponent->GetPhysicsLinearVelocity();
				// TODO: Implement GetMaxSpeed
				const FVector VirtualVelocity = ImpactAcceleration.IsZero() ? ImpactVelocity : ImpactAcceleration.GetSafeNormal() * Velocity.ProjectOnToNormal(GetUpOrientation(MODE_Gravity)).Size();

				float Dot = 0.f;

				if (bScalePushForceToVelocity && !ComponentVelocity.IsNearlyZero())
				{
					Dot = ComponentVelocity | VirtualVelocity;

					if (Dot > 0.f && Dot < 1.f)
					{
						PushForceModificator *= Dot;
					}
				}

				if (bPushForceScaledToMass)
				{
					PushForceModificator *= BodyMass;
				}

				Force *= PushForceModificator;
				const float ZeroVelocityTolerance = 1.f;
				if (ComponentVelocity.IsNearlyZero(ZeroVelocityTolerance))
				{
					Force *= InitialPushForceFactor;
					ImpactComponent->AddImpulseAtLocation(Force, ForcePoint, Impact.BoneName);
				}
				else
				{
					Force *= PushForceFactor;
					ImpactComponent->AddForceAtLocation(Force, ForcePoint, Impact.BoneName);
				}
			}
		}
	}
}

void UCustomMovementComponent::ApplyDownwardForce(float DeltaTime)
{
	if (StandingDownwardForceScale != 0.0f && CurrentFloor.HitResult.IsValidBlockingHit())
	{
		UPrimitiveComponent* BaseComp = CurrentFloor.HitResult.GetComponent();
		const FVector SetGravity = -GetUpOrientation(MODE_Gravity) * GetGravityZ();

		if (BaseComp && BaseComp->IsAnySimulatingPhysics() && !SetGravity.IsZero())
		{
			BaseComp->AddForceAtLocation(SetGravity * Mass * StandingDownwardForceScale, CurrentFloor.HitResult.ImpactPoint, CurrentFloor.HitResult.BoneName);
		}
	}
}

void UCustomMovementComponent::ApplyRepulsionForce(float DeltaTime)
{
	if (UpdatedPrimitive && RepulsionForce > 0.f && PawnOwner != nullptr)
	{
		const TArray<FOverlapInfo>& Overlaps = UpdatedPrimitive->GetOverlapInfos();
		if (Overlaps.Num() > 0)
		{
			FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(RMC_ApplyRepulsionForce));
			QueryParams.bReturnFaceIndex = false;
			QueryParams.bReturnPhysicalMaterial = false;

			float CapsuleRadius = GetCapsuleRadius();
			float CapsuleHalfHeight = GetCapsuleHalfHeight();

			const float RepulsionForceRadius = CapsuleRadius * 1.2f;
			const float StopBodyDistance = 2.5f;
			const FVector MyLocation = UpdatedPrimitive->GetComponentLocation();

			for (int32 i = 0; i < Overlaps.Num(); i++)
			{
				const FOverlapInfo& Overlap = Overlaps[i];

				UPrimitiveComponent* OverlapComp = Overlap.OverlapInfo.Component.Get();
				if (!OverlapComp || OverlapComp->Mobility < EComponentMobility::Movable)
				{
					continue;
				}

				// Use the body instead of component for cases where we have multi-body overlaps enabled
				FBodyInstance* OverlapBody = nullptr;
				const int32 OverlapBodyIndex = Overlap.GetBodyIndex();
				const USkeletalMeshComponent* SkelMeshForBody = (OverlapBodyIndex != INDEX_NONE) ? Cast<USkeletalMeshComponent>(OverlapComp) : nullptr;
				if (SkelMeshForBody != nullptr)
				{
					OverlapBody = SkelMeshForBody->Bodies.IsValidIndex(OverlapBodyIndex) ? SkelMeshForBody->Bodies[OverlapBodyIndex] : nullptr;
				}
				else
				{
					OverlapBody = OverlapComp->GetBodyInstance();
				}

				if (!OverlapBody)
				{
					FLog(Warning, "%s Could not find overlap body for body index %d", *GetName(), OverlapBodyIndex);
					continue;
				}

				if (!OverlapBody->IsInstanceSimulatingPhysics())
				{
					continue;
				}

				FTransform BodyTransform = OverlapBody->GetUnrealWorldTransform();

				FVector BodyVelocity = OverlapBody->GetUnrealWorldVelocity();
				FVector BodyLocation = BodyTransform.GetLocation();

				// Trace to get the hit location on the capsule
				FVector EndTraceLocation = FVector::VectorPlaneProject(MyLocation, GetUpOrientation(MODE_Gravity)) + BodyLocation.ProjectOnToNormal(GetUpOrientation(MODE_Gravity));
				FHitResult Hit;
				bool bHasHit = UpdatedPrimitive->LineTraceComponent(Hit, BodyLocation, EndTraceLocation, QueryParams);

				FVector HitLoc = Hit.ImpactPoint;
				bool bIsPenetrating = Hit.bStartPenetrating || Hit.PenetrationDepth > StopBodyDistance;

				// If we didn't hit the capsule, we're inside the capsule
				if (!bHasHit)
				{
					HitLoc = BodyLocation;
					bIsPenetrating = true;
				}

				const float DistanceNow = FVector::VectorPlaneProject((HitLoc - BodyLocation), GetUpOrientation(MODE_Gravity)).Size();
				const float DistanceLater = FVector::VectorPlaneProject((HitLoc - (BodyLocation + BodyVelocity * DeltaTime)), GetUpOrientation(MODE_Gravity)).Size();

				if (bHasHit && DistanceNow < StopBodyDistance && !bIsPenetrating)
				{
					OverlapBody->SetLinearVelocity(FVector::ZeroVector, false);
				}
				else if (DistanceLater <= DistanceNow || bIsPenetrating)
				{
					FVector ForceCenter = MyLocation;
					const FVector ForceCenterPlanar = FVector::VectorPlaneProject(ForceCenter, GetUpOrientation(MODE_Gravity));
					FVector ForceCenterVertical = ForceCenter.ProjectOnToNormal(GetUpOrientation(MODE_Gravity));
					if (bHasHit)
					{
						ForceCenterVertical = HitLoc.ProjectOnToNormal(GetUpOrientation(MODE_Gravity));
					}
					else
					{
						FVector BodyLocationVert = BodyLocation.ProjectOnToNormal(GetUpOrientation(MODE_Gravity));
						FVector MyLocationVert = MyLocation.ProjectOnToNormal(GetUpOrientation(MODE_Gravity));
						const FVector UpOrientation = GetUpOrientation(MODE_Gravity);
						ForceCenterVertical = (BodyLocationVert).GetClampedToSize((MyLocationVert - CapsuleHalfHeight * UpOrientation).Size(), (MyLocation + CapsuleHalfHeight * UpOrientation).Size());
					}
					ForceCenter = ForceCenterPlanar + ForceCenterVertical;
					OverlapBody->AddRadialForceToBody(ForceCenter, RepulsionForceRadius, RepulsionForce * Mass, ERadialImpulseFalloff::RIF_Constant);
				}

			}
		}
	}
}


#pragma endregion Physics Interactions

#pragma region Moving Base


void UCustomMovementComponent::SetBase(UPrimitiveComponent* NewBaseComponent, const FName InBoneName, bool bNotifyPawn)
{
	/* If new base component is nullptr, ignore bone name */
	const FName BoneName = (NewBaseComponent ? InBoneName : NAME_None);

	/* See what has changed */
	const bool bBaseChanged = (NewBaseComponent != BasedMovement.MovementBase);
	const bool bBoneChanged = (BoneName != BasedMovement.BoneName);

	if (bBaseChanged || bBoneChanged)
	{
		/* Verify no recursion */
		APawn* Loop = (NewBaseComponent ? Cast<APawn>(NewBaseComponent->GetOwner()): nullptr);
		while (Loop)
		{
			if (Loop == PawnOwner) return;

			if (UPrimitiveComponent* LoopBase = Loop->GetMovementBase())
			{
				Loop = Cast<APawn>(LoopBase->GetOwner());
			}
			else
			{
				break;
			}
		}

		/* Set Base */
		UPrimitiveComponent* OldBase = BasedMovement.MovementBase;
		BasedMovement.MovementBase = NewBaseComponent;
		BasedMovement.BoneName = BoneName;

		/* Set tick dependencies */
		const bool bBaseIsSimulating = MovementBaseUtility::IsSimulatedBase(NewBaseComponent);
		if (bBaseChanged)
		{
			MovementBaseUtility::RemoveTickDependency(PrimaryComponentTick, OldBase);
			/* Use special post physics function if simulating, otherwise add normal tick prereqs. */
			if (!bBaseIsSimulating)
			{
				MovementBaseUtility::AddTickDependency(PrimaryComponentTick, NewBaseComponent);
			}
		}

		if (NewBaseComponent)
		{
			SaveBaseLocation();
			PostPhysicsTickFunction.SetTickFunctionEnable(bBaseIsSimulating);
		}
		else
		{
			BasedMovement.BoneName = NAME_None;
			BasedMovement.bRelativeRotation = false;
			CurrentFloor.Clear();
			PostPhysicsTickFunction.SetTickFunctionEnable(false);
		}
	}
}

void UCustomMovementComponent::SetBaseFromFloor(const FGroundingStatus& FloorResult)
{
	if (FloorResult.IsWalkableFloor())
	{
		SetBase(FloorResult.HitResult.GetComponent(), FloorResult.HitResult.BoneName);
	}
	else
	{
		SetBase(nullptr);
	}
}

UPrimitiveComponent* UCustomMovementComponent::GetMovementBase() const
{
	return BasedMovement.MovementBase;
}

// TODO: Math is right I think (PawnUp to define impart plane)
FVector UCustomMovementComponent::GetImpartedMovementBaseVelocity() const
{
	if (!bMoveWithBase) return FVector::ZeroVector;
	
	FVector Result = FVector::ZeroVector;

	UPrimitiveComponent* MovementBase = BasedMovement.MovementBase;
	if (MovementBaseUtility::IsDynamicBase(MovementBase))
	{
		FVector BaseVelocity = MovementBaseUtility::GetMovementBaseVelocity(MovementBase, BasedMovement.BoneName);

		if (bImpartBaseAngularVelocity)
		{
			const FVector BasePointPosition = (UpdatedComponent->GetComponentLocation() - GetUpOrientation(MODE_FloorNormal) * UpdatedPrimitive->GetCollisionShape().GetCapsuleHalfHeight());
			const FVector BaseTangentialVel = MovementBaseUtility::GetMovementBaseTangentialVelocity(MovementBase, BasedMovement.BoneName, BasePointPosition);
			BaseVelocity += BaseTangentialVel;
		}
		
		if (bImpartBaseVelocityPlanar)
		{
			Result += FVector::VectorPlaneProject(BaseVelocity, GetUpOrientation(MODE_PawnUp));
		}

		if (bImpartBaseVelocityVertical)
		{
			Result += BaseVelocity.ProjectOnToNormal(GetUpOrientation(MODE_PawnUp));
		}
	}

	return Result;
}

void UCustomMovementComponent::DecayFormerBaseVelocity(float DeltaTime)
{
	if (!bMoveWithBase) return;
	
	if (FormerBaseVelocityDecayHalfLife == 0.f)
	{
		DecayingFormerBaseVelocity = FVector::ZeroVector;
	}
	else
	{
		DecayingFormerBaseVelocity *= FMath::Exp2(-DeltaTime * 1.f / FormerBaseVelocityDecayHalfLife);
	}
}


void UCustomMovementComponent::TryUpdateBasedMovement(float DeltaTime)
{
	if (!bMoveWithBase) return;
	
	bDeferUpdateBasedMovement = false;

	UPrimitiveComponent* MovementBase = GetMovementBase();
	if (MovementBaseUtility::UseRelativeLocation(MovementBase))
	{
		if (!MovementBaseUtility::IsSimulatedBase(MovementBase))
		{
			bDeferUpdateBasedMovement = false;
			UpdateBasedMovement(DeltaTime);
			// If previously simulated, go back to using normal tick dependencies
			if (PostPhysicsTickFunction.IsTickFunctionEnabled())
			{
				PostPhysicsTickFunction.SetTickFunctionEnable(false);
				MovementBaseUtility::AddTickDependency(PrimaryComponentTick, MovementBase);
			}
		}
		else
		{
			// Defer movement base update until after physics
			bDeferUpdateBasedMovement = true;
			// If previously not simulating, remove tick dependencies and use post physics tick function
			if (!PostPhysicsTickFunction.IsTickFunctionEnabled())
			{
				PostPhysicsTickFunction.SetTickFunctionEnable(true);
				MovementBaseUtility::RemoveTickDependency(PrimaryComponentTick, MovementBase);
			}
		}
	}
	else
	{
		// Remove any previous tick dependencies. SetBase() takes care of the other dependencies
		if (PostPhysicsTickFunction.IsTickFunctionEnabled())
		{
			PostPhysicsTickFunction.SetTickFunctionEnable(false);
		}
	}
}

void UCustomMovementComponent::UpdateBasedMovement(float DeltaTime)
{
	if (!bMoveWithBase) return;
	
	const UPrimitiveComponent* MovementBase = GetMovementBase();
	if (!MovementBaseUtility::UseRelativeLocation(MovementBase))
	{
		return;
	}

	if (!IsValid(MovementBase) || !IsValid(MovementBase->GetOwner()))
	{
		SetBase(nullptr);
		return;
	}

	// Ignore collision with bases during these movement
	TGuardValue<EMoveComponentFlags> ScopedFlagRestore(MoveComponentFlags, MoveComponentFlags | MOVECOMP_IgnoreBases);

	FQuat DeltaQuat = FQuat::Identity;
	FVector DeltaPosition = FVector::ZeroVector;

	FQuat NewBaseQuat;
	FVector NewBaseLocation;
	if (!MovementBaseUtility::GetMovementBaseTransform(MovementBase, BasedMovement.BoneName, NewBaseLocation, NewBaseQuat))
	{
		return;
	}

	// Find change in rotation
	const bool bRotationChanged = !OldBaseQuat.Equals(NewBaseQuat, 1e-8f);
	if (bRotationChanged)
	{
		DeltaQuat = NewBaseQuat * OldBaseQuat.Inverse();
	}

	// Only if base moved
	if (bRotationChanged || (OldBaseLocation != NewBaseLocation))
	{
		// Calculate new transform matrix of base actor (ignoring scale)
		const FQuatRotationTranslationMatrix OldLocalToWorld(OldBaseQuat, OldBaseLocation);
		const FQuatRotationTranslationMatrix NewLocalToWorld(NewBaseQuat, NewBaseLocation);

		FQuat FinalQuat = UpdatedComponent->GetComponentQuat();

		if (bRotationChanged && !bIgnoreBaseRotation)
		{
			// Apply change in rotation and pipe through FaceRotation to maintain axis restrictions
			const FQuat PawnOldQuat = UpdatedComponent->GetComponentQuat();
			const FQuat TargetQuat = DeltaQuat * FinalQuat;
			FRotator TargetRotator(TargetQuat);

			// TODO: Option to only take planar ?
			MoveUpdatedComponent(FVector::ZeroVector, TargetRotator, false);
			FinalQuat = UpdatedComponent->GetComponentQuat();
			
			// Pipe through ControlRotation to affect camera
			if (PawnOwner->Controller)
			{
				const FQuat PawnDeltaRotation = FinalQuat * PawnOldQuat.Inverse();
				FRotator FinalRotation = FinalQuat.Rotator();
				UpdateBasedRotation(FinalRotation, PawnDeltaRotation.Rotator());
				FinalQuat = UpdatedComponent->GetComponentQuat();
			}
		}

		// We need to offset the base of the character here, not its origin, so offset by half height
		float HalfHeight = GetCapsuleHalfHeight();
		float Radius = GetCapsuleRadius();

		const FVector BaseOffset = GetUpOrientation(MODE_PawnUp) * HalfHeight;
		const FVector LocalBasePos = OldLocalToWorld.InverseTransformPosition(UpdatedComponent->GetComponentLocation() - BaseOffset);
		const FVector NewWorldPos = ConstrainLocationToPlane(NewLocalToWorld.TransformPosition(LocalBasePos) + BaseOffset);
		DeltaPosition = ConstrainDirectionToPlane(NewWorldPos - UpdatedComponent->GetComponentLocation());

		// Move attached actor
		FVector BaseMoveDelta = NewBaseLocation - OldBaseLocation;
		if (!bRotationChanged && (BaseMoveDelta.X == 0.f) && (BaseMoveDelta.Y == 0.f))
		{
			DeltaPosition.X = 0.f;
			DeltaPosition.Y = 0.f;
		}

		FHitResult MoveOnBaseHit(1.f);
		const FVector OldLocation = UpdatedComponent->GetComponentLocation();
		MoveUpdatedComponent(DeltaPosition, FinalQuat, true, &MoveOnBaseHit);
		if ((UpdatedComponent->GetComponentLocation() - (OldLocation + DeltaPosition)).IsNearlyZero() == false)
		{
			LOG_HIT(MoveOnBaseHit, 2);
			OnUnableToFollowBaseMove(DeltaPosition, OldLocation, MoveOnBaseHit);
		}

		if (MovementBase->IsSimulatingPhysics() && SkeletalMesh)
		{
			SkeletalMesh->ApplyDeltaToAllPhysicsTransforms(DeltaPosition, DeltaQuat);
		}
	}
	
}

// TODO: How? Is This Right? Keeping Roll?
void UCustomMovementComponent::UpdateBasedRotation(FRotator& FinalRotation, const FRotator& ReducedRotation)
{
	if (!bMoveWithBase) return;
	
	AController* Controller = PawnOwner ? PawnOwner->Controller : nullptr;

	if ((Controller != nullptr) && !bIgnoreBaseRotation)
	{
		FRotator ControllerRot = Controller->GetControlRotation();
		Controller->SetControlRotation(ControllerRot + ReducedRotation);
	}
}

void UCustomMovementComponent::SaveBaseLocation()
{
	if (!bDeferUpdateBasedMovement)
	{
		const UPrimitiveComponent* MovementBase = GetMovementBase();
		if (MovementBase)
		{
			// Read transforms into old base location and quat regardless of whether its movable (because mobility can change)
			MovementBaseUtility::GetMovementBaseTransform(MovementBase, BasedMovement.BoneName, OldBaseLocation, OldBaseQuat);

			if (MovementBaseUtility::UseRelativeLocation(MovementBase))
			{
				// Relative Location
				FVector RelativeLocation;
				MovementBaseUtility::GetLocalMovementBaseLocation(MovementBase, BasedMovement.BoneName, UpdatedComponent->GetComponentLocation(), RelativeLocation);
				
				// Rotation
				if (bIgnoreBaseRotation)
				{
					// Absolute Rotation
					BasedMovement.Location = RelativeLocation;
					BasedMovement.Rotation = UpdatedComponent->GetComponentRotation();
					BasedMovement.bRelativeRotation = false;
				}
				else
				{
					// Relative Rotation
					const FRotator RelativeRotation = (FQuatRotationMatrix(UpdatedComponent->GetComponentQuat()) * FQuatRotationMatrix(OldBaseQuat).GetTransposed()).Rotator();
					BasedMovement.Location = RelativeLocation;
					BasedMovement.Rotation = RelativeRotation;
					BasedMovement.bRelativeRotation = true;
				}
			}
		}
	}
}

void UCustomMovementComponent::OnUnableToFollowBaseMove(const FVector& DeltaPosition, const FVector& OldPosition,
	const FHitResult& MoveOnBaseHit)
{
}

#pragma endregion Moving Base

#pragma region Utility

void UCustomMovementComponent::VisualizeMovement() const
{
	float HeightOffset = 0.f;
	const float OffsetPerElement = 10.0f;

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	const FVector TopOfCapsule = GetActorLocation() + GetUpOrientation(MODE_PawnUp) * GetCapsuleHalfHeight();
	
	// Position
	{
		const FColor DebugColor = FColor::White;
		const FVector DebugLocation = TopOfCapsule + HeightOffset * GetUpOrientation(MODE_PawnUp);
		FString DebugText = FString::Printf(TEXT("Position: %s"), *GetActorLocation().ToCompactString());
		DrawDebugString(GetWorld(), DebugLocation, DebugText, nullptr, DebugColor, 0.f, true);
	}

	// Velocity
	{
		const FColor DebugColor = FColor::Green;
		HeightOffset += OffsetPerElement;
		const FVector DebugLocation = TopOfCapsule + HeightOffset * GetUpOrientation(MODE_PawnUp);
		DrawDebugDirectionalArrow(GetWorld(), DebugLocation - 5.f * GetUpOrientation(MODE_PawnUp), DebugLocation - 5.f * GetUpOrientation(MODE_PawnUp) + Velocity,
			100.f, DebugColor, false, -1.f, (uint8)'\000', 10.f);

		FString DebugText = FString::Printf(TEXT("Velocity: %s (Speed: %.2f) (Max: %.2f)"), *Velocity.ToCompactString(), Velocity.Size(), GetMaxSpeed());
		DrawDebugString(GetWorld(), DebugLocation, DebugText, nullptr, DebugColor, 0.f, true);
	}

	// Acceleration
	{
		const FColor DebugColor = FColor::Yellow;
		HeightOffset += OffsetPerElement;
		const float MaxAccelerationLineLength = 100.f;
		const FVector DebugLocation = TopOfCapsule + HeightOffset * GetUpOrientation(MODE_PawnUp);
		DrawDebugDirectionalArrow(GetWorld(), DebugLocation - 5.f * GetUpOrientation(MODE_PawnUp), 
			DebugLocation - 5.f * GetUpOrientation(MODE_PawnUp) + Acceleration.GetSafeNormal(UE_SMALL_NUMBER) * MaxAccelerationLineLength,
			25.f, DebugColor, false, -1.f, (uint8)'\000', 8.f);

		FString DebugText = FString::Printf(TEXT("Acceleration: %s"), *Acceleration.ToCompactString());
		DrawDebugString(GetWorld(), DebugLocation, DebugText, nullptr, DebugColor, 0.f, true);
	}

	// Movement Mode
	{
		const FColor DebugColor = FColor::Blue;
		HeightOffset += OffsetPerElement;
		FVector DebugLocation = TopOfCapsule + HeightOffset * GetUpOrientation(MODE_PawnUp);
		FString DebugText = FString::Printf(TEXT("MovementMode: %s"), *(PhysicsState.GetValue() == STATE_Grounded ? FString("Grounded") : FString("Aerial")));
		DrawDebugString(GetWorld(), DebugLocation, DebugText, nullptr, DebugColor, 0.f, true);
	}

#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
}

void UCustomMovementComponent::DisplayDebug(UCanvas* Canvas, const FDebugDisplayInfo& DebugDisplay, float& YL, float& YPos)
{
	FDisplayDebugManager& DisplayDebugManager = Canvas->DisplayDebugManager;
	
	DisplayDebugManager.SetDrawColor(FColor::Yellow);
	FString T = FString::Printf(TEXT("----- MOVE STATE -----"));
	DisplayDebugManager.DrawString(T);

	DisplayDebugManager.SetDrawColor(PhysicsState == STATE_Grounded ? FColor::Orange : FColor::Red);

	// Movement Information
	T = FString::Printf(TEXT("Movement State: %s"), (PhysicsState == STATE_Grounded ? TEXT("Grounded") : TEXT("Aerial")));
	DisplayDebugManager.DrawString(T);

	DisplayDebugManager.SetDrawColor(FColor::White);
	T = FString::Printf(TEXT("Velocity: %s"), *Velocity.ToCompactString());
	DisplayDebugManager.DrawString(T);

	T = FString::Printf(TEXT("Acceleration: %s"), *Acceleration.ToCompactString());
	DisplayDebugManager.DrawString(T);

	// Floor Info
	DisplayDebugManager.SetDrawColor(FColor::Yellow);
	T = FString::Printf(TEXT("----- FLOOR INFO -----"));
	DisplayDebugManager.DrawString(T);

	DisplayDebugManager.SetDrawColor(CurrentFloor.bBlockingHit ? FColor::Green : FColor::Red);
	T = FString::Printf(TEXT("Blocking Hit: %s"), BOOL2STR(CurrentFloor.bBlockingHit));
	DisplayDebugManager.DrawString(T);

	if (CurrentFloor.bBlockingHit)
	{
		DisplayDebugManager.SetDrawColor(CurrentFloor.bWalkableFloor ? FColor::Green : FColor::Orange);
		T = FString::Printf(TEXT("Stable Floor: %s"), BOOL2STR(CurrentFloor.bWalkableFloor));
		DisplayDebugManager.DrawString(T);

		DisplayDebugManager.SetDrawColor(FColor::White);
		T = FString::Printf(TEXT("Floor Distance: %f"), CurrentFloor.GetDistanceToFloor());
		DisplayDebugManager.DrawString(T);
	}
	// Move Parameters
	DisplayDebugManager.SetDrawColor(FColor::Yellow);
	T = FString::Printf(TEXT("----- MOVE PARAMETERS -----"));
	DisplayDebugManager.DrawString(T);

	DisplayDebugManager.SetDrawColor(FColor::White);
	T = FString::Printf(TEXT("Perch Radius Threshold: %f"), PerchRadiusThreshold);
	DisplayDebugManager.DrawString(T);

	T = FString::Printf(TEXT("Step Height: %f"), MaxStepHeight);
	DisplayDebugManager.DrawString(T);

	T = FString::Printf(TEXT("Extra Probing Distance: %f"), ExtraFloorProbingDistance);
	DisplayDebugManager.DrawString(T);

	T = FString::Printf(TEXT("Capsule Radius: %f"), GetCapsuleRadius());
	DisplayDebugManager.DrawString(T);
}


#pragma endregion Utility