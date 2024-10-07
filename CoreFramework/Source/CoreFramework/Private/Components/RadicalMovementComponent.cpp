// Copyright 2023 Abdulrahmen Almodaimegh. All Rights Reserved.
#include "RadicalMovementComponent.h"
#include "GameFramework/Character.h"
#include "Debug/RMC_LOG.h"
#include "CFW_PCH.h"
#include "CharacterMovementComponentAsync.h"
#include "RadicalCharacter.h"
#include "StaticLibraries/CoreMathLibrary.h"
#include "VisualLogger/VisualLogger.h"

#pragma region Profiling & CVars
/* Core Update Loop */
DECLARE_CYCLE_STAT(TEXT("Tick Component"), STAT_TickComponent, STATGROUP_RadicalMovementComp)
DECLARE_CYCLE_STAT(TEXT("On Movement State Changed"), STAT_OnMovementStateChanged, STATGROUP_RadicalMovementComp)
DECLARE_CYCLE_STAT(TEXT("Perform Movement"), STAT_PerformMovement, STATGROUP_RadicalMovementComp)
DECLARE_CYCLE_STAT(TEXT("Pre Movement Update"), STAT_PreMovementUpdate, STATGROUP_RadicalMovementComp)

/* Movement Ticks */
DECLARE_CYCLE_STAT(TEXT("Ground Movement Tick"), STAT_GroundTick, STATGROUP_RadicalMovementComp)
DECLARE_CYCLE_STAT(TEXT("Air Movement Tick"), STAT_AirTick, STATGROUP_RadicalMovementComp)
DECLARE_CYCLE_STAT(TEXT("General Movement Tick"), STAT_GenTick, STATGROUP_RadicalMovementComp)

/* Collision Resolution */
DECLARE_CYCLE_STAT(TEXT("Slide Along Surface"), STAT_SlideAlongSurface, STATGROUP_RadicalMovementComp)
DECLARE_CYCLE_STAT(TEXT("Two Wall Adjust"), STAT_TwoWallAdjust, STATGROUP_RadicalMovementComp)
DECLARE_CYCLE_STAT(TEXT("Handle Impact"), STAT_HandleImpact, STATGROUP_RadicalMovementComp)
DECLARE_CYCLE_STAT(TEXT("Step Up"), STAT_StepUp, STATGROUP_RadicalMovementComp)

/* Events */
DECLARE_CYCLE_STAT(TEXT("Calculate Velocity"), STAT_CalcVel, STATGROUP_RadicalMovementComp)
DECLARE_CYCLE_STAT(TEXT("Update Rotation"), STAT_UpdateRot, STATGROUP_RadicalMovementComp)


/* Hit Queries */
DECLARE_CYCLE_STAT(TEXT("Find Floor"), STAT_FindFloor, STATGROUP_RadicalMovementComp)
DECLARE_CYCLE_STAT(TEXT("Process Landed"), STAT_ProcessLanded, STATGROUP_RadicalMovementComp)

/* Features */
DECLARE_CYCLE_STAT(TEXT("Tick Pose"), STAT_TickPose, STATGROUP_RadicalMovementComp)
DECLARE_CYCLE_STAT(TEXT("Apply Root Motion"), STAT_RootMotion, STATGROUP_RadicalMovementComp)
DECLARE_CYCLE_STAT(TEXT("Move With Base"), STAT_MoveWithBase, STATGROUP_RadicalMovementComp)
DECLARE_CYCLE_STAT(TEXT("Physics Interaction"), STAT_PhysicsInteraction, STATGROUP_RadicalMovementComp)

namespace RMCCVars
{
#if ALLOW_CONSOLE && !NO_LOGGING
	int32 EnableVLog = 0;
	FAutoConsoleVariableRef CVarEnableVLog
	(
		TEXT("rmc.EnableVLog"),
		EnableVLog,
		TEXT("Enable VLogger. 0: Disable, 1: Enable"),
		ECVF_Default
	);
	
	int32 VisualizeMovement = 0;
	FAutoConsoleVariableRef CVarShowMovementVectors
	(
		TEXT("rmc.VisualizeMovement"),
		VisualizeMovement,
		TEXT("Visualize Movement. 0: Disable, 1: Enable"),
		ECVF_Default
	);
	
	int32 VisualizeTrajectories = 0;
	FAutoConsoleVariableRef CVarShowTrajectories
	(
		TEXT("rmc.VisualizeTrajectories"),
		VisualizeTrajectories,
		TEXT("Visualize Trajectories. 0: Disable, 1: Mesh and Root, 2: Root Only"),
		ECVF_Default
	);

	int32 VisualizeAccelerationCurve = 0;
	FAutoConsoleVariableRef CVarShowAcceleration
	(
		TEXT("rmc.VisualizeAccelerationCurve"),
		VisualizeAccelerationCurve,
		TEXT("Visualize Acc Curve. 0: Disable, > 0: Height Of Curve"),
		ECVF_Default
	);

	int32 LogHits = 0;
	FAutoConsoleVariableRef CVarShowGroundSweep
	(
		TEXT("rmc.LogMovementHits"),
		LogHits,
		TEXT("Visualize the result of move hits. 0: Disable, 1: Enable"),
		ECVF_Default
	);
#endif 
}

/* Log Cateogry */
DEFINE_LOG_CATEGORY(LogRMCMovement)
DEFINE_LOG_CATEGORY(VLogRMCMovement)

#pragma endregion Profiling & CVars

// Sets default values for this component's properties
URadicalMovementComponent::URadicalMovementComponent()
{
	// Setup Post Physics Tick Group
	PostPhysicsTickFunction.bCanEverTick = true;
	PostPhysicsTickFunction.bStartWithTickEnabled = false;
	PostPhysicsTickFunction.SetTickFunctionEnable(false);
	PostPhysicsTickFunction.TickGroup = TG_PostPhysics;
	
	// General Defaults
	PhysicsState = STATE_Falling;
	MovementData = nullptr;
	bAlwaysOrientToGravity = false;
	bJustTeleported = true;

	GravityScale = 2.f;
	GravityDir = FVector(0,0,-1.f);
	
	// Set Simulation Defaults
	bEnableScopedMovementUpdates = true;
	MaxSimulationTimeStep = 0.05f;
	MaxSimulationIterations = 8;
	MaxDepenetrationWithGeometry = 500.f;
	MaxDepenetrationWithPawn = 100.f;
	
	// Set Ground Detection Defaults
	StabilityOrientationMode = MODE_Gravity;
	MaxStableSlopeAngle = 60.f;
	ExtraFloorProbingDistance = 20.f;
	bAlwaysCheckFloor = true;
	bForceNextFloorCheck = true;
	bUseFlatBaseForFloorChecks = false;
	MaxStepHeight = 45.f;
	
	
	// Set Ledge Defaults
	bCanWalkOffLedges = true;
	LedgeCheckThreshold = 4.f;
	PerchRadiusThreshold = 0.f;
	PerchAdditionalHeight = 40.f;
	
	bLedgeAndDenivelationHandling = true;
	MinVelocityForDenivelationEvaluation = 0.f;
	MaxStableUpwardsDenivelationAngle = 180.f;
	MaxStableDownwardsDenivelationAngle = 180.f;

	// Set Crouching Defaults
	bCanWalkOffLedgesWhenCrouched = true;

	// Set Physics Interactions Defaults
	Mass = 100.f;
	bEnablePhysicsInteraction = true;
	bTouchForceScaledToMass = true;
	bPushForceScaledToMass = false;
	bPushForceUsingVerticalOffset = false;
	bScalePushForceToVelocity = true;
	
	RepulsionForce = 2.5f;
	TouchForceFactor = 1.f;
	MinTouchForce = -1.f;
	MaxTouchForce = 250.f;
	StandingDownwardForceScale = 1.f;
	InitialPushForceFactor = 500.f;
	PushForceFactor = 750000.f;
	PushForcePointVerticalOffsetFactor = -0.75f;

	// Set MovingBase Defaults
	bMoveWithBase = true;
	bIgnoreBaseRotation = false;
	bImpartBaseVelocityPlanar = true;
	bImpartBaseVelocityVertical = true;
	bImpartBaseAngularVelocity = true;
	FormerBaseVelocityDecayHalfLife = 0.f;
	
	// Set Avoidance Defaults TODO: RVO Avoidance still not implemented...

	// Set Nav-Movement defaults
	NavAgentProps.bCanJump = true;
	NavAgentProps.bCanWalk = true;
	NavAgentProps.bCanSwim = false;
	MovementState = NavAgentProps;

	bRequestedMoveUseAcceleration = true;

	MovementDataClass = UMovementData::StaticClass();
}

// Called when the game starts
void URadicalMovementComponent::BeginPlay()
{
	// Create movement data instance object
	if (!MovementDataClass) MovementDataClass = UMovementData::StaticClass();
	MovementData = NewObject<UMovementData>(this, MovementDataClass, FName("Movement Data"));
	
	// This will reserve this component specifically for pawns, do we want that?
	if (!CharacterOwner)
	{
		Super::BeginPlay();
		return;
	}

	// Set root collision Shape

	// Cache any data we wanna always hold the original reference to

	// Call Super
	Super::BeginPlay();
	PhysicsState = STATE_Grounded;

	// Check if MovementData was supplied
	if (!MovementData)
	{
		RMC_FLog(Error, "No movement data asset provided")
	}
}

void URadicalMovementComponent::PostLoad()
{
	Super::PostLoad();

	CharacterOwner = Cast<ARadicalCharacter>(PawnOwner);
}

void URadicalMovementComponent::Deactivate()
{
	Super::Deactivate();
	if (!IsActive())
	{
		ClearAccumulatedForces();
	}
}



/* FUNCTIONAL */
void URadicalMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	SCOPED_NAMED_EVENT(URadicalMovementComponent_TickComponent, FColor::Yellow)
	SCOPE_CYCLE_COUNTER(STAT_TickComponent)

	InputVector = ConsumeInputVector();
	const FVector OldVelocity = Velocity;

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
	
	/* Store Previous Mesh and Root info for trajectory visualization (separate from LastUpdateLocation since we never have proper access to it here)*/
	const FVector OldRootLocation = UpdatedComponent->GetComponentLocation();
	
	/* Perform Move */
	PerformMovement(DeltaTime);
	
	/* Physics interactions updates */
	if (bEnablePhysicsInteraction)
	{
		SCOPE_CYCLE_COUNTER(STAT_PhysicsInteraction)
		ApplyDownwardForce(DeltaTime);
		ApplyRepulsionForce(DeltaTime);
	}

	/* Set Delta Values */
	if (DeltaTime != 0.f)
	{
		PhysicalAcceleration = (Velocity - OldVelocity) / DeltaTime;
	}

	/* Nice extra debug visualizer should add eventually from CMC */
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	const bool bVisualizeMovement = RMCCVars::VisualizeMovement > 0;
	if (bVisualizeMovement)
	{
		VisualizeMovement();
	}
	const bool bVisualizeTrajectories = RMCCVars::VisualizeTrajectories > 0;
	if (bVisualizeTrajectories)
	{
		VisualizeTrajectories(OldRootLocation);
	}
	if (RMCCVars::VisualizeAccelerationCurve > 0)
	{
		VisualizeAccelerationCurve(OldRootLocation);
	}

	// Set it after so when its used during VisualizeTrajectories, it's using the last updates version (Mesh ticks after this)
	DebugOldMeshLocation = (CharacterOwner ? CharacterOwner->GetMesh()->GetBoneLocation(TrajectoryBoneName) : FVector::ZeroVector);
	DebugOldVelocity = Velocity;
#endif 
}

void FRadicalMovementComponentPostPhysicsTickFunction::ExecuteTick(float DeltaTime, ELevelTick TickType, ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
{
	FActorComponentTickFunction::ExecuteTickHelper(Target, /*bTickInEditor=*/ false, DeltaTime, TickType, [this](float DilatedTime)
	{
		Target->PostPhysicsTickComponent(DilatedTime, *this);
	});
}


void URadicalMovementComponent::PostPhysicsTickComponent(float DeltaTime, FRadicalMovementComponentPostPhysicsTickFunction& ThisTickFunction)
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

void URadicalMovementComponent::SetUpdatedComponent(USceneComponent* NewUpdatedComponent)
{
	// Check if it exists
	if (!NewUpdatedComponent)
	{
		return;
	}
	
	// Check if its valid, and if anything is using the delegate event
	if (IsValid(UpdatedPrimitive) && UpdatedPrimitive->OnComponentBeginOverlap.IsBound())
	{
		UpdatedPrimitive->OnComponentBeginOverlap.RemoveDynamic(this, &URadicalMovementComponent::RootCollisionTouched);
	}

	Super::SetUpdatedComponent(NewUpdatedComponent);
	CharacterOwner = Cast<ARadicalCharacter>(PawnOwner);
	
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
		UpdatedPrimitive->OnComponentBeginOverlap.AddUniqueDynamic(this, &URadicalMovementComponent::RootCollisionTouched);
	}
}

bool URadicalMovementComponent::IsCrouching() const
{
	return CharacterOwner && CharacterOwner->bIsCrouched;
}

void URadicalMovementComponent::AddRadialForce(const FVector& Origin, float Radius, float Strength,
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

void URadicalMovementComponent::AddRadialImpulse(const FVector& Origin, float Radius, float Strength,
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

void URadicalMovementComponent::StopActiveMovement()
{
	Super::StopActiveMovement();

	InputAcceleration = FVector::ZeroVector;
	bHasRequestedVelocity = false;
	RequestedVelocity = FVector::ZeroVector;
	LastUpdateRequestedVelocity = FVector::ZeroVector;
}

// END UMovementComponent

// BEGIN UPawnMovementComponent

void URadicalMovementComponent::NotifyBumpedPawn(APawn* BumpedPawn)
{
	Super::NotifyBumpedPawn(BumpedPawn);
}

void URadicalMovementComponent::OnTeleported()
{
	Super::OnTeleported();

	bJustTeleported = true;

	UpdateFloorFromAdjustment();

	UPrimitiveComponent* OldBase = GetMovementBase();
	UPrimitiveComponent* NewBase = nullptr;

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
		if (PhysicsState == STATE_Grounded) // NOTE: Assume that if we were set to STATE_General, it will be manually handled
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

#pragma region Events

void URadicalMovementComponent::CalculateVelocity(float DeltaTime)
{
	if (!VelocityBindings.Execute(this, DeltaTime))
	{
		if (!MovementData)
		{
			RMC_FLog(Error, "CalcualteVel not bound and MovementData is null. No method of calculating velocity")
		}
		else
		{
			MovementData->CalculateVelocity(this, DeltaTime);
		}
	}
}

void URadicalMovementComponent::UpdateRotation(float DeltaTime)
{
	if (!RotationBindings.Execute(this, DeltaTime))
	{
		if (!MovementData)
		{
			RMC_FLog(Error, "UpdateRot not bound and MovementData is null. No method of updating rotation")
		}
		else
		{
			MovementData->UpdateRotation(this, DeltaTime);
		}
	}
}

#pragma endregion

#pragma region Movement State & Interface

void URadicalMovementComponent::SetMovementState(EMovementState NewMovementState)
{
	if (PhysicsState == NewMovementState)
	{
		return;
	}

	const EMovementState PrevMovementState = PhysicsState;
	PhysicsState = NewMovementState;
	
	OnMovementStateChanged(PrevMovementState);
}


void URadicalMovementComponent::OnMovementStateChanged(EMovementState PreviousMovementState)
{
	SCOPE_CYCLE_COUNTER(STAT_OnMovementStateChanged);
	
	if (PhysicsState == STATE_Grounded)
	{
		/* Update floor/base on initial entry of the walking physics */
		FindFloor(UpdatedComponent->GetComponentLocation(), CurrentFloor, false);
		if (!CurrentFloor.IsWalkableFloor()) // HACK: FIX FOR WHEN SETTING MOVEMENT STATE TO GROUNDED WHEN NO VALID GROUND EXISTS, DEFLECTS OUR VEL STRONGLY FOR A FRAME IN AN ANNOYING WAY SO I'LL ADD A CHECK HERE TEMPORARILY (specific case was rail grinding and OnEndAction it being set to grounded incorrectly deflects us to the left all of a sudden)
		{
			SetMovementState(STATE_Falling);
			return;
		}
		AdjustFloorHeight();
		SetBaseFromFloor(CurrentFloor);
		
		// DEBUG:
		LOG_HIT(CurrentFloor.HitResult, 2.f);
		
		if (CurrentFloor.bWalkableFloor && PreviousMovementState == STATE_Falling)
		{
			// NOTE: Maybe use GetTangentOnSurface? Idk what the diff between the two are should check that...
			Velocity = FVector::VectorPlaneProject(Velocity, CurrentFloor.HitResult.ImpactNormal);
		}
	}
	else if (PhysicsState == STATE_Falling)
	{
		CurrentFloor.Clear();
		
		if (!CharacterOwner->HasBasedMovementOverride())
		{
			DecayingFormerBaseVelocity = GetImpartedMovementBaseVelocity();
			Velocity += DecayingFormerBaseVelocity;
		}

		// Cache the time we left ground in case we wanna access it, useful for Coyote time
		TimeLeftGround = GetWorld()->GetTimeSeconds();
		
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

	if (CharacterOwner) CharacterOwner->MovementStateChanged(PreviousMovementState);
}

void URadicalMovementComponent::DisableMovement()
{
	StopActiveMovement();
	SetMovementState(STATE_None);
}

void URadicalMovementComponent::EnableMovement()
{
	SetMovementState(STATE_Grounded);
}


void URadicalMovementComponent::AddImpulse(FVector Impulse, bool bVelocityChange)
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

void URadicalMovementComponent::AddForce(FVector Force)
{
	if (!Force.IsZero() && (PhysicsState != STATE_None) && IsActive())
	{
		if (Mass > UE_SMALL_NUMBER)
		{
			PendingForceToApply += Force / Mass;
		}
	}
}

void URadicalMovementComponent::Launch(const FVector& LaunchVel)
{
	PendingLaunchVelocity = LaunchVel;
}


void URadicalMovementComponent::ClearAccumulatedForces()
{
	PendingImpulseToApply = FVector::ZeroVector;
	PendingForceToApply = FVector::ZeroVector;
	PendingLaunchVelocity = FVector::ZeroVector;
}

void URadicalMovementComponent::ApplyAccumulatedForces(float DeltaTime)
{
	const float PendingVertImpulse = PendingImpulseToApply | GetUpOrientation(MODE_Gravity);
	const float PendingVertForce = PendingForceToApply | GetUpOrientation(MODE_Gravity);
	if (PendingVertImpulse != 0.f || PendingVertForce != 0.f)
	{
		// check to see if applied momentum is enough to overcome gravity
		if ( IsMovingOnGround() && (PendingVertImpulse + (PendingVertForce * DeltaTime) + (GetGravityZ() * DeltaTime) > UE_SMALL_NUMBER))
		{
			SetMovementState(STATE_Falling);
		}
	}

	Velocity += PendingImpulseToApply + (PendingForceToApply * DeltaTime);
	
	// Don't call ClearAccumulatedForces() because it could affect launch velocity
	PendingImpulseToApply = FVector::ZeroVector;
	PendingForceToApply = FVector::ZeroVector;
}


bool URadicalMovementComponent::HandlePendingLaunch()
{
	if (!PendingLaunchVelocity.IsZero())
	{
		Velocity = PendingLaunchVelocity;
		PendingLaunchVelocity = FVector::ZeroVector;
		if (IsMovingOnGround() && (Velocity | GetUpOrientation(MODE_Gravity)) > 0)
			SetMovementState(STATE_Falling);
		return true;
	}
	return false;
}

#pragma endregion Movement State & Interface

#pragma region Core Simulation Handling

/* FUNCTIONAL */
bool URadicalMovementComponent::CanMove() const
{
	if (!UpdatedComponent || !PawnOwner) return false;
	if (UpdatedComponent->Mobility != EComponentMobility::Movable) return false;

	return true;
}

float URadicalMovementComponent::GetSimulationTimeStep(float RemainingTime, uint32 Iterations) const
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


void URadicalMovementComponent::PerformMovement(float DeltaTime)
{
	SCOPE_CYCLE_COUNTER(STAT_PerformMovement)
	
	// Setup movement, and do not progress if setup fails
	if (!PreMovementUpdate(DeltaTime))
	{
		return;
	}
	
	// Internal Character Move - looking at CMC, it applies UpdateVelocity, RootMotion, etc... before the character move...
	{
		// Scoped updates can improve performance of multiple MoveComponent calls - CMC
		FScopedMovementUpdate ScopedMovementUpdate(UpdatedComponent, bEnableScopedMovementUpdates ? EScopedUpdate::DeferredUpdates : EScopedUpdate::ImmediateUpdates);
		
		/* Update Based Movement */
		TryUpdateBasedMovement(DeltaTime);

		/* Clean up invalid root motion sources (including ones that ended naturally) */
		const bool bHasRootMotionSources = HasRootMotionSources();
		{
			if (bHasRootMotionSources)
			{
				const FVector VelocityBeforeCleanup = Velocity;
				CurrentRootMotion.CleanUpInvalidRootMotion(DeltaTime, *CharacterOwner, *this);
			}
		}
		
		FVector OldVelocity = Velocity; // Used to check for AdditiveRootMotion (would apply accumulated forces to root motion)
		
		/* Trigger gameplay event for velocity modification & apply pending impulses and forces*/
		ApplyAccumulatedForces(DeltaTime); 
		HandlePendingLaunch(); 
		ClearAccumulatedForces();

		/* Updated saved LastPreAdditiveVelocity with any external changes to character velocity from the above methods */
		{
			if (CurrentRootMotion.HasAdditiveVelocity())
			{
				const FVector Adjustment = (Velocity - OldVelocity);
				CurrentRootMotion.LastPreAdditiveVelocity += Adjustment;
			}
		}
		
		/* Apply root motion after velocity modifications */
		if (bHasRootMotionSources)
		{
			if (CharacterOwner && CharacterOwner->IsPlayingRootMotion())
			{
				TickPose(DeltaTime);
			}

			/* Generates root motion to be used this frame from sources other than animation */
			{
				CurrentRootMotion.PrepareRootMotion(DeltaTime, *CharacterOwner, *this, true);
			}
		}

		/* Apply Root motion To Velocity */
		if (CurrentRootMotion.HasOverrideVelocity() || HasAnimRootMotion())
		{
			InitApplyRootMotionToVelocity(DeltaTime);
		}

		// Nan check
		ensureMsgf(!Velocity.ContainsNaN(), TEXT("URadicalMovementComponent::PerformMovement: Velocity contains NaN (%s)\n%s"), *GetPathNameSafe(this), *Velocity.ToString());
		
		/* Perform actual move */
		StartMovementTick(DeltaTime, 0);

		if (!HasAnimRootMotion())
		{
			SCOPE_CYCLE_COUNTER(STAT_UpdateRot);
			UpdateRotation(DeltaTime);
		}
		else if (HasAnimRootMotion()) // Apply physics rotation
		{
			const FQuat OldActorRotationQuat = UpdatedComponent->GetComponentQuat();
			const FQuat RootMotionRotationQuat = RootMotionParams.GetRootMotionTransform().GetRotation();
			if (!RootMotionRotationQuat.Equals(FQuat::Identity))
			{
				FQuat NewActorRotationQuat = RootMotionRotationQuat * OldActorRotationQuat;
				// Post Process RM Rotation Event before its applied
				if (!PostProcessRMRotationDelegate.ExecuteIfBound(this, NewActorRotationQuat, DeltaTime))
				{
					RMC_FLog(Log, "Post Process RM Rotation Event Not Bound");
				}
				MoveUpdatedComponent(FVector::ZeroVector, NewActorRotationQuat, true);
			}

			RootMotionParams.Clear();
		} // Apply root motion rotation
		else if (CurrentRootMotion.HasActiveRootMotionSources())
		{
			FQuat RootMotionRotationQuat;
			if (CharacterOwner && UpdatedComponent && CurrentRootMotion.GetOverrideRootMotionRotation(DeltaTime, *CharacterOwner, *this, RootMotionRotationQuat))
			{
				const FQuat OldActorRotationQuat = UpdatedComponent->GetComponentQuat();
				FQuat NewActorRotationQuat = RootMotionRotationQuat * OldActorRotationQuat;
				// Post Process RM Rotation Event before its applied
				if (!PostProcessRMRotationDelegate.ExecuteIfBound(this, NewActorRotationQuat, DeltaTime))
				{
					RMC_FLog(Log, "Post Process RM Rotation Event Not Bound");
				}
				MoveUpdatedComponent(FVector::ZeroVector, NewActorRotationQuat, true);
			}
		} // Rotation from root motion sources

		// Consume path following requested velocity
		LastUpdateRequestedVelocity = bHasRequestedVelocity ? RequestedVelocity : FVector::ZeroVector;
		bHasRequestedVelocity = false;
	}
	
	VLOG_STATE
	
	PostMovementUpdate(DeltaTime);
}

bool URadicalMovementComponent::PreMovementUpdate(float DeltaTime)
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
		if (CharacterOwner->IsPlayingRootMotion() && CharacterOwner->GetMesh())
		{
			TickPose(DeltaTime);
			RootMotionParams.Clear();
		}
		if (CurrentRootMotion.HasActiveRootMotionSources())
		{
			CurrentRootMotion.Clear();
		}
		/* Clear pending physics forces*/
		ClearAccumulatedForces();

		return false;
	}

	/* Setup */
	bForceNextFloorCheck |= (IsMovingOnGround() && bTeleportedSinceLastUpdate);

	// Update saved LastPreAdditiveVelocity with any external changes to character Velocity that happened since last update.
	if( CurrentRootMotion.HasAdditiveVelocity() )
	{
		const FVector Adjustment = (Velocity - LastUpdateVelocity);
		CurrentRootMotion.LastPreAdditiveVelocity += Adjustment;
	}

	// Setup crouch if needed
	{
		const bool bIsCrouching = IsCrouching();
		if (bIsCrouching && (!bWantsToCrouch || !CanCrouchInCurrentState()))
		{
			UnCrouch();
		}
		else if (!bIsCrouching && bWantsToCrouch && CanCrouchInCurrentState())
		{
			Crouch();
		}
	}
	
	return true;
}

#pragma region Actual Movement Ticks

void URadicalMovementComponent::StartMovementTick(float DeltaTime, uint32 Iterations)
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
		case STATE_General:
			GeneralMovementTick(DeltaTime, Iterations);
			break;
		default:
			SetMovementState(STATE_None);
			break;
	}
}


void URadicalMovementComponent::GroundMovementTick(float DeltaTime, uint32 Iterations)
{
	SCOPE_CYCLE_COUNTER(STAT_GroundTick)

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
		bSuccessfulSlideAlongSurface = false;
		const float IterTick = GetSimulationTimeStep(RemainingTime, Iterations);
		RemainingTime -= IterTick;

		/* Cache current values */
		UPrimitiveComponent* const OldBase = GetMovementBase();
		const FVector PreviousBaseLocation = (OldBase != NULL) ? OldBase->GetComponentLocation() : FVector::ZeroVector;
		const FVector OldLocation = UpdatedComponent->GetComponentLocation();
		PreviousFloor = CurrentFloor;

		/* Restore */
		RestorePreAdditiveRootMotionVelocity();

		/* Project Velocity To Ground Before Computing Move Deltas */
		MaintainHorizontalGroundVelocity();
		const FVector OldVelocity = Velocity;

		/* Apply acceleration after projection */
		if (!HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity())
		{
			SCOPE_CYCLE_COUNTER(STAT_CalcVel);
			if (MovementData)
				InputAcceleration = MovementData->ComputeInputAcceleration(this);
			
			CalculateVelocity(IterTick);
		}
		
		/* Apply root motion sources as they can work alongside existing velocity calculations */
		ApplyRootMotionToVelocity(IterTick);

		/* Custom CalcVelocity may have swapped us to falling so ensure that before performing the move */
		if (IsFalling()) // New
		{
			StartMovementTick(RemainingTime + IterTick, Iterations-1);
			return;
		}
		
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
			MoveAlongFloor(IterTick, &StepDownResult);

			/* Worth keeping this here if we wanna put the CalcVelocity event after MoveAlongFloor */
			if (IsFalling())
			{
				const float DesiredDist = Delta.Size();
				if (DesiredDist > UE_KINDA_SMALL_NUMBER)
				{
					// NOTE: Change here, dont know why I had the projection
					const float ActualDist = (UpdatedComponent->GetComponentLocation() - OldLocation).Size();//FVector::VectorPlaneProject(UpdatedComponent->GetComponentLocation() - OldLocation, GetUpOrientation(MODE_Gravity)).Size();
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
			const FVector NewDelta = bTriedLedgeMove ? FVector::ZeroVector : GetLedgeMove(OldLocation, Delta);
			if (!NewDelta.IsZero())
			{
				/* First revert current move (Want to avoid walking off ledges in this scope) */
				RevertMove(OldLocation, OldBase, PreviousBaseLocation, PreviousFloor, false);

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
				if ((bMustJump || !bCheckedFall) && CheckFall(PreviousFloor, CurrentFloor.HitResult, Delta, OldLocation, RemainingTime, IterTick, Iterations, bMustJump))
				{
					return;
				}
				bCheckedFall = true;

				/* Revert current move, zero delta for ledge adjustment */
				RevertMove(OldLocation, OldBase, PreviousBaseLocation, PreviousFloor, true);
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
				if (ShouldCatchAir(PreviousFloor, CurrentFloor))
				{
					HandleWalkingOffLedge(PreviousFloor.HitResult.ImpactNormal, PreviousFloor.HitResult.Normal, OldLocation, IterTick);
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
				if ((bMustJump || !bCheckedFall) && CheckFall(PreviousFloor, CurrentFloor.HitResult, Delta, OldLocation, RemainingTime, IterTick, Iterations, bMustJump))
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
			if (!bJustTeleported && !HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity() && IterTick >= MIN_TICK_TIME) // New: Root motion check
			{
				RecalculateVelocityToReflectMove(OldLocation, IterTick);
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


void URadicalMovementComponent::AirMovementTick(float DeltaTime, uint32 Iterations)
{
	SCOPE_CYCLE_COUNTER(STAT_AirTick)

	/* Validate everything before doing anything */
	if (DeltaTime < MIN_TICK_TIME)
	{
		return;	
	}
	
	float RemainingTime = DeltaTime;
	// NOTE: Not much way to get the ShouldLimitAirControl equivalent here :/

	const FVector Orientation = GetUpOrientation(MODE_Gravity);
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
		const FVector OldVelocityWithRootMotion = Velocity;
		RestorePreAdditiveRootMotionVelocity();
		const FVector OldVelocity = Velocity;
		
		/* Gameplay stuff like applying gravity */
		if (!HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity())
		{
			SCOPE_CYCLE_COUNTER(STAT_CalcVel);
			if (MovementData)
				InputAcceleration = MovementData->ComputeInputAcceleration(this);
			
			CalculateVelocity(IterTick);
		}
		
		/* Applying root motion to velocity & Decaying former base velocity */
		ApplyRootMotionToVelocity(IterTick);
		DecayFormerBaseVelocity(IterTick);

		/* Compute move parameters (if no root motion this just reduces to velocity * dt) */
		FVector Adjusted = 0.5f * (OldVelocityWithRootMotion + Velocity) * IterTick;

		/* Perform the move */
		FHitResult Hit(1.f);
		SafeMoveUpdatedComponent(Adjusted, PawnRotation, true, Hit);
		
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
			else // TODO: Implement "Sliding off of a pawns capsule", (e.g [https://twitter.com/leztusi/status/1654501852388401153?s=19]), see the discord todo channel
			{
				/* Compute impact deflection based on final velocity */
				Adjusted = Velocity * IterTick;
				
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
					else
					{
						CurrentFloor.bBlockingHit = true;
						CurrentFloor.bUnstableFloor = true;
					}
				}

				/* Hit could not be interpreted as a floor, adjust accordingly */
				HandleImpact(Hit, LastMoveTimeSlice, Adjusted);

				// DEBUG: Skipping the air control thing here, may come back to it once MovementData API is better figured out

				/* Adjust delta based on hit */
				const FVector OldHitNormal = Hit.Normal;
				const FVector OldHitImpactNormal = Hit.ImpactNormal;
				FVector Delta = ComputeSlideVector(Adjusted, 1.f - Hit.Time, OldHitNormal, Hit);

				/* Compute the velocity after the deflection */
				const UPrimitiveComponent* HitComponent = Hit.GetComponent();

				// NOTE: Look at CharacterMovementCVars to understand the first scope here
				if (!Velocity.IsNearlyZero() && MovementBaseUtility::IsSimulatedBase(HitComponent))
				{
					const FVector ContactVelocity = MovementBaseUtility::GetMovementBaseVelocity(HitComponent, NAME_None) + MovementBaseUtility::GetMovementBaseTangentialVelocity(HitComponent, NAME_None, Hit.ImpactPoint);
					const FVector NewVelocity = Velocity - Hit.ImpactNormal * FVector::DotProduct(Velocity - ContactVelocity, Hit.ImpactNormal);
					Velocity = HasAnimRootMotion() || CurrentRootMotion.HasOverrideVelocityWithIgnoreZAccumulate() ? FVector::VectorPlaneProject(Velocity, Orientation) + NewVelocity.ProjectOnToNormal(Orientation) : NewVelocity;
				}
				else if (SubTimeTickRemaining > UE_KINDA_SMALL_NUMBER && !bJustTeleported)
				{
					const FVector NewVelocity = (Delta / SubTimeTickRemaining);
					Velocity = HasAnimRootMotion() || CurrentRootMotion.HasOverrideVelocityWithIgnoreZAccumulate() ? FVector::VectorPlaneProject(Velocity, Orientation) + NewVelocity.ProjectOnToNormal(Orientation) : NewVelocity;
				}
				
				/* Perform a move in the deflected direction and solve accordingly */
				if (SubTimeTickRemaining > UE_KINDA_SMALL_NUMBER && (Delta | Adjusted) > 0.f)
				{
					/* Move in deflected direction */
					SafeMoveUpdatedComponent(Delta, PawnRotation, true, Hit);

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
						if ((Hit.Normal | Orientation) > 0.001f) // NOTE:  If normal of hit is slightly vertically upwards 
						{
							const FVector LastMoveDelta = OldVelocity * LastMoveTimeSlice;
							Delta = ComputeSlideVector(LastMoveDelta, 1.f, OldHitNormal, Hit);
						}

						TwoWallAdjust(Delta, Hit, OldHitNormal);

						// DEBUG: Another air control thing here im not sure how to handle just yet

						/* Compute velocity after deflection */
						if (SubTimeTickRemaining > UE_KINDA_SMALL_NUMBER && !bJustTeleported)
						{
							const FVector NewVelocity = (Delta / SubTimeTickRemaining);
							/* Might move this to an event for PostProcessRootMotion Velocity because we don't want to make assumptions */
							Velocity = HasAnimRootMotion() || CurrentRootMotion.HasOverrideVelocityWithIgnoreZAccumulate() ? FVector::VectorPlaneProject(Velocity, Orientation) + NewVelocity.ProjectOnToNormal(Orientation): NewVelocity;
						}

						/* bDitch = true means the pawn is straddling between two slopes neither of which it can stand on */
						bool bDitch = (((OldHitImpactNormal | Orientation) > 0.f) && ((Hit.ImpactNormal | Orientation) > 0.f) && (FMath::Abs(Delta | Orientation) <= UE_KINDA_SMALL_NUMBER) && ((Hit.ImpactNormal | OldHitImpactNormal) < 0.f));
						SafeMoveUpdatedComponent(Delta, PawnRotation, true, Hit);
						if (Hit.Time == 0.f)
						{
							/* If we're stuck, try to side step */
							FVector SideDelta = FVector::VectorPlaneProject(OldHitNormal + Hit.ImpactNormal, Orientation).GetSafeNormal();
							if (SideDelta.IsNearlyZero())
							{
								SideDelta = FVector(OldHitNormal.Y, -OldHitNormal.X, 0).GetSafeNormal(); // HACK: TEMP CHANGE THE MATH HERE LATER
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
		else
		{
			/* For unstable floors NOTE: A shit approach but it works (UnstableFloor may be checked without being properly valid) */
			CurrentFloor.bBlockingHit = false;
			CurrentFloor.bUnstableFloor = false;
		}
	}
}

void URadicalMovementComponent::GeneralMovementTick(float DeltaTime, uint32 Iterations)
{
	SCOPE_CYCLE_COUNTER(STAT_GenTick);
	
	if (DeltaTime < MIN_TICK_TIME) return;


	bJustTeleported = false;
	float RemainingTime = DeltaTime;
	
	while ((RemainingTime >= MIN_TICK_TIME) && (Iterations < MaxSimulationIterations))
	{
		/* Setup current move iteration */
		Iterations++;
		bJustTeleported = false;
		bSuccessfulSlideAlongSurface = false;
		const float IterTick = GetSimulationTimeStep(RemainingTime, Iterations);
		RemainingTime -= IterTick;

		/* Restore */
		RestorePreAdditiveRootMotionVelocity();

		/* Apply acceleration after projection */
		if (!HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity())
		{
			SCOPE_CYCLE_COUNTER(STAT_CalcVel);
			if (MovementData)
				InputAcceleration = MovementData->ComputeInputAcceleration(this);
			
			CalculateVelocity(IterTick);
		}

		/* Apply root motion (always after) */
		ApplyRootMotionToVelocity(IterTick);

		/* Cache current values & Perform Move */
		FVector OldLocation = UpdatedComponent->GetComponentLocation();
		const FVector Delta = Velocity * IterTick;
		FHitResult Hit(1.f);
		SafeMoveUpdatedComponent(Delta, UpdatedComponent->GetComponentQuat(), true, Hit);

		/* Evaluate Hit */
		if (Hit.Time < 1.f)
		{
			const FVector GravDir = GetGravityDir();
			const FVector VelDir = Velocity.GetSafeNormal();
			const float UpDown = GravDir | VelDir;

			bool bSteppedUp = false;
			if ((FMath::Abs(Hit.ImpactNormal | GravDir) < 0.2f) && (UpDown < 0.5f) && (UpDown > -0.2f) && CanStepUp(Hit))
			{
				float StepH = UpdatedComponent->GetComponentLocation() | (-GravDir);
				// BUG: This doesn't work very well in this case (Can be seen w/ the air grind especially movement breaks)
				//bSteppedUp = StepUp(GravDir, Hit, Delta * (1.f - Hit.Time));
				if (bSteppedUp) 
				{
					OldLocation = FVector::VectorPlaneProject(OldLocation, -GravDir) + (((UpdatedComponent->GetComponentLocation() + OldLocation) | (-GravDir)) - StepH);
				}
			}

			if (!bSteppedUp)
			{
				// Adjust and try again
				HandleImpact(Hit, DeltaTime, Delta);
				SlideAlongSurface(Delta, (1.f-Hit.Time), Hit.Normal, Hit, true);
			}
		}
		
		/* Make velocity reflect actual move */ 
		if (!bJustTeleported && !HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity() && IterTick >= MIN_TICK_TIME)
		{
			// NOTE: We're not guaranteed to have a floor result in STATE_General
			if (CurrentFloor.bWalkableFloor)
				RecalculateVelocityToReflectMove(OldLocation, IterTick);
			else
				Velocity = (UpdatedComponent->GetComponentLocation() - OldLocation) / IterTick;
				
		}

		/* If we didn't move at all, abort since future iterations will also be stuck */
		if (UpdatedComponent->GetComponentLocation() == OldLocation)
		{
			RemainingTime = 0.f;
			break;
		}
	}

}


void URadicalMovementComponent::MoveAlongFloor(const float DeltaTime, FStepDownFloorResult* OutStepDownResult)
{
	if (!CurrentFloor.IsWalkableFloor())
	{
		return;
	}
	
	/* Move along the current Delta */
	FHitResult Hit(1.f);
	FVector Delta = Velocity * DeltaTime;
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
			OnStuckInGeometry(&Hit);
		}
	} // Was in penetration
	else if (Hit.IsValidBlockingHit())
	{
		float PercentTimeApplied = Hit.Time;

		// TODO: Might be good to check if this is a hit we can continue projecting onto
		/* Hit could be a valid floor, lets see if we can project our move onto it */
		/*if (IsFloorStable(Hit))
		{
			// Another walkable ramp.
			const float InitialPercentRemaining = 1.f - PercentTimeApplied;
			Delta = ComputeGroundMovementDelta(Delta * InitialPercentRemaining, Hit, false);
			LastMoveTimeSlice = InitialPercentRemaining * LastMoveTimeSlice;
			SafeMoveUpdatedComponent(Delta, UpdatedComponent->GetComponentQuat(), true, Hit);

			const float SecondHitPercent = Hit.Time * InitialPercentRemaining;
			PercentTimeApplied = FMath::Clamp(PercentTimeApplied + SecondHitPercent, 0.f, 1.f);
		}*/
		
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
				bJustTeleported = true;
				const float StepUpTimeSlice = (1.f - PercentTimeApplied) * DeltaTime;
				if (!HasAnimRootMotion() && StepUpTimeSlice > UE_KINDA_SMALL_NUMBER)
				{
					// NOTE: Used to be MaintainHorizontalGroundVelocity but that doesn't really make sense as we make the assumption in StepUp that everything is aligned with gravity (though no assumptions on the direction of gravity)
					RecalculateVelocityToReflectMove(PreStepUpLocation, StepUpTimeSlice);
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

void URadicalMovementComponent::PostMovementUpdate(float DeltaTime)
{
	SaveBaseLocation();
	
	UpdateComponentVelocity();
	
	LastUpdateLocation = UpdatedComponent ? UpdatedComponent->GetComponentLocation() : FVector::ZeroVector;
	LastUpdateRotation = UpdatedComponent ? UpdatedComponent->GetComponentQuat() : FQuat::Identity;
	LastUpdateVelocity = Velocity;

	// Check if we're now in a state where we have to uncrouch
	{
		if (IsCrouching() && !CanCrouchInCurrentState())
		{
			UnCrouch();
		}
	}
}

void URadicalMovementComponent::ProcessLanded(FHitResult& Hit, float DeltaTime, uint32 Iterations)
{
	SCOPE_CYCLE_COUNTER(STAT_ProcessLanded);

	if (CharacterOwner) CharacterOwner->Landed(Hit);

	const FVector PreImpactAccel = InputAcceleration + (IsFalling() ? GetUpOrientation(MODE_Gravity) * GetGravityZ() : FVector::ZeroVector) ;
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

	// Log Ground Hit
	{
		VLOG_HIT(Hit, "[LANDED]");
	}
}

void URadicalMovementComponent::StartFalling(uint32 Iterations, float RemainingTime, float IterTick, const FVector& Delta, const FVector SubLoc)
{
	const float DesiredDist = Delta.Size();
	const float ActualDist = FVector::VectorPlaneProject(UpdatedComponent->GetComponentLocation() - SubLoc, GetUpOrientation(MODE_PawnUp)).Size(); // NOTE: Doesn't respect arbitrary rotation, but we're transitioning to a fall
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

void URadicalMovementComponent::RevertMove(const FVector& OldLocation, UPrimitiveComponent* OldBase, const FVector& InOldBaseLocation, const FGroundingStatus& OldFloor, bool bFailMove)
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
		InputAcceleration = FVector::ZeroVector;
	}
}

void URadicalMovementComponent::OnStuckInGeometry(const FHitResult* Hit)
{
	if (Hit == nullptr)
	{
		RMC_FLog(Log, "%s is stuck and failed to move!", *CharacterOwner->GetName());
	}
	else
	{
		RMC_FLog(Log, "%s is stuck and failed to move! Velocity: X=%3.2f Y=%3.2f Z=%3.2f Location: X=%3.2f Y=%3.2f Z=%3.2f Normal: X=%3.2f Y=%3.2f Z=%3.2f PenetrationDepth:%.3f Actor:%s Component:%s BoneName:%s",
			*GetNameSafe(CharacterOwner),
			Velocity.X, Velocity.Y, Velocity.Z,
			Hit->Location.X, Hit->Location.Y, Hit->Location.Z,
			Hit->Normal.X, Hit->Normal.Y, Hit->Normal.Z,
			Hit->PenetrationDepth,
			*Hit->HitObjectHandle.GetName(),
			*GetNameSafe(Hit->GetComponent()),
			Hit->BoneName.IsValid() ? *Hit->BoneName.ToString() : TEXT("NONE"));
	}

	
	CharacterOwner->OnStuckInGeometry(*Hit);
	
	// Don't update velocity based on our (failed) change in position this update since we're stuck
	bJustTeleported = true;
}


#pragma endregion Core Simulation Handling

// TODO: For stuff like JumpOff, can use an event and handle it later elsewhere

#pragma region Ground Stability Handling

bool URadicalMovementComponent::IsFloorStable(const FHitResult& Hit) const
{
	if (!Hit.IsValidBlockingHit()) return false;

	const FVector Orientation = StabilityOrientationMode == MODE_Gravity ? GetUpOrientation(MODE_Gravity) : GetUpOrientation(MODE_PawnUp);
	float TestStableAngle = MaxStableSlopeAngle;
	
	// See if the component overrides floor angle
	const UPrimitiveComponent* HitComponent = Hit.Component.Get();
	if (HitComponent)
	{
		const FWalkableSlopeOverride& SlopeOverride = HitComponent->GetWalkableSlopeOverride();
		TestStableAngle = SlopeOverride.ModifyWalkableFloorZ(TestStableAngle);
		if (TestStableAngle != MaxStableSlopeAngle) TestStableAngle = SlopeOverride.GetWalkableSlopeAngle();
	}

	const float Angle = FMath::RadiansToDegrees(FMath::Acos(Hit.ImpactNormal | Orientation));
	
	if (Angle > TestStableAngle)
	{
		RMC_FLog(Warning, "Angle Limit Hit For (%f) Along Direction (%s): %f", TestStableAngle, *Orientation.ToCompactString(), Angle);
		LOG_HIT(Hit, 2);
		return false;
	}
	
	return true;
}

bool URadicalMovementComponent::CheckFall(const FGroundingStatus& OldFloor, const FHitResult& Hit, const FVector& Delta,
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
void URadicalMovementComponent::UpdateFloorFromAdjustment()
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
// BUG: (15th August, 2023) Not sure if Pawn orientation is the best here. Doesn't work well w/ the proposed changes to MaintainHorizontalGroundVel...
// BUG (15th August, 2023) Using PawnOrientation here can result in weird behavior when "Stepping down" while not upright, Results in a sudden side movement that looks awful, changed it to Gravity for now
// BUG (17th August, 2023) Snapping down causes issues at the edges of a ramp, mainly happens when orientation is Upright
void URadicalMovementComponent::AdjustFloorHeight()
{
	// Use orientation variable to test stuff out easier (Initially was PawnUp)
	const FVector Orientation = GetUpOrientation(MODE_Gravity);//CurrentFloor.HitResult.ImpactNormal;//GetUpOrientation(MODE_Gravity);
	
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
		const float InitialHeight = UpdatedComponent->GetComponentLocation() | Orientation;
		const float AvgFloorDist = (MIN_FLOOR_DIST + MAX_FLOOR_DIST) * 0.5f;

		/* Move dist such that if we're above MAX, it'll move us down - below MIN, it'll move us up. Relative to the average of the two thresholds */
		const float MoveDist = AvgFloorDist - OldFloorDist;//FMath::Max(AvgFloorDist - OldFloorDist, 0.f); // NOTE: Disabled snapping down (clamp min to 0)

		// BUG: (17th August, 2023) Adressing that point, it seems to be a product of RecalculateVel after a step down. This helps resolve that but is kinda hacky. Look at KCC instead for this.
		// NOTE: In RecalculateVelToReflectMove, maybe only recalculate the part that's on the ground plane, and the stuff along the plane normal we use the prev version, sorta like throwing away Z
		bJustTeleported |= MoveDist < 0.f;
		
		SafeMoveUpdatedComponent(Orientation * MoveDist, UpdatedComponent->GetComponentQuat(), true, AdjustHit);

		VLOG_MOVE(Orientation * MoveDist, "[%.2f cm] Ground Snapping", MoveDist)

		/* Check if the snapping resulted in a valid hit */
		if (!AdjustHit.IsValidBlockingHit())
		{
			CurrentFloor.FloorDist += MoveDist; 
		} // Invalid blocking hit, adjust our cached floor dist for next next
		else if (MoveDist > 0.f)
		{
			const float CurrentHeight = UpdatedComponent->GetComponentLocation() | Orientation;
			CurrentFloor.FloorDist += CurrentHeight - InitialHeight;
		} // Below MIN_FLOOR_DIST
		else
		{
			const float CurrentHeight = UpdatedComponent->GetComponentLocation() | Orientation;
			CurrentFloor.FloorDist = CurrentHeight - (AdjustHit.Location | Orientation);
			if (IsFloorStable(AdjustHit))
			{
				CurrentFloor.SetFromSweep(AdjustHit, CurrentFloor.FloorDist, true);
			}
			else CurrentFloor.bUnstableFloor = true;
		} // Above MAX_FLOOR_DIST, could be a new floor value so we set that if its walkable

		/* Don't recalculate velocity based on snapping, also avoid if we moved out of penetration */
		bJustTeleported |= (OldFloorDist < 0.f); 

		/* If something caused us to adjust our height (especially a depenetration), we should ensure another check next frame or we will keep a stale result */
		bForceNextFloorCheck = true;
	}
}

// DEBUG: Seems right?
bool URadicalMovementComponent::IsWithinEdgeTolerance(const FVector& CapsuleLocation, const FVector& TestImpactPoint,
	const float CapsuleRadius) const
{
	const float DistFromCenterSq = FVector::VectorPlaneProject(TestImpactPoint - CapsuleLocation, GetUpOrientation(MODE_PawnUp)).SizeSquared();
	const float ReducedRadiusSq = FMath::Square(FMath::Max(SWEEP_EDGE_REJECT_DISTANCE + UE_KINDA_SMALL_NUMBER, CapsuleRadius - SWEEP_EDGE_REJECT_DISTANCE));
	return DistFromCenterSq < ReducedRadiusSq;
}


void URadicalMovementComponent::ComputeFloorDist(const FVector& CapsuleLocation, float LineDistance, float SweepDistance,
	FGroundingStatus& OutFloorResult, float SweepRadius, const FHitResult* DownwardSweepResult) const
{
	OutFloorResult.Clear();

	float PawnRadius, PawnHalfHeight;
	CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleSize(PawnRadius, PawnHalfHeight);
	
	bool bSkipSweep = false;
	if (DownwardSweepResult != nullptr && DownwardSweepResult->IsValidBlockingHit())
	{
		/* Accept it only if the supplied sweep was vertical and downwards relative to character orientation */
		float TraceStartHeight = DownwardSweepResult->TraceStart | GetUpOrientation(MODE_PawnUp); 
		float TraceEndHeight = DownwardSweepResult->TraceEnd | GetUpOrientation(MODE_PawnUp);
		float TracePlaneProjection = FVector::VectorPlaneProject(DownwardSweepResult->TraceStart - DownwardSweepResult->TraceEnd, GetUpOrientation(MODE_PawnUp)).SizeSquared();

		if ((TraceStartHeight > TraceEndHeight) && (TracePlaneProjection <= UE_KINDA_SMALL_NUMBER))
		{
			/* Reject hits that are barely on the cusp of the radius of the capsule */
			if (IsWithinEdgeTolerance(DownwardSweepResult->Location, DownwardSweepResult->ImpactPoint, PawnRadius))
			{
				/* Don't try a redundant sweep, regardless of whether this sweep is usable */
				bSkipSweep = true;

				DEBUG_PRINT_MSG(1, "Skipping Sweet, using down hit result")

				LOG_HIT((*DownwardSweepResult), 2.f);
				
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
					OutFloorResult.SetWalkable(true);
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
				OutFloorResult.bUnstableFloor = true; // NOTE: SetFromLineTrace will set it to unstable otherwise
				if (LineResult <= LineDistance && IsFloorStable(Hit))
				{
					OutFloorResult.SetFromLineTrace(Hit, OutFloorResult.FloorDist, LineResult, true);
					return;
				}
			}
		}
	}

	/* If we're here, that means no hits were acceptable */
	OutFloorResult.SetWalkable(false);
}

// TODO
void URadicalMovementComponent::FindFloor(const FVector& CapsuleLocation, FGroundingStatus& OutFloorResult,
	bool bCanUseCachedLocation, const FHitResult* DownwardSweepResult) const
{
	SCOPE_CYCLE_COUNTER(STAT_FindFloor)
	
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
	
	const float CapsuleRadius = CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleRadius();

	/* Increase height check slightly if currently groudned to prevent ground snapping height from later invalidating the floor result */
	const float HeightCheckAdjust = (IsMovingOnGround() ? MAX_FLOOR_DIST + UE_KINDA_SMALL_NUMBER : -MAX_FLOOR_DIST);

	/* Setup sweep distances */
	float FloorSweepTraceDist = FMath::Max(MAX_FLOOR_DIST, FMath::Max(ExtraFloorProbingDistance, MaxStepHeight) + HeightCheckAdjust);
	float FloorLineTraceDist = FloorSweepTraceDist;
	bool bNeedToValidateFloor =  true;

	/* Perform sweep */
	if (FloorLineTraceDist > 0.f || FloorSweepTraceDist > 0.f)
	{
		URadicalMovementComponent* MutableThis = const_cast<URadicalMovementComponent*>(this);
		
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
				RMC_FLog(Display, "Skipping. Floor Sweep not required")
				
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
				OutFloorResult.SetWalkable(false);
			}
		}
	}
}

// DONE
bool URadicalMovementComponent::FloorSweepTest(FHitResult& OutHit, const FVector& Start, const FVector& End,
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
		float CapsuleRadius, CapsuleHeight;
		CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleSize(CapsuleRadius, CapsuleHeight);
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
bool URadicalMovementComponent::IsValidLandingSpot(const FVector& CapsuleLocation, const FHitResult& Hit) const
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

		float PawnRadius, PawnHalfHeight;
		CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleSize(PawnRadius, PawnHalfHeight);

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
bool URadicalMovementComponent::ShouldCheckForValidLandingSpot(const FHitResult& Hit) const
{
	LOG_HIT(Hit, 2);
	/*
	 * See if we hit an edge of a surface on the lower portion of the capsule.
	 * In this case the normal will not equal the impact normal and a downward sweep may find a walkable surface on top of the edge
	 */
	const FVector PawnUp = GetUpOrientation(MODE_PawnUp); // DEBUG: Pawn up should be the right thing here

	if ((Hit.Normal | PawnUp) > UE_KINDA_SMALL_NUMBER)// && !Hit.Normal.Equals(Hit.ImpactNormal)) BUG: Temporary removed that check to get unstable floors working w/out an extra FindFloor call, will it break anything?
	{
		const FVector PawnLocation = UpdatedComponent->GetComponentLocation();
		if (IsWithinEdgeTolerance(PawnLocation, Hit.ImpactPoint, CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleRadius()))
		{
			return true;
		}
	}
	return false;
}

void URadicalMovementComponent::MaintainHorizontalGroundVelocity()
{
	/*
	auto FromToRot = [](FVector FromDir, FVector ToDir)
	{
		FVector Axis = FVector::CrossProduct(FromDir, ToDir).GetSafeNormal();
		float Angle = FMath::Acos(FromDir.GetSafeNormal() | ToDir.GetSafeNormal());
		return FQuat(Axis, Angle);
	};

	// FromDir is the "Up vector" of our velocity
	FQuat VelRotation = Velocity.Rotation().Quaternion();
	FVector FromVec = VelRotation.GetUpVector();
	const float FromAngle = FMath::Acos(FromVec.GetSafeNormal() | CurrentFloor.HitResult.ImpactNormal);
	
	// If our angle with the ground plane is > 180, mirror it on that plane (the up vector)
	if (FromAngle >= PI/2.f) FromVec = FromVec.MirrorByVector(CurrentFloor.HitResult.ImpactNormal);

	// Now we're ready to rotate
	const FQuat AlignmentRot = FromToRot(FromVec, CurrentFloor.HitResult.ImpactNormal);
	Velocity = AlignmentRot * Velocity;
	*/

	/*
	const FVector PlaneNormal = CurrentFloor.HitResult.ImpactNormal.GetSafeNormal();
	const FVector PlaneBiTangent = (Velocity.GetSafeNormal() ^ PlaneNormal).GetSafeNormal();
	Velocity = (PlaneNormal ^ PlaneBiTangent).GetSafeNormal() * Velocity.Size();
	return;
	*/
	
	// Old Method, worked pretty well, split the projected into components so we don't change dir of our vel on the ground plane
	const float VelMag = Velocity.Size();
	FVector ProjectedVelocity = FVector::VectorPlaneProject(Velocity, CurrentFloor.HitResult.ImpactNormal).GetSafeNormal() * VelMag;
	Velocity = FVector::VectorPlaneProject(Velocity, GravityDir).GetSafeNormal() * FVector::VectorPlaneProject(ProjectedVelocity, GravityDir).Size() + ProjectedVelocity.ProjectOnToNormal(GravityDir);

	// CONE SQUEEZE ROTATION V (THERE IS POTENTIAL HERE GANG) //
	// First cone opening angle defines the prev floor normal we were moving on
	/*
	const float PrevOpeningAngle = FMath::Acos(PreviousFloor.HitResult.ImpactNormal | (-GravityDir));
	const float NewOpeningAngle = FMath::Acos(CurrentFloor.HitResult.ImpactNormal | (-GravityDir));

	// Construct a quaternion where the axis of rotation is velocity cross gravity, we're tilting our velocity up, never adding anything sideways
	const FVector RotAxis = (Velocity.GetSafeNormal() ^ (-GravityDir)).GetSafeNormal();
	const FQuat GroundQuat(RotAxis, NewOpeningAngle - PrevOpeningAngle);

	Velocity = (GroundQuat * Velocity.GetSafeNormal()) * Velocity.Size();*/

	/*
	const FVector RotAxis = (Velocity.GetSafeNormal() ^ (-GravityDir)).GetSafeNormal();
	const FVector VelUp = (RotAxis ^ Velocity.GetSafeNormal()).GetSafeNormal();
	
	const float CurrentOpeningAngle = FMath::Acos(VelUp | (-GravityDir));
	const float NewOpeningAngle = FMath::Acos(CurrentFloor.HitResult.ImpactNormal | (-GravityDir));
	const float RotAngle = NewOpeningAngle - CurrentOpeningAngle;
	
	const FQuat GroundQuat(RotAxis, RotAngle);
	Velocity = (GroundQuat * Velocity.GetSafeNormal()) * Velocity.Size();*/
}

void URadicalMovementComponent::RecalculateVelocityToReflectMove(const FVector& OldLocation, const float DeltaTime)
{
	const float PreVelSize = FMath::Abs(Velocity.Size());
	const FVector OldVelocity = Velocity;
	
	//Fconst FVector MoveDelta = UpdatedComponent->GetComponentLocation() - OldLocation;
	if (false && IsMovingOnGround())
	{
		const FVector PlaneNormal = CurrentFloor.HitResult.ImpactNormal; // Doing this will remove any contribution due to floor snapping
		const FVector RecalcedVelOnPlane = FVector::VectorPlaneProject((UpdatedComponent->GetComponentLocation() - OldLocation) / DeltaTime, PlaneNormal);
		const FVector ProperVelAlongNormal = Velocity.ProjectOnToNormal(PlaneNormal);
		// The velocity on the ground plane is recalculated, and velocity along the plane normal isnt. Makes it so
		Velocity = RecalcedVelOnPlane + ProperVelAlongNormal;
	}
	else
	{
		Velocity = (UpdatedComponent->GetComponentLocation() - OldLocation) / DeltaTime;
	}
	const float PostVelSize = FMath::Abs(Velocity.Size());
	//if (PostVelSize > PreVelSize && !bSuccessfulSlideAlongSurface && (Velocity | OldVelocity) >= 0) // In case recomputed vel changes direction, makes sense itll be higher mag
	//{
	//	Velocity = Velocity.GetSafeNormal() * PreVelSize;
	//}

	// BUG: Why was I doing this? The diff between the two scopes is that one maintains magnitude after projection but I can't tell the difference in gameplay.
//	if (bSuccessfulSlideAlongSurface)
//	{
//		Velocity = FVector::VectorPlaneProject(Velocity, CurrentFloor.HitResult.ImpactNormal);
	//}
	//else
	{
		MaintainHorizontalGroundVelocity();
	}

	// Ensure magnitude never increases
	Velocity = Velocity.GetClampedToMaxSize(PreVelSize);
}

#pragma endregion Ground Stability Handling

#pragma region Step Handling

bool URadicalMovementComponent::StepUp(const FVector& Orientation, const FHitResult& StepHit, const FVector& Delta, FStepDownFloorResult* OutStepDownResult)
{
	SCOPE_CYCLE_COUNTER(STAT_StepUp)

	/* Ensure we can step up */
	if (!CanStepUp(StepHit) || MaxStepHeight <= 0.f) return false;

	/* Get Pawn Location And Properties */
	const FVector OldLocation = UpdatedComponent->GetComponentLocation();
	float PawnRadius, PawnHalfHeight;
	CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleSize(PawnRadius, PawnHalfHeight);

	/* Don't step up if top of the capsule is what recieved the hit */
	const FVector InitialImpactPoint = StepHit.ImpactPoint;
	if (UCoreMathLibrary::DistanceAlongAxis(OldLocation, InitialImpactPoint, Orientation) > (PawnHalfHeight - PawnRadius)) // -Radius, want center of top hemisphere
	{
		return false;
	}

	/* Step Parameter Setup */
	float StepTravelUpHeight = MaxStepHeight;
	float StepTravelDownHeight = StepTravelUpHeight;
	const float StepSideP = StepHit.ImpactNormal | Orientation;
	float PawnInitialFloorBaseP = (OldLocation | Orientation) - PawnHalfHeight;
	float PawnFloorPointP = PawnInitialFloorBaseP;

	/* Further check the validity of the hit before attempting to step up */
	if (IsMovingOnGround() && CurrentFloor.IsWalkableFloor())
	{
		// Since we float a variable amount off the floor, we need to enforce max step height off the actual point of impact with the floor.
		const float FloorDist = FMath::Max(0.f, CurrentFloor.GetDistanceToFloor());
		PawnInitialFloorBaseP -= FloorDist;
		StepTravelUpHeight = FMath::Max(StepTravelUpHeight - FloorDist, 0.f);
		StepTravelDownHeight = (MaxStepHeight + 2.f * MAX_FLOOR_DIST);

		const bool bHitVerticalFace = !IsWithinEdgeTolerance(StepHit.Location, StepHit.ImpactPoint, PawnRadius);
		if (!CurrentFloor.bLineTrace && !bHitVerticalFace)
		{
			PawnFloorPointP = CurrentFloor.HitResult.ImpactPoint | Orientation;
		}
		else
		{
			// Base floor point is the base of the capsule moved down by how far we are hovering over the surface we are hitting.
			PawnFloorPointP -= CurrentFloor.FloorDist;
		}
	}

	/* Don't step up if impact is below us */
	if (UCoreMathLibrary::DistanceAlongAxis(PawnInitialFloorBaseP * Orientation, InitialImpactPoint, Orientation) <= 0.f)
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

		// NOTE: This might not actually have been true, the issue might've lied with using Normal instead of ImpactNormal in MaintainHorizontalGroundVelocity()
		if (DeltaH > MaxStepHeight)
		{
			RMC_FLog(Warning, "Failed Step Up w/ Height %.3f", DeltaH);
			ScopedStepUpMovement.RevertMove();
			return false;
		}
		else
		{
			RMC_FLog(Warning, "Succesful Step Up w/ Height %.3f", DeltaH)
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
			if (UCoreMathLibrary::DistanceAlongAxis(OldLocation, Hit.Location, Orientation) > 0.f)
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
		if (DeltaH > 0.f && !CanStepUp(Hit)) // Allow stepping down to invalid surfaces tho
		{
			ScopedStepUpMovement.RevertMove();
			return false;
		}

		// See if we can validate the floor as a result of the step down
		if (OutStepDownResult != nullptr) // DEBUG: Gonna try to throw away the step down result for now (No longer an issue w/ Angle restrictions...?)
		{
			FindFloor(UpdatedComponent->GetComponentLocation(), StepDownResult.FloorResult, false, &Hit);

			// Reject unwalkable normals if we end up higher than our initial height (ok if we end up lower tho)
			if (UCoreMathLibrary::DistanceAlongAxis(OldLocation, Hit.Location, Orientation) > 0.f)
			{
				const float MAX_STEP_SIDE_H = 0.08f; // DEBUG: Oh they're referring to normals of the step hit here...
				if (!StepDownResult.FloorResult.bBlockingHit && StepSideP < MAX_STEP_SIDE_H)
				{
					RMC_FLog(Warning, "Failed Step Up Due To Normal w/ Height  %.3f", DeltaH);
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
	
	LOG_HIT(Hit, 3.f);
	
	return true;
}

bool URadicalMovementComponent::CanStepUp(const FHitResult& StepHit) const
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

float URadicalMovementComponent::GetValidPerchRadius() const
{
	const float PawnRadius = CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleRadius();
	return FMath::Clamp(PawnRadius - GetPerchRadiusThreshold(), 0.11f, PawnRadius);
};

bool URadicalMovementComponent::ShouldCatchAir(const FGroundingStatus& OldFloor, const FGroundingStatus& NewFloor) const
{
	// Check if we even need to look at this setting
	if (!bLedgeAndDenivelationHandling) return false;
	
	// There are four possible permutations to consider: (Velocity to the right in below diagrams)
	// 1.) [ _/ ] ALWAYS FALSE (Negative Angle Change, Positive Height Change)
	// 2.) [ /- ] UpwardsDenivelationAngle (Positive Angle Change, Positive Height Change)
	// 3.) [ -\ ] DownwardsDenivelationAngle (Negative Angle Change, Negative Height Change)
	// 4.) [ \_ ] ALWAYS FALSE (Positive Angle Change, Negative Height Change)

	// If we're coming from being in air, our old floor is invalid (been cleared), so we dont perform a check this tick
	if (!OldFloor.bWalkableFloor) return false;
	
	// If our velocity is below this threshold, we should not lose stability on any denivelation angle
	if (Velocity.SizeSquared() < MinVelocityForDenivelationEvaluation * MinVelocityForDenivelationEvaluation)
	{
		return false;
	}
	
	// If Dot(OldFloorNormal, Gravity) < Dot(CurrentFloorNormal, Gravity), we use UpwardDenivelation given that the new normal is more aligned with "Up", indicating we are going right in a scenario [ /- ] rather than left 
	const bool bPositiveAngleChange = (OldFloor.HitResult.ImpactNormal | GetUpOrientation(MODE_Gravity)) < (NewFloor.HitResult.ImpactNormal | GetUpOrientation(MODE_Gravity));

	// If bHeightChange = true, we're going up (so below math is positive). If it's negative, its false and we're going downwards
	const bool bPositiveHeightChange = (Velocity | GetUpOrientation(MODE_Gravity)) > 0.f;//((NewFloor.HitResult.ImpactPoint - OldFloor.HitResult.ImpactPoint) | GetUpOrientation(MODE_Gravity)) > 0;

	if (bPositiveAngleChange != bPositiveHeightChange) return false;

	const float Angle = FMath::RadiansToDegrees(FMath::Acos(OldFloor.HitResult.ImpactNormal | NewFloor.HitResult.ImpactNormal));

	// DEBUG: Need to avoid catching air on first vertex of [ _/- ], Only wanna catch air on second vertex if going up (This case may not be a big deal, will usually detect floor next tick)
	if (Angle >= (bPositiveAngleChange ? MaxStableUpwardsDenivelationAngle : MaxStableDownwardsDenivelationAngle))
	{
		return true;
	}
	return false;
}

// Done
bool URadicalMovementComponent::ShouldComputePerchResult(const FHitResult& InHit, bool bCheckRadius) const
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
bool URadicalMovementComponent::ComputePerchResult(const float TestRadius, const FHitResult& InHit,
	const float InMaxFloorDist, FGroundingStatus& OutPerchFloorResult) const
{
	if (InMaxFloorDist <= 0.f)
	{
		return false;
	}

	/* Sweep further than the actual requested distance, because a reduced capsule radius means we could miss some hits the normal radius would catch */
	float PawnRadius, PawnHalfHeight;
	CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleSize(PawnRadius, PawnHalfHeight);

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
		OutPerchFloorResult.SetWalkable(false);
		return false;
	}
	
	return true;
}


bool URadicalMovementComponent::CheckLedgeDirection(const FVector& OldLocation, const FVector& SideStep)
{
	const FVector SideDest = OldLocation + SideStep;
	FCollisionQueryParams CapsuleParams(SCENE_QUERY_STAT(CheckLedgeDirection), false, PawnOwner);
	FCollisionResponseParams ResponseParams;
	InitCollisionParams(CapsuleParams, ResponseParams);
	const FCollisionShape CapsuleShape = GetCapsuleCollisionShape(SHRINK_None);
	const ECollisionChannel CollisionChannel = UpdatedComponent->GetCollisionObjectType();
	FHitResult Result(1.f);
	GetWorld()->SweepSingleByChannel(Result, OldLocation, SideDest, FQuat::Identity, CollisionChannel, CapsuleShape, CapsuleParams, ResponseParams);

	LOG_HIT(Result, 2.f);
	
	if (!Result.bBlockingHit || IsFloorStable(Result))
	{
	    if (!Result.bBlockingHit)
	    {
			GetWorld()->SweepSingleByChannel(Result, SideDest, SideDest + GravityDir * (MaxStepHeight + LedgeCheckThreshold), FQuat::Identity, CollisionChannel, CapsuleShape, CapsuleParams, ResponseParams);
	    }
	    if ((Result.Time < 1.f) && IsFloorStable(Result))
	    {
			return true;
	    }
	}

	return false;
}

FVector URadicalMovementComponent::GetLedgeMove(const FVector& OldLocation, const FVector& Delta)
{
	if (Delta.IsZero()) return FVector::ZeroVector;

	// Establish a basis based on GravityDir & Pawn Forward
	const FVector PawnForward = UpdatedComponent->GetForwardVector();
	
	const FVector BasisUp = -GravityDir.GetSafeNormal();
	const FVector BasisRight = (Delta ^ (-GravityDir)).GetSafeNormal();
	const FVector BasisForward = (BasisUp ^ BasisRight).GetSafeNormal();
	
	FVector SideDir = Delta.ProjectOnToNormal(BasisRight) - Delta.ProjectOnToNormal(BasisForward);

	// try left
	if (CheckLedgeDirection(OldLocation, SideDir)) return SideDir;

	// Try right
	SideDir *= -1.f;
	if (CheckLedgeDirection(OldLocation, SideDir)) return SideDir;

	return FVector::ZeroVector;
}

void URadicalMovementComponent::HandleWalkingOffLedge(const FVector& PreviousFloorImpactNormal,
	const FVector& PreviousFloorContactNormal, const FVector& PreviousLocation, float TimeDelta)
{
	FHitResult Hit(CurrentFloor.HitResult);
	Hit.ImpactPoint = PreviousLocation - PreviousFloorImpactNormal * CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	Hit.Location = PreviousLocation;
	Hit.Normal = PreviousFloorContactNormal;
	Hit.ImpactNormal = PreviousFloorImpactNormal;
	LOG_HIT(Hit, 2);

	CharacterOwner->WalkingOffLedge(PreviousFloorImpactNormal, PreviousFloorContactNormal, PreviousLocation, TimeDelta);
}


#pragma endregion Ledge Handling

#pragma region Crouching

bool URadicalMovementComponent::CanCrouchInCurrentState() const
{
	if (!CanEverCrouch())
	{
		return false;
	}

	return (IsFalling() || IsMovingOnGround()) && UpdatedComponent && !UpdatedComponent->IsSimulatingPhysics();
}

void URadicalMovementComponent::Crouch()
{
	if (!IsValid(CharacterOwner))
	{
		return;
	}

	if (!CanEverCrouch())
	{
		return;
	}

	// Our crouch height could just be our regular, so check that
	if (CharacterOwner->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight() == CrouchedHalfHeight)
	{
		CharacterOwner->OnStartCrouch(0.f, 0.f);
		return;
	}

	// Change collision size to crouching dimensions
	const float ComponentScale = CharacterOwner->GetCapsuleComponent()->GetShapeScale();
	const float OldUnscaledHalfHeight = CharacterOwner->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();
	const float OldUnscaledRadius = CharacterOwner->GetCapsuleComponent()->GetUnscaledCapsuleRadius();
	// Height is not allowed to be smaller than radius.
	const float ClampedCrouchedHalfHeight = FMath::Max3(0.f, OldUnscaledRadius, CrouchedHalfHeight);
	CharacterOwner->GetCapsuleComponent()->SetCapsuleSize(OldUnscaledRadius, ClampedCrouchedHalfHeight);
	float HalfHeightAdjust = (OldUnscaledHalfHeight - ClampedCrouchedHalfHeight);
	float ScaledHalfHeightAdjust = HalfHeightAdjust * ComponentScale;

	// Our crouched half height is larger than our actual height
	{
		// Crouching to a larger height? (this is rare)
		if (ClampedCrouchedHalfHeight > OldUnscaledHalfHeight)
		{
			FCollisionQueryParams CapsuleParams(SCENE_QUERY_STAT(CrouchTrace), false, CharacterOwner);
			FCollisionResponseParams ResponseParam;
			InitCollisionParams(CapsuleParams, ResponseParam);
			const bool bEncroached = GetWorld()->OverlapBlockingTestByChannel(UpdatedComponent->GetComponentLocation() - GetUpOrientation(MODE_PawnUp) * ScaledHalfHeightAdjust, UpdatedComponent->GetComponentQuat(),
				UpdatedComponent->GetCollisionObjectType(), GetCapsuleCollisionShape(SHRINK_None), CapsuleParams, ResponseParam);

			// If encroached, cancel
			if( bEncroached )
			{
				CharacterOwner->GetCapsuleComponent()->SetCapsuleSize(OldUnscaledRadius, OldUnscaledHalfHeight);
				return;
			}
		}

		if (ShouldCrouchMaintainBaseLocation())
		{
			// Intentionally not using MoveUpdatedComponent, where a horizontal plane constraint would prevent the base of the capsule from staying at the same spot.
			UpdatedComponent->MoveComponent(-GetUpOrientation(MODE_PawnUp) * ScaledHalfHeightAdjust, UpdatedComponent->GetComponentQuat(), true, nullptr, EMoveComponentFlags::MOVECOMP_NoFlags, ETeleportType::TeleportPhysics);
		}

		CharacterOwner->bIsCrouched = true;
	}

	// Could've not maintained base location, so important to check for floor if we've moved down
	bForceNextFloorCheck = true;

	// OnStartCrouch takes the change from the Default size, not the current one (though they are usually the same).
	ARadicalCharacter* DefaultCharacter = CharacterOwner->GetClass()->GetDefaultObject<ARadicalCharacter>();
	HalfHeightAdjust = (DefaultCharacter->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight() - ClampedCrouchedHalfHeight);
	ScaledHalfHeightAdjust = HalfHeightAdjust * ComponentScale;

	CharacterOwner->OnStartCrouch( HalfHeightAdjust, ScaledHalfHeightAdjust );

}

void URadicalMovementComponent::UnCrouch()
{
	if (!IsValid(CharacterOwner))
	{
		return;
	}

	// We use the CDO to revert back
	ARadicalCharacter* DefaultCharacter = CharacterOwner->GetClass()->GetDefaultObject<ARadicalCharacter>();

	// See if collision is already at desired size.
	if( CharacterOwner->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight() == DefaultCharacter->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight() )
	{
		CharacterOwner->OnEndCrouch( 0.f, 0.f );
		return;
	}

	const float CurrentCrouchedHalfHeight = CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();

	const float ComponentScale = CharacterOwner->GetCapsuleComponent()->GetShapeScale();
	const float OldUnscaledHalfHeight = CharacterOwner->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();
	const float HalfHeightAdjust = DefaultCharacter->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight() - OldUnscaledHalfHeight;
	const float ScaledHalfHeightAdjust = HalfHeightAdjust * ComponentScale;
	const FVector PawnLocation = UpdatedComponent->GetComponentLocation();

	// Grow to uncrouched size.
	check(CharacterOwner->GetCapsuleComponent());

	// Do the uncrouching
	{
		// Try to stay in place and see if the larger capsule fits. We use a slightly taller capsule to avoid penetration.
		const UWorld* MyWorld = GetWorld();
		const float SweepInflation = UE_KINDA_SMALL_NUMBER * 10.f;
		FCollisionQueryParams CapsuleParams(SCENE_QUERY_STAT(CrouchTrace), false, CharacterOwner);
		FCollisionResponseParams ResponseParam;
		InitCollisionParams(CapsuleParams, ResponseParam);

		// Compensate for the difference between current capsule size and standing size
		const FCollisionShape StandingCapsuleShape = GetCapsuleCollisionShape(SHRINK_HeightCustom, -SweepInflation - ScaledHalfHeightAdjust); // Shrink by negative amount, so actually grow it.
		const ECollisionChannel CollisionChannel = UpdatedComponent->GetCollisionObjectType();
		bool bEncroached = true;

		if (!ShouldCrouchMaintainBaseLocation())
		{
			// Expand in place
			bEncroached = MyWorld->OverlapBlockingTestByChannel(PawnLocation, UpdatedComponent->GetComponentQuat(), CollisionChannel, StandingCapsuleShape, CapsuleParams, ResponseParam);
		
			if (bEncroached)
			{
				// Try adjusting capsule position to see if we can avoid encroachment.
				if (ScaledHalfHeightAdjust > 0.f)
				{
					// Shrink to a short capsule, sweep down to base to find where that would hit something, and then try to stand up from there.
					float PawnRadius, PawnHalfHeight;
					CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleSize(PawnRadius, PawnHalfHeight);
					const float ShrinkHalfHeight = PawnHalfHeight - PawnRadius;
					const float TraceDist = PawnHalfHeight - ShrinkHalfHeight;
					const FVector Down = -GetUpOrientation(MODE_PawnUp) * TraceDist;

					FHitResult Hit(1.f);
					const FCollisionShape ShortCapsuleShape = GetCapsuleCollisionShape(SHRINK_HeightCustom, ShrinkHalfHeight);
					const bool bBlockingHit = MyWorld->SweepSingleByChannel(Hit, PawnLocation, PawnLocation + Down, UpdatedComponent->GetComponentQuat(), CollisionChannel, ShortCapsuleShape, CapsuleParams);
					if (Hit.bStartPenetrating)
					{
						bEncroached = true;
					}
					else
					{
						// Compute where the base of the sweep ended up, and see if we can stand there
						const float DistanceToBase = (Hit.Time * TraceDist) + ShortCapsuleShape.Capsule.HalfHeight;
						const FVector NewLoc = FVector::VectorPlaneProject(PawnLocation, GetUpOrientation(MODE_PawnUp)) + PawnLocation.ProjectOnTo(GetUpOrientation(MODE_PawnUp)) + GetUpOrientation(MODE_PawnUp) *(-DistanceToBase + StandingCapsuleShape.Capsule.HalfHeight + SweepInflation + MIN_FLOOR_DIST / 2.f);
						bEncroached = MyWorld->OverlapBlockingTestByChannel(NewLoc, UpdatedComponent->GetComponentQuat(), CollisionChannel, StandingCapsuleShape, CapsuleParams, ResponseParam);
						if (!bEncroached)
						{
							// Intentionally not using MoveUpdatedComponent, where a horizontal plane constraint would prevent the base of the capsule from staying at the same spot.
							UpdatedComponent->MoveComponent(NewLoc - PawnLocation, UpdatedComponent->GetComponentQuat(), false, nullptr, EMoveComponentFlags::MOVECOMP_NoFlags, ETeleportType::TeleportPhysics);
						}
					}
				}
			}
		}
		else
		{
			// Expand while keeping base location the same.
			FVector StandingLocation = PawnLocation + GetUpOrientation(MODE_PawnUp) * (StandingCapsuleShape.GetCapsuleHalfHeight() - CurrentCrouchedHalfHeight);
			bEncroached = MyWorld->OverlapBlockingTestByChannel(StandingLocation, UpdatedComponent->GetComponentQuat(), CollisionChannel, StandingCapsuleShape, CapsuleParams, ResponseParam);

			if (bEncroached)
			{
				if (IsMovingOnGround())
				{
					// Something might be just barely overhead, try moving down closer to the floor to avoid it.
					const float MinFloorDist = UE_KINDA_SMALL_NUMBER * 10.f;
					if (CurrentFloor.bBlockingHit && CurrentFloor.FloorDist > MinFloorDist)
					{
						StandingLocation -= GetUpOrientation(MODE_PawnUp) * CurrentFloor.FloorDist - MinFloorDist;
						bEncroached = MyWorld->OverlapBlockingTestByChannel(StandingLocation, UpdatedComponent->GetComponentQuat(), CollisionChannel, StandingCapsuleShape, CapsuleParams, ResponseParam);
					}
				}				
			}

			if (!bEncroached)
			{
				// Commit the change in location.
				UpdatedComponent->MoveComponent(StandingLocation - PawnLocation, UpdatedComponent->GetComponentQuat(), false, nullptr, EMoveComponentFlags::MOVECOMP_NoFlags, ETeleportType::TeleportPhysics);
				bForceNextFloorCheck = true;
			}
		}

		// If still encroached then abort.
		if (bEncroached)
		{
			return;
		}

		CharacterOwner->bIsCrouched = false;
	}

	// Now call SetCapsuleSize() to cause touch/untouch events and actually grow the capsule
	CharacterOwner->GetCapsuleComponent()->SetCapsuleSize(DefaultCharacter->GetCapsuleComponent()->GetUnscaledCapsuleRadius(), DefaultCharacter->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight(), true);

	CharacterOwner->OnEndCrouch( HalfHeightAdjust, ScaledHalfHeightAdjust );
}

#pragma endregion Crouching

#pragma region Collision Adjustments

FVector URadicalMovementComponent::GetPenetrationAdjustment(const FHitResult& Hit) const
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

bool URadicalMovementComponent::ResolvePenetrationImpl(const FVector& Adjustment, const FHitResult& Hit, const FQuat& NewRotation)
{
	// If movement occurs, mark that we teleported so we don't incorrectly adjust velocity based on potentially very different movement than ours
	bJustTeleported |= Super::ResolvePenetrationImpl(Adjustment, Hit, NewRotation);
	return bJustTeleported;
}

void URadicalMovementComponent::HandleImpact(const FHitResult& Hit, float TimeSlice, const FVector& MoveDelta)
{
	SCOPE_CYCLE_COUNTER(STAT_HandleImpact)

	if (CharacterOwner)
	{
		CharacterOwner->MoveBlockedBy(Hit);
	}
	
	// Notify other pawn
	if (Hit.HitObjectHandle.DoesRepresentClass(APawn::StaticClass()))
	{
		APawn* OtherPawn = Hit.HitObjectHandle.FetchActor<APawn>();
		NotifyBumpedPawn(OtherPawn);
	}

	if (bEnablePhysicsInteraction)
	{
		const FVector ForceAccel = InputAcceleration + (IsFalling() ? GetGravity() : FVector::ZeroVector);
		ApplyImpactPhysicsForces(Hit, ForceAccel, Velocity);
	}
}

FVector URadicalMovementComponent::ComputeSlideVector(const FVector& Delta, const float Time, const FVector& Normal,
	const FHitResult& Hit) const
{
	FVector Result = Super::ComputeSlideVector(Delta, Time, Normal, Hit);

	if (IsFalling())
	{
		Result = HandleSlopeBoosting(Result, Delta, Time, Normal, Hit);
	}

	return Result;
}

FVector URadicalMovementComponent::HandleSlopeBoosting(const FVector& SlideResult, const FVector& Delta, const float Time, const FVector& Normal,
	const FHitResult& Hit) const
{
	FVector Result = SlideResult;
	const FVector UpDirection = GetUpOrientation(MODE_Gravity);

	if ((Result | UpDirection) > 0)
	{
		// Don't move any higher than we originally intended
		const float UpLimit = (Delta | UpDirection) * Time;
		if ((Result | UpDirection) - UpLimit > UE_KINDA_SMALL_NUMBER)
		{
			if (UpLimit > 0.f)
			{
				// Rescale the entire vector (not just vertical component) otherwise we change the direction and likely head right back into the impact
				const float UpPercent = UpLimit / (Result | UpDirection);
				Result *= UpPercent;
			}
			else
			{
				// We were heading down but were going to reflect upward, just make the deflection horizontal
				Result = FVector::ZeroVector;
			}

			// Make remaining portion of original result horizontal and parallel to impact normal
			const FVector RemainderHoriz = FVector::VectorPlaneProject(SlideResult - Result, UpDirection);
			const FVector NormalHoriz = FVector::VectorPlaneProject(Normal, UpDirection).GetSafeNormal();
			const FVector Adjust = Super::ComputeSlideVector(RemainderHoriz, 1.f, NormalHoriz, Hit);
			Result += Adjust;
		}
	}

	return Result;
}

// BUG: This may need revision, returns incorrect result in StepUp i think
float URadicalMovementComponent::SlideAlongSurface(const FVector& Delta, float Time, const FVector& InNormal, FHitResult& Hit, bool bHandleImpact)
{
	SCOPE_CYCLE_COUNTER(STAT_SlideAlongSurface)
	
	if (!Hit.bBlockingHit) return 0.f;
	
	FVector Normal(InNormal);
	if (IsMovingOnGround())
	{
		// First scope is for hits that MAY be walkable, we perform an additional check
		// Second scope is for DEFINTIELY not walkable hits
		
		if (Normal.Z > 0) // Don't push up an unwalkable surface (Might not be necessary since we expect gravity to handle this and don't wanna treat them as barriers)
		{
			if (!IsFloorStable(Hit))
				Normal = FVector::VectorPlaneProject(Normal, CurrentFloor.HitResult.ImpactNormal).GetSafeNormal();
		}
		else if (Normal.Z < 0) // Don't push down into the floor when impact is on upper portion of the capsule
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

				Normal = FVector::VectorPlaneProject(Normal, CurrentFloor.HitResult.ImpactNormal).GetSafeNormal();
			}
		}
	}
	

	const float SlideTime = Super::SlideAlongSurface(Delta, Time, Normal, Hit, bHandleImpact);
	bSuccessfulSlideAlongSurface = SlideTime > 0.f;
	return SlideTime;
}

void URadicalMovementComponent::TwoWallAdjust(FVector& Delta, const FHitResult& Hit, const FVector& OldHitNormal) const
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

void URadicalMovementComponent::TickPose(float DeltaTime)
{
	SCOPE_CYCLE_COUNTER(STAT_TickPose)

	check(CharacterOwner && CharacterOwner->GetMesh());
	USkeletalMeshComponent* SkeletalMesh = CharacterOwner->GetMesh();

	if (SkeletalMesh->ShouldTickPose())
	{
		bool bWasPlayingRootMotion = SkeletalMesh->IsPlayingRootMotion();

		SkeletalMesh->TickPose(DeltaTime, true);
		
		// Grab Root Motion Now That We Have Ticked The Pose
		if (SkeletalMesh->IsPlayingRootMotion() || bWasPlayingRootMotion)
		{
			FRootMotionMovementParams RootMotion = SkeletalMesh->ConsumeRootMotion();
			if (RootMotion.bHasRootMotion)
			{
				RootMotion.ScaleRootMotionTranslation(CharacterOwner->GetAnimRootMotionTranslationScale());
				RootMotionParams.Accumulate(RootMotion);
			}
		}
	}

	if (HasAnimRootMotion())
	{
		UAnimMontage* RootMotionMontage = nullptr;
		float RootMotionMontagePosition = -1.f;
		if (const auto RootMotionMontageInstance = CharacterOwner->GetRootMotionAnimMontageInstance())
		{
			RootMotionMontage = RootMotionMontageInstance->Montage;
			RootMotionMontagePosition = RootMotionMontageInstance->GetPosition();
		}
		
		if (ShouldDiscardRootMotion(RootMotionMontage, RootMotionMontagePosition)) 
		{
			RootMotionParams = FRootMotionMovementParams();
			return;
		}
	}
}

FVector URadicalMovementComponent::CalcRootMotionVelocity(FVector RootMotionDeltaMove, float DeltaTime, const FVector& CurrentVelocity) const
{
	// Ignore components with very small delta values
	RootMotionDeltaMove.X = FMath::IsNearlyEqual(RootMotionDeltaMove.X, 0.f, 0.01f) ? 0.f : RootMotionDeltaMove.X;
	RootMotionDeltaMove.Y = FMath::IsNearlyEqual(RootMotionDeltaMove.Y, 0.f, 0.01f) ? 0.f : RootMotionDeltaMove.Y;
	RootMotionDeltaMove.Z = FMath::IsNearlyEqual(RootMotionDeltaMove.Z, 0.f, 0.01f) ? 0.f : RootMotionDeltaMove.Z;

	const FVector RootMotionVelocity = RootMotionDeltaMove / DeltaTime;
	return RootMotionVelocity; // TODO: Post process event
}

void URadicalMovementComponent::InitApplyRootMotionToVelocity(float DeltaTime)
{
	SCOPE_CYCLE_COUNTER(STAT_RootMotion);
	
	// Animation root motion overrides velocity and doesn't allow for any other root motion sources
	if (HasAnimRootMotion())
	{
		// Get rid of root motion sources if we have any, this will always override it so we don't get weird behavior (esp because we have no "GAS" stuff so we have to manually handle them here)
		// NOTE: Clear() will just clear the root motion but tasks will still fire off delegates, ClearAndDestroy destroys the Task objects so its delegates wont be fired
		CurrentRootMotion.ClearAndDestroy();
		
		// Convert to world space
		USkeletalMeshComponent* SkeletalMesh = CharacterOwner->GetMesh();
		if (SkeletalMesh)
		{
			// Convert local space root motion to world space. Do it right before physics to make sure transforms are up to date
			RootMotionParams.Set(SkeletalMesh->ConvertLocalRootMotionToWorld(RootMotionParams.GetRootMotionTransform()));
		}

		// Then turn root motion to velocity
		if (DeltaTime > 0.f)
		{
			Velocity = CalcRootMotionVelocity(RootMotionParams.GetRootMotionTransform().GetTranslation(), DeltaTime, Velocity);
		}
	} // Have root motion from animation
	else 
	{
		if (DeltaTime > 0.f)
		{
			const FVector VelocityBeforeOverride = Velocity;
			FVector NewVelocity = Velocity;
			CurrentRootMotion.AccumulateOverrideRootMotionVelocity(DeltaTime, *CharacterOwner, *this, NewVelocity);
			if (IsFalling()) // TODO:
			{
				NewVelocity += CurrentRootMotion.HasOverrideVelocityWithIgnoreZAccumulate() ? FVector(DecayingFormerBaseVelocity.X, DecayingFormerBaseVelocity.Y, 0.f) : DecayingFormerBaseVelocity;
			}
			Velocity = NewVelocity;
		}
	} // Have root motion from sources

	// Post Process Root Motion Velocity
	if (!PostProcessRMVelocityDelegate.ExecuteIfBound(this, Velocity, DeltaTime))
	{
		RMC_FLog(Log, "Post Process RM Velocity Not Bound")
	}
}


void URadicalMovementComponent::ApplyRootMotionToVelocity(float DeltaTime)
{
	SCOPE_CYCLE_COUNTER(STAT_RootMotion);
	// NOTE: Only calling on PostProcessRMVelocity in the case of overrides. Most changes here are additive and some not through RM

	// Animation root motion is distinct from root motion sources and takes precedence (NOTE: UE comment mentions "right now" so maybe they'll be mixed later?)
	if (HasAnimRootMotion() && DeltaTime > 0.f)
	{
		if (IsFalling())
		{
			Velocity += CurrentRootMotion.HasOverrideVelocityWithIgnoreZAccumulate() ? FVector::VectorPlaneProject(DecayingFormerBaseVelocity, GetUpOrientation(MODE_Gravity)) : DecayingFormerBaseVelocity;
		}
		return;
	}

	const FVector OldVelocity = Velocity;

	bool bAppliedRootMotion = false;

	// Apply Override Velocity
	if (CurrentRootMotion.HasOverrideVelocity())
	{
		CurrentRootMotion.AccumulateOverrideRootMotionVelocity(DeltaTime, *CharacterOwner, *this, Velocity);
		// Post Process Override Velocity
		if (!PostProcessRMVelocityDelegate.ExecuteIfBound(this, Velocity, DeltaTime)) // NOTE: Is this needed here? Think so otherwise prev PP are discarded with override sources
		{
			RMC_FLog(Log, "Post Process RM Velocity Not Bound")
		}
		if (IsFalling())
		{
			Velocity += CurrentRootMotion.HasOverrideVelocityWithIgnoreZAccumulate() ? FVector::VectorPlaneProject(DecayingFormerBaseVelocity, GetUpOrientation(MODE_Gravity)) : DecayingFormerBaseVelocity;
		}
		bAppliedRootMotion = true;
	}

	// Next apply additive root motion
	if( CurrentRootMotion.HasAdditiveVelocity() )
	{
		CurrentRootMotion.LastPreAdditiveVelocity = Velocity; // Save off pre-additive Velocity for restoration next tick
		CurrentRootMotion.AccumulateAdditiveRootMotionVelocity(DeltaTime, *CharacterOwner, *this, Velocity);
		CurrentRootMotion.bIsAdditiveVelocityApplied = true; // Remember that we have it applied
		bAppliedRootMotion = true;
	}

	// Switch to Falling if we have vertical velocity from root motion so we can lift off the ground
	const FVector AppliedVelocityDelta = Velocity - OldVelocity;
	if( bAppliedRootMotion && (AppliedVelocityDelta | GetUpOrientation(MODE_Gravity)) != 0.f && IsMovingOnGround() )
	{
		float LiftoffBound;
		if( CurrentRootMotion.LastAccumulatedSettings.HasFlag(ERootMotionSourceSettingsFlags::UseSensitiveLiftoffCheck) )
		{
			// Sensitive bounds - "any positive force"
			LiftoffBound = UE_SMALL_NUMBER;
		}
		else
		{
			// Default bounds - the amount of force gravity is applying this tick
			LiftoffBound = FMath::Max(-GetGravityZ() * DeltaTime, UE_SMALL_NUMBER); // TODO: Custom GetGravityZ to incorporate MovementData adjustments
		}

		if( (AppliedVelocityDelta | GetUpOrientation(MODE_Gravity)) > LiftoffBound )
		{
			SetMovementState(STATE_Falling);
		}
	}
}

void URadicalMovementComponent::RestorePreAdditiveRootMotionVelocity()
{
	// Restore last frame's pre-additive Velocity if we had additive applied 
	// so that we're not adding more additive velocity than intended
	if( CurrentRootMotion.bIsAdditiveVelocityApplied )
	{
		Velocity = CurrentRootMotion.LastPreAdditiveVelocity;
		CurrentRootMotion.bIsAdditiveVelocityApplied = false;
	}
}


bool URadicalMovementComponent::ShouldDiscardRootMotion(const UAnimMontage* RootMotionMontage, float RootMotionMontagePosition) const
{
	// Return false if montage is null
	if (!RootMotionMontage) return false;
	
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

bool URadicalMovementComponent::HasRootMotionSources() const
{
	return CurrentRootMotion.HasActiveRootMotionSources() || (CharacterOwner && CharacterOwner->IsPlayingRootMotion() && CharacterOwner->GetMesh());
}

uint16 URadicalMovementComponent::ApplyRootMotionSource(TSharedPtr<FRootMotionSourceCFW> SourcePtr)
{
	if (ensure(SourcePtr.IsValid()))
	{
		// Set default StartTime if it hasn't been set manually
		if (!SourcePtr->IsStartTimeValid())
		{
			if (CharacterOwner)
			{
				SourcePtr->StartTime = GetWorld()->GetTimeSeconds();
			}
		}

		OnRootMotionSourceBeingApplied(SourcePtr.Get());

		return CurrentRootMotion.ApplyRootMotionSource(SourcePtr);
	}

	return (uint16)ERootMotionSourceID::Invalid;
}

void URadicalMovementComponent::OnRootMotionSourceBeingApplied(const FRootMotionSource* Source)
{
	// TODO: This is an event
}

TSharedPtr<FRootMotionSourceCFW> URadicalMovementComponent::GetRootMotionSource(FName InstanceName)
{
	return StaticCastSharedPtr<FRootMotionSourceCFW>(CurrentRootMotion.GetRootMotionSource(InstanceName));
}

TSharedPtr<FRootMotionSourceCFW> URadicalMovementComponent::GetRootMotionSourceByID(uint16 RootMotionSourceID)
{
	return StaticCastSharedPtr<FRootMotionSourceCFW>(CurrentRootMotion.GetRootMotionSourceByID(RootMotionSourceID));
}

void URadicalMovementComponent::RemoveRootMotionSource(FName InstanceName)
{
	CurrentRootMotion.RemoveRootMotionSource(InstanceName);
}

void URadicalMovementComponent::RemoveRootMotionSourceByID(uint16 RootMotionSourceID)
{
	CurrentRootMotion.RemoveRootMotionSourceByID(RootMotionSourceID);
}


/*
void URadicalMovementComponent::BlockSkeletalMeshPoseTick() const
{
	if (!SkeletalMesh) return;
	SkeletalMesh->bIsAutonomousTickPose = false;
	SkeletalMesh->bOnlyAllowAutonomousTickPose = true;
}
*/

#pragma endregion Root Motion

#pragma region Physics Interactions

// TODO: Review this, its pretty old by now
void URadicalMovementComponent::RootCollisionTouched(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	// TODO: If the OtherActor has a RadicalMovementComp, allow us to be pushed. Can make the "Push force" a parameter in Physics Interactions or a seperate thing. We move back by their Velocity direction. Only do this if planar collision, if we've been jumped on thats something else.
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

void URadicalMovementComponent::ApplyImpactPhysicsForces(const FHitResult& Impact, const FVector& ImpactAcceleration, const FVector& ImpactVelocity)
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

void URadicalMovementComponent::ApplyDownwardForce(float DeltaTime)
{
	if (StandingDownwardForceScale != 0.0f && CurrentFloor.HitResult.IsValidBlockingHit())
	{
		UPrimitiveComponent* BaseComp = CurrentFloor.HitResult.GetComponent();
		const FVector SetGravity = GetUpOrientation(MODE_Gravity) * GetGravityZ(); // DEBUG: THIS WAS THE BUG

		if (BaseComp && BaseComp->IsAnySimulatingPhysics() && !SetGravity.IsZero())
		{
			BaseComp->AddForceAtLocation(SetGravity * Mass * StandingDownwardForceScale, CurrentFloor.HitResult.ImpactPoint, CurrentFloor.HitResult.BoneName);
		}
	}
}

void URadicalMovementComponent::ApplyRepulsionForce(float DeltaTime)
{
	if (UpdatedPrimitive && RepulsionForce > 0.f && PawnOwner != nullptr)
	{
		const TArray<FOverlapInfo>& Overlaps = UpdatedPrimitive->GetOverlapInfos();
		if (Overlaps.Num() > 0)
		{
			FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(RMC_ApplyRepulsionForce));
			QueryParams.bReturnFaceIndex = false;
			QueryParams.bReturnPhysicalMaterial = false;

			float CapsuleRadius, CapsuleHalfHeight;
			CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleSize(CapsuleRadius, CapsuleHalfHeight);

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
					RMC_FLog(Warning, "%s Could not find overlap body for body index %d", *GetName(), OverlapBodyIndex);
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


void URadicalMovementComponent::SetBase(UPrimitiveComponent* NewBaseComponent, const FName InBoneName, bool bNotifyPawn)
{
	if (CharacterOwner)
	{
		CharacterOwner->SetBase(NewBaseComponent, NewBaseComponent ? InBoneName : NAME_None, bNotifyPawn);
	}
}

void URadicalMovementComponent::SetBaseFromFloor(const FGroundingStatus& FloorResult)
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

UPrimitiveComponent* URadicalMovementComponent::GetMovementBase() const
{
	return CharacterOwner ? CharacterOwner->GetMovementBase() : nullptr;
}

// TODO: Math is right I think (PawnUp to define impart plane)
FVector URadicalMovementComponent::GetImpartedMovementBaseVelocity() const
{
	if (!bMoveWithBase) return FVector::ZeroVector;
	
	FVector Result = FVector::ZeroVector;

	UPrimitiveComponent* MovementBase = CharacterOwner->GetMovementBase();
	if (MovementBaseUtility::IsDynamicBase(MovementBase))
	{
		FVector BaseVelocity = MovementBaseUtility::GetMovementBaseVelocity(MovementBase, CharacterOwner->GetBasedMovement().BoneName);

		if (bImpartBaseAngularVelocity)
		{
			// NOTE: the below used to be MODE_FloorNormal but removed that mode since nothing else used it though I think using that metric makes more sense?
			const FVector BasePointPosition = (UpdatedComponent->GetComponentLocation() - GetUpOrientation(MODE_Gravity) * CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
			const FVector BaseTangentialVel = MovementBaseUtility::GetMovementBaseTangentialVelocity(MovementBase, CharacterOwner->GetBasedMovement().BoneName, BasePointPosition);
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

void URadicalMovementComponent::DecayFormerBaseVelocity(float DeltaTime)
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


void URadicalMovementComponent::TryUpdateBasedMovement(float DeltaTime)
{
	SCOPE_CYCLE_COUNTER(STAT_MoveWithBase);
	
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

void URadicalMovementComponent::UpdateBasedMovement(float DeltaTime)
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
	if (!MovementBaseUtility::GetMovementBaseTransform(MovementBase, CharacterOwner->GetBasedMovement().BoneName, NewBaseLocation, NewBaseQuat))
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
		float Radius, HalfHeight;
		CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleSize(Radius, HalfHeight);

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

		if (MovementBase->IsSimulatingPhysics() && CharacterOwner->GetMesh())
		{
			CharacterOwner->GetMesh()->ApplyDeltaToAllPhysicsTransforms(DeltaPosition, DeltaQuat);
		}
	}
	
}

// TODO: How? Is This Right? Keeping Roll?
void URadicalMovementComponent::UpdateBasedRotation(FRotator& FinalRotation, const FRotator& ReducedRotation)
{
	if (!bMoveWithBase) return;
	
	AController* Controller = CharacterOwner ? CharacterOwner->Controller : nullptr;

	if ((Controller != nullptr) && !bIgnoreBaseRotation)
	{
		FRotator ControllerRot = Controller->GetControlRotation();
		Controller->SetControlRotation(ControllerRot + ReducedRotation);
	}
}

void URadicalMovementComponent::SaveBaseLocation()
{
	if (!bDeferUpdateBasedMovement)
	{
		const UPrimitiveComponent* MovementBase = GetMovementBase();
		if (MovementBase)
		{
			// Read transforms into old base location and quat regardless of whether its movable (because mobility can change)
			MovementBaseUtility::GetMovementBaseTransform(MovementBase, CharacterOwner->GetBasedMovement().BoneName, OldBaseLocation, OldBaseQuat);

			if (MovementBaseUtility::UseRelativeLocation(MovementBase))
			{
				// Relative Location
				FVector RelativeLocation;
				MovementBaseUtility::TransformLocationToLocal(MovementBase, CharacterOwner->GetBasedMovement().BoneName, UpdatedComponent->GetComponentLocation(), RelativeLocation);
				
				// Rotation
				if (bIgnoreBaseRotation)
				{
					// Absolute Rotation
					CharacterOwner->SaveRelativeBasedMovement(RelativeLocation, UpdatedComponent->GetComponentRotation(), false);
				}
				else
				{
					// Relative Rotation
					const FRotator RelativeRotation = (FQuatRotationMatrix(UpdatedComponent->GetComponentQuat()) * FQuatRotationMatrix(OldBaseQuat).GetTransposed()).Rotator();
					CharacterOwner->SaveRelativeBasedMovement(RelativeLocation, RelativeRotation, true);
				}
			}
		}
	}
}

void URadicalMovementComponent::OnUnableToFollowBaseMove(const FVector& DeltaPosition, const FVector& OldPosition,
	const FHitResult& MoveOnBaseHit)
{
	// TODO:
}

#pragma endregion Moving Base


#pragma region AI Path Following & RVO

// NOTE: This is for outside of NAV movement, speed set directly
void URadicalMovementComponent::RequestDirectMove(const FVector& MoveVelocity, bool bForceMaxSpeed)
{
	// No actual velocity requested, abort
	if (MoveVelocity.SizeSquared() < UE_KINDA_SMALL_NUMBER)
	{
		return;
	}

	// TODO: Air control when falling

	// Set requested move info
	RequestedVelocity = MoveVelocity;
	bHasRequestedVelocity = true;
	bRequestedMoveWithMaxSpeed = bForceMaxSpeed;

	// If moving on ground, ensure the velocity is on the ground plane
	if (IsMovingOnGround())
	{
		RequestedVelocity = FVector::VectorPlaneProject(RequestedVelocity, CurrentFloor.HitResult.ImpactNormal);
	}
}

void URadicalMovementComponent::RequestPathMove(const FVector& MoveInput)
{
	FVector AdjustedMoveInput(MoveInput);

	// Preserve magnitude when moving on ground/falling and requested input has vertical component
	if (IsMovingOnGround() && (MoveInput | CurrentFloor.HitResult.ImpactNormal) != 0)
	{
		AdjustedMoveInput = FVector::VectorPlaneProject(MoveInput, CurrentFloor.HitResult.ImpactNormal).GetSafeNormal() * MoveInput.Size();
	}
	else if (IsFalling() && (MoveInput | GetUpOrientation(MODE_Gravity)) != 0)
	{
		AdjustedMoveInput = FVector::VectorPlaneProject(MoveInput, GetUpOrientation(MODE_Gravity)).GetSafeNormal() * MoveInput.Size();
	}

	Super::RequestPathMove(AdjustedMoveInput);
}

bool URadicalMovementComponent::CanStartPathFollowing() const
{
	// Can't nav if we have root motion or our owner is invalid
	if (!CharacterOwner || HasAnimRootMotion())
	{
		return false;
	}

	// Can't nav if we're simulation physics
	if (CharacterOwner->GetRootComponent() && CharacterOwner->GetRootComponent()->IsSimulatingPhysics())
	{
		return false;
	}

	return Super::CanStartPathFollowing();
}

bool URadicalMovementComponent::CanStopPathFollowing() const
{
	return !IsFalling();
}

float URadicalMovementComponent::GetPathFollowingBrakingDistance(float MaxSpeed) const
{
	if (bUseFixedBrakingDistanceForPaths)
	{
		return FixedPathBrakingDistance;
	}

	const float BrakingDeceleration = FMath::Abs(MovementData->GetMaxBrakingDeceleration(PhysicsState));

	// Character wont be able to stop with negative or near zero deceleration, use MaxSpeed for path length calculations
	const float BrakingDistance = (BrakingDeceleration < UE_SMALL_NUMBER) ? MaxSpeed : (FMath::Square(MaxSpeed) / (2.f * BrakingDeceleration));
	return BrakingDistance;
}

// NOTE: This can go inside of MovementData, doesn't belong here
bool URadicalMovementComponent::ApplyRequestedMove(float DeltaTime, float MaxAccel, float MaxSpeed, float Friction,
	float BrakingDeceleration, FVector& OutAcceleration, float& OutRequestedSpeed)
{
	if (bHasRequestedVelocity)
	{
		const float RequestedSpeedSquared = RequestedVelocity.SizeSquared();
		if (RequestedSpeedSquared < UE_KINDA_SMALL_NUMBER)
		{
			return false;
		}

		// Compute requested speed from path following
		float RequestedSpeed = FMath::Sqrt(RequestedSpeedSquared);
		const FVector RequestedMoveDir = RequestedVelocity / RequestedSpeed;
		RequestedSpeed = (bRequestedMoveWithMaxSpeed ? MaxSpeed : FMath::Min(MaxSpeed, RequestedSpeed));

		// Compute actual requested velocity
		const FVector MoveVelocity = RequestedMoveDir * RequestedSpeed;

		// Compute acceleration. Use MaxAccel to limit speed increase (1% buffer)
		FVector NewAcceleration = FVector::ZeroVector;
		const float CurrentSpeedSq = Velocity.SizeSquared();
		if (ShouldComputeAccelerationToReachRequestedVelocity(RequestedSpeed))
		{
			// Turn in the same manner as with input acceleration
			const float VelSize = FMath::Sqrt(CurrentSpeedSq);
			Velocity = Velocity - (Velocity - RequestedMoveDir * VelSize) * FMath::Min(DeltaTime * Friction, 1.f);

			// How much do we need to accelerate to get to the new velocity?
			NewAcceleration = ((MoveVelocity - Velocity)/DeltaTime);
			NewAcceleration = NewAcceleration.GetClampedToMaxSize(MaxAccel);
		}
		else
		{
			// Just set velocity directly.
			Velocity = MoveVelocity;
		}

		// Copy to out params
		OutRequestedSpeed = RequestedSpeed;
		OutAcceleration = NewAcceleration;

		return true;
	}

	return false;
}

bool URadicalMovementComponent::ShouldComputeAccelerationToReachRequestedVelocity(const float RequestedSpeed) const
{
	// Compute acceleration if accelerating toward requested speed, 1% buffer.
	return bRequestedMoveUseAcceleration && Velocity.SizeSquared() < FMath::Square(RequestedSpeed * 1.01f);
}

#pragma endregion AI 


#pragma region Utility

FVector URadicalMovementComponent::GetCapsuleExtent(const EShrinkCapsuleExtent ShrinkMode,
	const float CustomShrinkAmount) const
{
	check(CharacterOwner);

	float Radius, HalfHeight;
	CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleSize(Radius, HalfHeight);
	FVector CapsuleExtent(Radius, Radius, HalfHeight);

	float RadiusEpsilon = 0.f;
	float HeightEpsilon = 0.f;

	switch(ShrinkMode)
	{
		case SHRINK_None:
			return CapsuleExtent;

		case SHRINK_RadiusCustom:
			RadiusEpsilon = CustomShrinkAmount;
			break;

		case SHRINK_HeightCustom:
			HeightEpsilon = CustomShrinkAmount;
			break;
		
		case SHRINK_AllCustom:
			RadiusEpsilon = CustomShrinkAmount;
			HeightEpsilon = CustomShrinkAmount;
			break;

		default:
			UE_LOG(LogRMCMovement, Warning, TEXT("Unknown EShrinkCapsuleExtent in URadicalMovementComponent::GetCapsuleExtent"));
			break;
	}

	// Don't shrink to zero extent.
	const FVector::FReal MinExtent = UE_KINDA_SMALL_NUMBER * 10;
	CapsuleExtent.X = FMath::Max<FVector::FReal>(CapsuleExtent.X - RadiusEpsilon, MinExtent);
	CapsuleExtent.Y = CapsuleExtent.X;
	CapsuleExtent.Z = FMath::Max<FVector::FReal>(CapsuleExtent.Z - HeightEpsilon, MinExtent);

	return CapsuleExtent;
}

FCollisionShape URadicalMovementComponent::GetCapsuleCollisionShape(const EShrinkCapsuleExtent ShrinkMode,
	const float CustomShrinkAmount)
{
	FVector Extent = GetCapsuleExtent(ShrinkMode, CustomShrinkAmount);
	return FCollisionShape::MakeCapsule(Extent);
}

void URadicalMovementComponent::VisualizeMovement() const
{
	float HeightOffset = 0.f;
	const float OffsetPerElement = 10.0f;

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	const FVector TopOfCapsule = GetActorLocation() + GetUpOrientation(MODE_PawnUp) * CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	
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

	// Physical Acceleration
	{
		const FColor DebugColor = FColor::Purple;
		HeightOffset += OffsetPerElement;
		const float MaxAccelerationLineLength = 100.f;
		const FVector DebugLocation = TopOfCapsule + HeightOffset * GetUpOrientation(MODE_PawnUp);
		DrawDebugDirectionalArrow(GetWorld(), DebugLocation - 5.f * GetUpOrientation(MODE_PawnUp), 
			DebugLocation - 5.f * GetUpOrientation(MODE_PawnUp) + PhysicalAcceleration.GetSafeNormal(UE_SMALL_NUMBER) * MaxAccelerationLineLength,
			25.f, DebugColor, false, -1.f, (uint8)'\000', 8.f);

		FString DebugText = FString::Printf(TEXT("Physical Accel: %s"), *PhysicalAcceleration.ToCompactString());
		DrawDebugString(GetWorld(), DebugLocation, DebugText, nullptr, DebugColor, 0.f, true);
	}
	
	// Input Acceleration
	{
		const FColor DebugColor = FColor::Yellow;
		HeightOffset += OffsetPerElement;
		const float MaxAccelerationLineLength = 100.f;
		const FVector DebugLocation = TopOfCapsule + HeightOffset * GetUpOrientation(MODE_PawnUp);
		DrawDebugDirectionalArrow(GetWorld(), DebugLocation - 5.f * GetUpOrientation(MODE_PawnUp), 
			DebugLocation - 5.f * GetUpOrientation(MODE_PawnUp) + InputAcceleration.GetSafeNormal(UE_SMALL_NUMBER) * MaxAccelerationLineLength,
			25.f, DebugColor, false, -1.f, (uint8)'\000', 8.f);

		FString DebugText = FString::Printf(TEXT("Input Accel: %s"), *InputAcceleration.ToCompactString());
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

void URadicalMovementComponent::VisualizeTrajectories(const FVector& OldLocation) const
{
	// TODO: For now just drawing base trajectory
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

	int32 DebugVal = RMCCVars::VisualizeTrajectories;

	// Setup draw colors
	static const FColor RootLineColor = FColor::Turquoise;
	static const FColor RootNormalColor = FColor::Blue;
	static const FColor MeshLineColor = FColor::Orange;
	static const FColor MeshNormalColor = FColor::Red;
	static const FColor ConnectingLineColor = FColor::Yellow;
	const float VerticalOffset = DebugVal < 2 ? 30.f : 0.f;

	// Get Current Info
	 FVector CurrentRootLocation = UpdatedComponent->GetComponentLocation() + UpdatedComponent->GetUpVector() * VerticalOffset;
	 FVector OldRootLocation = OldLocation + UpdatedComponent->GetUpVector() * VerticalOffset;
	const FVector CurrentMeshLocation = (CharacterOwner ? CharacterOwner->GetMesh()->GetBoneLocation(TrajectoryBoneName) : FVector::ZeroVector);
	const FVector MeanMeshPoint = (DebugOldMeshLocation + CurrentMeshLocation) / 2.f;
	 FVector MeanRootPoint = (OldRootLocation + CurrentRootLocation) / 2.f;

	// Move root location up a bit for better visibility
	
	
	// Draw Capsule Movement Connected Trajectory If Not Equal
	if (!CurrentRootLocation.Equals(OldRootLocation))
	{
		// If at feet, push them down by the capsule half height
		if (DebugVal > 2)
		{
			CurrentRootLocation -= UpdatedComponent->GetUpVector() * CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
			OldRootLocation -= UpdatedComponent->GetUpVector() * CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
			MeanRootPoint -= UpdatedComponent->GetUpVector() * CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		}
		DrawDebugLine(GetWorld(), OldRootLocation, CurrentRootLocation, RootLineColor, false, 5.f, 0, 4.f);
		const FVector VelUp = UpdatedComponent->GetUpVector();//CurrentRootLocation - OldRootLocation) ^ UpdatedComponent->GetRightVector();
		DrawDebugLine(GetWorld(), MeanRootPoint, MeanRootPoint + 10.f * VelUp.GetSafeNormal(), RootNormalColor, false, 5.f, 0.f, 1.f);
	}
	
	// Draw Mesh Movement Connected Trajectory w/ Normals
	if (!CurrentMeshLocation.Equals(DebugOldMeshLocation) && DebugVal < 2)
	{
		DrawDebugLine(GetWorld(), DebugOldMeshLocation, CurrentMeshLocation, MeshLineColor, false, 5.f, 0, 4.f);
		const FVector VelUp = (CharacterOwner ? CharacterOwner->GetMesh()->GetBoneQuaternion(TrajectoryBoneName).GetUpVector() : FVector::ZeroVector);
		DrawDebugLine(GetWorld(), MeanMeshPoint, MeanMeshPoint + 10.f * VelUp.GetSafeNormal(), MeshNormalColor, false, 5.f, 0.f, 1.f);

		// Draw connected line
		DrawDebugLine(GetWorld(), MeanMeshPoint, MeanRootPoint, ConnectingLineColor, false, 5.f, 0.f, 1.5f);
	}
#endif 
}

void URadicalMovementComponent::VisualizeAccelerationCurve(const FVector& OldRootLocation) const
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

	// Draw Color
	static const FColor CurveColor = FColor::Green;

	float HeightFactor = RMCCVars::VisualizeAccelerationCurve * 1.f; 
	
	float OldHeightFactor = HeightFactor * DebugOldVelocity.Length() / MovementData->MaxSpeed;
	float NewHeightFactor = HeightFactor * Velocity.Length() / MovementData->MaxSpeed;
	
	DrawDebugLine(GetWorld(), OldRootLocation + UpdatedComponent->GetUpVector() * OldHeightFactor, UpdatedComponent->GetComponentLocation() + UpdatedComponent->GetUpVector() * NewHeightFactor, CurveColor, false, 10.f, 0.f, 4.f);
#endif
}


void URadicalMovementComponent::DisplayDebug(UCanvas* Canvas, const FDebugDisplayInfo& DebugDisplay, float& YL, float& YPos)
{
	FDisplayDebugManager& DisplayDebugManager = Canvas->DisplayDebugManager;

	FString T = FString::Printf(TEXT("----- PHYSICS BINDINGS -----"));
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	// Physics Bindings
	DisplayDebugManager.SetDrawColor(FColor::Red);
	DisplayDebugManager.DrawString(T);
	DisplayDebugManager.SetDrawColor(FColor::Orange);
	T = FString::Printf(TEXT("Primary Velocity Binding: %s"), *(VelocityBindings.PriorityBinding.IsBound() ? VelocityBindings.PriorityBinding.GetUObject()->GetName() : "NONE"));
	DisplayDebugManager.DrawString(T);
	T = FString::Printf(TEXT("Secondary Velocity Binding: %s"), *(VelocityBindings.SecondaryBinding.IsBound() ? VelocityBindings.SecondaryBinding.TryGetBoundFunctionName().ToString() : "NONE"));
	DisplayDebugManager.DrawString(T);
	T = FString::Printf(TEXT("Primary Rotation Binding: %s"), *(RotationBindings.PriorityBinding.IsBound() ? RotationBindings.PriorityBinding.GetUObject()->GetName() : "NONE"));
	DisplayDebugManager.DrawString(T);
	T = FString::Printf(TEXT("Secondary Rotation Binding: %s"), *(RotationBindings.SecondaryBinding.IsBound() ? RotationBindings.SecondaryBinding.TryGetBoundFunctionName().ToString() : "NONE"));
	DisplayDebugManager.DrawString(T);
#endif
	
	DisplayDebugManager.SetDrawColor(FColor::Yellow);
	T = FString::Printf(TEXT("----- MOVE STATE -----"));
	DisplayDebugManager.DrawString(T);
	
	DisplayDebugManager.SetDrawColor(PhysicsState == STATE_Grounded ? FColor::Orange : FColor::Red);
	
	// Movement Information
	T = FString::Printf(TEXT("Movement State: %s - Crouched: %i"), (PhysicsState != STATE_Falling ? (PhysicsState == STATE_Grounded ? TEXT("Grounded") : TEXT("General")) : TEXT("Aerial")), IsCrouching());
	DisplayDebugManager.DrawString(T);

	DisplayDebugManager.SetDrawColor(CurrentFloor.bUnstableFloor ? FColor::Red : FColor::Green);
	T = FString::Printf(TEXT("CurrentFloor.bUnstableFloor: %s"), BOOL2STR(CurrentFloor.bUnstableFloor));
	DisplayDebugManager.DrawString(T);

	DisplayDebugManager.SetDrawColor((!CurrentFloor.bBlockingHit ) ? FColor::Red : FColor::Green);
	T = FString::Printf(TEXT("CurrentFloor.bBlockingHit: %s"), BOOL2STR((CurrentFloor.bBlockingHit)));
	DisplayDebugManager.DrawString(T);

	DisplayDebugManager.SetDrawColor(FColor::White);
	T = FString::Printf(TEXT("Velocity: %s"), *Velocity.ToCompactString());
	DisplayDebugManager.DrawString(T);

	T = FString::Printf(TEXT("Speed: %f"), FMath::Abs(Velocity.Size()));
	DisplayDebugManager.DrawString(T);

	T = FString::Printf(TEXT("Acceleration: %s"), *InputAcceleration.ToCompactString());
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

	T = FString::Printf(TEXT("Capsule Radius: %f"), CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleRadius());
	DisplayDebugManager.DrawString(T);
}


#pragma endregion Utility
