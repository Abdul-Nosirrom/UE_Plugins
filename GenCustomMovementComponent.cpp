// Fill out your copyright notice in the Description page of Project Settings.


#include "GenCustomMovementComponent.h"

#include "DrawDebugHelpers.h"
#include "FlatCapsuleComponent.h"
#include "GMC_LOG.h"
#include "Components/BoxComponent.h"
#include "Components/SphereComponent.h"

#pragma region GENMOVEMENT_OVERRIDES

void UGenCustomMovementComponent::DebugMessage(const FColor color, const FString message) const
{
	GEngine->AddOnScreenDebugMessage(-1, 0.005f, color, message);
}

UGenCustomMovementComponent::UGenCustomMovementComponent()
{
	
}

#if WITH_EDITOR

void UGenCustomMovementComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
}

#endif

void UGenCustomMovementComponent::BeginPlay()
{
	// I think this just ensures that the pawn is using GenPawn
	if (!PawnOwner || !GetGenPawnOwner())
	{
		Super::BeginPlay();
		return;
	}

	// Get a reference to the skeletal mesh
	const auto Mesh = PawnOwner->FindComponentByClass(USkeletalMeshComponent::StaticClass());
	if (Mesh)
	{
		// Sets skeletal mesh here, haven't gotten to it yet
		SetSkeletalMeshReference(Cast<USkeletalMeshComponent>(Mesh));
		checkGMC(SkeletalMesh == Mesh);
	}

	// Set root collision parameters
	CurrentRootCollisionShape = static_cast<uint8>(Super::GetRootCollisionShape());
	CurrentRootCollisionExtent = Super::GetRootCollisionExtent();
	
	// Set start grounding status
	GroundingStatus = EGroundingStatus::Grounded;

	// Intended input mode for this component is Absolute Z, meaning XY is relative to controller view but Z is to world
	// TODO: Might not need this, check up on it later!
	GetGenPawnOwner()->SetInputMode(EInputMode::AbsoluteZ);

	Super::BeginPlay();
}

void UGenCustomMovementComponent::GenReplicatedTick_Implementation(float DeltaTime)
{
	if (UpdatedComponent->IsSimulatingPhysics() || ShouldSkipUpdate(DeltaTime))
	{
		DisableMovement();
		return;
	}

	AutoResolvePenetration();

	PerformMovement(DeltaTime);

	if (bEnablePhysicsInteraction)
	{
		ApplyDownwardForce(DeltaTime);
		ApplyRepulsionForce(DeltaTime);
	}

	if (UpdatedComponent->IsSimulatingPhysics())
	{
		// Physics simulation may have been activated within this iteration. We still let the current tick finish in this case.
		DisableMovement();
		return;
	}

}


void UGenCustomMovementComponent::SetUpdatedComponent(USceneComponent* NewUpdatedComponent)
{
	if (!NewUpdatedComponent) return;

	const auto NewGenPawnOwner = Cast<AGenPawn>(NewUpdatedComponent->GetOwner());
	if (!NewGenPawnOwner)
	{
		UE_LOG(LogActor, Error, TEXT("New updated component must be owned by a pawn of type <AGenPawn>."));
		return;
	}
	
	if (
		!(Cast<UFlatCapsuleComponent>(NewUpdatedComponent)
		|| Cast<UCapsuleComponent>(NewUpdatedComponent)
		|| Cast<UBoxComponent>(NewUpdatedComponent)
		|| Cast<USphereComponent>(NewUpdatedComponent))
		)
	{
		// New updated component must have a supported collision shape. No log message since we don't want to give an error while editing the
		// Blueprint.
		return;
	}

	// Bind an event when our collision overlaps with something
	if (IsValid(UpdatedPrimitive) && UpdatedPrimitive->OnComponentBeginOverlap.IsBound())
	{
		UpdatedPrimitive->OnComponentBeginOverlap.RemoveDynamic(this, &UGenCustomMovementComponent::RootCollisionTouched);
	}
	
	Super::SetUpdatedComponent(NewUpdatedComponent);

	if (!IsValid(UpdatedComponent) || !IsValid(UpdatedPrimitive))
	{
		UE_LOG(LogActor, Error, TEXT("New updated component is not valid."));
		DisableMovement();
		return;
	}

	if (PawnOwner->GetRootComponent() != UpdatedComponent)
	{
		UE_LOG(LogActor, Error, TEXT("New updated component must be the root component."));
		PawnOwner->SetRootComponent(UpdatedComponent);
	}

	if (bEnablePhysicsInteraction)
	{
		UpdatedPrimitive->OnComponentBeginOverlap.AddUniqueDynamic(this, &UGenCustomMovementComponent::RootCollisionTouched);
	}
}

void UGenCustomMovementComponent::HandleImpact(const FHitResult& Impact, float TimeSlice, const FVector& MoveDelta)
{
	if (!Impact.bBlockingHit)
	{
		//checkfGMC(false, TEXT("HandleImpact should only be called for blocking hits."))
		return;
	}

	if (const auto PFAgent = GetPathFollowingAgent())
	{
		PFAgent->OnMoveBlockedBy(Impact);
	}

	if (const auto OtherPawn = Cast<APawn>(Impact.GetActor()))
	{
		NotifyBumpedPawn(OtherPawn);
	}

	if (bEnablePhysicsInteraction)
	{
		ApplyImpactPhysicsForces(Impact, GetTransientAcceleration(), GetVelocity());
	}
}


#pragma region UPDATES

void UGenCustomMovementComponent::PreMovementUpdate_Implementation(float DeltaSeconds)
{
	SetPhysDeltaTime(DeltaSeconds);
	ProcessedInputVector = FVector::ZeroVector;
	LedgeFallOffDirection = FVector::ZeroVector;
	UpdateFloor(CurrentFloor, FloorTraceLength);
	RootMotionParams = FRootMotionMovementParams();
}

void UGenCustomMovementComponent::PostPhysicsUpdate_Implementation(float DeltaSeconds)
{
	// Set the physics delta time again in case it was altered
	SetPhysDeltaTime(DeltaSeconds, false);

	// Ensure current floor is up-to-date after the pawn was moved
	UpdateFloor(CurrentFloor, FloorTraceLength);
}


bool UGenCustomMovementComponent::UpdateMovementModeDynamic_Implementation(const FFloorParams& Floor, float DeltaSeconds)
{
	// If we're airborne, reset the ShouldUnground flag
	if (IsAirborne())
	{
		if (bShouldUnground)
		{
			bShouldUnground = false;
			return true;
		}
	}
	else if (IsGrounded())
	{
		// If we should unground, set us to aerial
		if (bShouldUnground)
		{
			FHitResult Hit;
			SafeMoveUpdatedComponent({0.f, 0.f, MAX_DISTANCE_TO_FLOOR}, UpdatedComponent->GetComponentQuat(), true, Hit);
			SetGroundingStatus(EGroundingStatus::Aerial);
			bShouldUnground = false;
			return true;
		}

		checkGMC(!bShouldUnground); // This should be false by here

		// We should continue to stick to the ground if we did not receive external upward force and other constrains are not violated (e.g.
		// to not lose contact with a dynamic base we are standing on that is moving downwards).
		const bool bShapeHitWithinStepDownHeight =
		  Floor.HasValidShapeData() && Floor.GetShapeDistanceToFloor() <= MaxStepDownHeight + MAX_DISTANCE_TO_FLOOR;
		const bool bLineHitWithinStepDownHeight =
		  Floor.HasValidLineData() && Floor.GetLineDistanceToFloor() <= MaxStepDownHeight + MAX_DISTANCE_TO_FLOOR;
		const bool bShouldFallOffLedge =
		  !bLineHitWithinStepDownHeight
		  && Floor.HasValidShapeData()
		  && IsExceedingFallOffThreshold(Floor.ShapeHit().ImpactPoint, GetLowerBound(), UpdatedComponent->GetComponentLocation());
		if (bShapeHitWithinStepDownHeight && !bShouldFallOffLedge && HitWalkableFloor(Floor.ShapeHit()))
		{
			checkGMC(IsGrounded())
			return true;
		}
	}

	return false;
}

void UGenCustomMovementComponent::UpdateMovementModeStatic_Implementation(const FFloorParams& Floor, float DeltaSeconds)
{
	checkGMC(LedgeFallOffDirection.IsZero());
	bool bShouldFallOffLedge = false;
	if (Floor.HasValidShapeData())
	{
		// TODO: Verify some of this is what we want, also some stuff is in centimeters
		// We perform some checks on the floor
		const bool bShapeHitWithinFloorRange = Floor.GetShapeDistanceToFloor() <= MAX_DISTANCE_TO_FLOOR;
		const bool bLineHitWithinFloorRange = Floor.HasValidLineData() && Floor.GetLineDistanceToFloor() <= MAX_DISTANCE_TO_FLOOR;
		const bool bHitFloorIsWalkable =
		  bShapeHitWithinFloorRange && HitWalkableFloor(Floor.ShapeHit()) || bLineHitWithinFloorRange && HitWalkableFloor(Floor.LineHit());
		const bool bVelocityPointsAwayFromGround = (Floor.ShapeHit().ImpactNormal | GetVelocity().GetSafeNormal()) > DOT_PRODUCT_75;
		const bool bShouldRemainAirborne = IsAirborne() && bVelocityPointsAwayFromGround; // Note: Removed the Z velocity check because we want to keep Z velocity when grounded
		const bool bLineHitWithinStepDownHeight =
		  Floor.HasValidLineData() && Floor.GetLineDistanceToFloor() <= MaxStepDownHeight + MAX_DISTANCE_TO_FLOOR;
		bShouldFallOffLedge =
		  !bLineHitWithinStepDownHeight
		  && IsExceedingFallOffThreshold(Floor.ShapeHit().ImpactPoint, GetLowerBound(), UpdatedComponent->GetComponentLocation());

		if (
			(bShapeHitWithinFloorRange || bLineHitWithinFloorRange)
			&& bHitFloorIsWalkable
			&& !bShouldRemainAirborne
			)
		{
			const FHitResult& Hit = Floor.ShapeHit().IsValidBlockingHit() ? Floor.ShapeHit() : Floor.LineHit();
			checkGMC(Hit.IsValidBlockingHit())

			// Switching to grounded movement like this should trigger a hit notification as well.
			if (const auto UpdatedComponentPrimitive = Cast<UPrimitiveComponent>(UpdatedComponent))
			{
				PawnOwner->DispatchBlockingHit(UpdatedComponentPrimitive, Hit.GetComponent(), true, Hit);
			}

			// Process landing and set the movement mode to grounded.
			ProcessLanded(Hit, DeltaSeconds, false);

			return; // Forgot to add this which kept our movement mode always to aerial i think?
		}
	}

	// TODO: Study this some more
	SetGroundingStatus(EGroundingStatus::Aerial);

	if (bShouldFallOffLedge)
	{
		const FVector ImpactToCenter = (UpdatedComponent->GetComponentLocation() - Floor.ShapeHit().ImpactPoint);
		LedgeFallOffDirection = FVector(ImpactToCenter.X, ImpactToCenter.Y, 0.f).GetSafeNormal();
	}
}

void UGenCustomMovementComponent::OnMovementModeUpdated_Implementation(EGroundingStatus PreviousMovementMode)
{
	if (IsGrounded()) MaintainDistanceToFloor(CurrentFloor);
}


#pragma endregion UPDATES

#pragma region COLLISION_SETUP

USceneComponent* UGenCustomMovementComponent::SetRootCollisionShape(EGenCollisionShape NewCollisionShape, const FVector& Extent, FName Name/*not used*/)
{
	if (NewCollisionShape >= EGenCollisionShape::Invalid)
	{
		checkGMC(false)
		checkGMC(PawnOwner)
		if (!PawnOwner) return nullptr;
		const auto OriginalRootComponent = PawnOwner->GetRootComponent();
		checkGMC(OriginalRootComponent)
		return OriginalRootComponent;
	}

	// Use the appropriate constant as name to avoid errors with conflicting names.
	FName DynamicName;
	switch (NewCollisionShape)
	{
	case EGenCollisionShape::VerticalCapsule:
		DynamicName = ROOT_NAME_DYNAMIC_CAPSULE;
		break;
	case EGenCollisionShape::HorizontalCapsule:
		DynamicName = ROOT_NAME_DYNAMIC_FLAT_CAPSULE;
		break;
	case EGenCollisionShape::Box:
		DynamicName = ROOT_NAME_DYNAMIC_BOX;
		break;
	case EGenCollisionShape::Sphere:
		DynamicName = ROOT_NAME_DYNAMIC_SPHERE;
		break;
	case EGenCollisionShape::Invalid:
	case EGenCollisionShape::MAX:
	default: checkNoEntry();
	}
	const auto NewRootComponent = Super::SetRootCollisionShape(NewCollisionShape, Extent, DynamicName);
	CurrentRootCollisionShape = static_cast<uint8>(NewCollisionShape);
	// "Extent" could have been altered if it is not a valid extent vector so we query the actual root collision extent directly through the
	// parent function.
	CurrentRootCollisionExtent = Super::GetRootCollisionExtent();
	return NewRootComponent;
}

EGenCollisionShape UGenCustomMovementComponent::GetRootCollisionShape() const
{
	return static_cast<EGenCollisionShape>(CurrentRootCollisionShape);
}

void UGenCustomMovementComponent::SetRootCollisionExtent(const FVector& NewExtent, bool bUpdateOverlaps)
{
	Super::SetRootCollisionExtent(NewExtent, bUpdateOverlaps);
	// "NewExtent" could have been altered if it is not a valid extent vector so we query the actual root collision extent directly through
	// the parent function.
	CurrentRootCollisionExtent = Super::GetRootCollisionExtent();
}

FVector UGenCustomMovementComponent::GetRootCollisionExtent() const
{
	return CurrentRootCollisionExtent;
}



#pragma endregion COLLISION_SETUP



#pragma endregion GENMOVEMENT_OVERRIDES


#pragma region APPLYING_DEFAULTS

bool UGenCustomMovementComponent::CanMove() const
{
	return Super::CanMove() && GroundingStatus != EGroundingStatus::None && !bStuckInGeometry;
}

//TODO: Avoidance stuff here not yet implemented
void UGenCustomMovementComponent::HaltMovement()
{
	Super::HaltMovement();
	bShouldUnground = false;
	bHasAnimRootMotion = false;
	//RequestedVelocity = FVector::ZeroVector;
	//bIsUsingAvoidanceInternal = false;
	//ResetAvoidanceData();
}

void UGenCustomMovementComponent::DisableMovement()
{
	HaltMovement();
	SetGroundingStatus(EGroundingStatus::None);
}


// TODO: Needs finishing for input handling
void UGenCustomMovementComponent::ApplyRotation(bool bIsDirectBotMove, float DeltaSeconds)
{
	if (HasAnimRootMotion())
	{
		return;
	}

	if (RotationHandling == ERotationHandling::None) return;

	// Keeping it like this for now for testing
	RotateYawTowardsDirectionSafe(GetVelocity(), RotationRate, DeltaSeconds);
	
	return;
	
	// TODO: Come back to this once input is figured out some more
	if (RotationHandling == ERotationHandling::OrientToInput)
	{
		if (!bOrientToGroundNormal)
		{
			//RotateYawTowardsDirectionSafe(Controller->)
		}
	}
	else if (RotationHandling == ERotationHandling::OrientToVelocity)
	{
		if (bOrientToGroundNormal)
		{
			
		}

		// This does it relative to the plane defined by the character up, so we can handle it earlier
		RotateYawTowardsDirectionSafe(GetVelocity(), RotationRate, DeltaSeconds);
	}
}

#pragma endregion APPLYING_DEFAULTS

#pragma region PHYSICS_METHODS

// TODO: FINISH THIS
void UGenCustomMovementComponent::PerformMovement(float DeltaSeconds)
{
	PreMovementUpdate(DeltaSeconds);

	// I assume this basically freezes our character and the animation tick
	if (!CanMove())
	{
		BlockSkeletalMeshPoseTick();
		HaltMovement();
		return;
	}

	{
		// Grounding Status Update
		EGroundingStatus PreviousGroundingStatus = GetGroundingStatus();

		if (!UpdateMovementModeDynamic(CurrentFloor, DeltaSeconds))
		{
			UpdateMovementModeStatic(CurrentFloor, DeltaSeconds);
		}

		OnMovementModeUpdated(PreviousGroundingStatus);
	}

	// Get move input vector comes for our parent class (InputVector has to be binded!!!!) wanna handle it differently later on
	// so i dont have to do any name-matching via strings
	ProcessedInputVector = PreProcessInputVector(GetMoveInputVector());


	RunPhysics(DeltaSeconds);

	const FVector VelocityBeforeMovementUpdate = GetVelocity();

	// Preferred entry point for implementing movement logic
	MovementUpdate(DeltaSeconds);

	bHasAnimRootMotion = false;
	bool bSimulatedPoseTick{false};
	if (ShouldTickPose(&bSimulatedPoseTick))
	{
		TickPose(DeltaSeconds, bSimulatedPoseTick);
	}

	// TODO: Check the unground thing, might need to handle it differently because I want a non-zero Z-Velocity when grounded
	
	PostMovementUpdate(DeltaSeconds);
}


void UGenCustomMovementComponent::RunPhysics(float DeltaSeconds)
{
	PrePhysicsUpdate(DeltaSeconds);

	// Don't update physics or movement or anything if we're set to None!
	if (GroundingStatus == EGroundingStatus::None) return;

	// TODO: Figure out how to make this generic
	SolvePhysics(DeltaSeconds);

	PostPhysicsUpdate(DeltaSeconds);
}

// TODO: FINISH THIS
void UGenCustomMovementComponent::SolvePhysics(float DeltaSeconds)
{
	// Calculate ground angle here to initiate debugger, i can see that the LocationDelta has zero Z.
	float ImpactNormalAngle = FMath::RadiansToDegrees(FMath::Acos(CurrentFloor.ShapeHit().ImpactNormal | UpdatedComponent->GetUpVector()));
	if (ImpactNormalAngle > 50.f)
	{
		DebugMessage(FColor::Red, TEXT("Debugging"));
	}

	
	const FVector OldVelocity = GetVelocity();
	CalculateVelocity(DeltaSeconds);
	
	if (GroundingStatus == EGroundingStatus::Grounded)
	{
		// Velocity of the moving base we are on should be calculated before moving along it, but applied afterwards
		const FVector PlatformVelocity = bMoveWithBase ? ComputeBaseVelocity(CurrentFloor.ShapeHit().GetComponent()) : FVector{0};
		const FVector LocationDelta = GetVelocity() * DeltaSeconds;
		GroundedPhysics(LocationDelta, CurrentFloor, DeltaSeconds);
		if (bMoveWithBase) MoveWithBase(PlatformVelocity, CurrentFloor, DeltaSeconds);
	}
	else if (GroundingStatus == EGroundingStatus::Aerial)
	{
		// TODO: Implement the movement here too
		// Not sure why he uses midpoint integration to compute the location delta here and not above
		const FVector LocationDelta = 0.5f * (OldVelocity + GetVelocity()) * DeltaSeconds;
		AirbornePhysics(LocationDelta, DeltaSeconds);
		
	}
}

// TODO: BUG ON STEEP SLOPES DETECTING SLOPE EDGE AS ACTUAL SLOPE TANGENT
void UGenCustomMovementComponent::CalculateVelocity(float DeltaSeconds)
{
	if (IsGrounded())
	{
		// Project velocity to ground
		const FVector PlaneNormal = CurrentFloor.ShapeHit().Normal;
		FVector ProjectedVelocity = (PlaneNormal ^ (GetVelocity().GetSafeNormal() ^ PlaneNormal)) * GetVelocity().Size();
		DrawDebugDirectionalArrow(GetWorld(), CurrentFloor.ShapeHit().ImpactPoint, CurrentFloor.ShapeHit().ImpactPoint + ProjectedVelocity.GetSafeNormal()*200.0f, 100.0f, FColor::Purple, false, -1, 0, 10.0f);
		UpdateVelocity(FVector{GetVelocity().X, GetVelocity().Y, ProjectedVelocity.Z});
	}

	// TODO: There's some more shit to do here so keep track of it later
	ApplyRotation(false, DeltaSeconds);
	
	//PostProcessPawnVelocity(); Blueprint shit
}


// TODO: BIG CHANGES IN THE SLOPE WOULD PROLLY BE HANDLED HERE
// Might actually be fine handling it like this (so long as the condition checks are valid) if we still keep that Z-Velocity,
// we're essentially ignoring it going up the slopes though
FVector UGenCustomMovementComponent::GroundedPhysics(const FVector& LocationDelta, FFloorParams& Floor, float DeltaSeconds)
{
	// TODO: Check how to not project velocity on steps to avoid jitter, the point of the bool below
	bool bSteppedUp = false;
	// TODO: Double check, might want to keep the dZ if we're gonna project the velocity
	// Maybe all the things slopes should handle here are just whether it is a valid slope or not, e.g valid grounding stauts
	// and then within the movement update itself it should read that and handle projecting the velocity.
	const FVector MoveDelta = FVector(LocationDelta.X, LocationDelta.Y, LocationDelta.Z);
	// No need to step through solving if we're not moving
	if (MoveDelta.IsNearlyZero())
	{
		return FVector::ZeroVector;
	}
	
	// Want this to basically undo parts of the function call if need be (in the case of ledges below)
	FScopedMovementUpdate ScopedMovement(UpdatedComponent, EScopedUpdate::DeferredUpdates);

	
	FHitResult MoveHitResult;
	const float MaxSpeed = GetVelocity().Size();
	const FVector StartLocation = UpdatedComponent->GetComponentLocation();

	// TODO: Study this
	const bool bStartedWithinLineMaxStepDownHeight =
		Floor.HasValidLineData() && Floor.GetLineDistanceToFloor() <= MaxStepDownHeight + MAX_DISTANCE_TO_FLOOR;

	// TODO: Is this how we want to handle it? Should we instead let it update velocity?
	FVector RampVector = ComputeRampVector(MoveDelta, Floor.ShapeHit());
	SafeMoveUpdatedComponent(RampVector, UpdatedComponent->GetComponentQuat(), true, MoveHitResult);
	const float MoveHitTime = MoveHitResult.Time;
	const float MoveHitTimeRemaining = 1.0f - MoveHitTime;
	const FVector MoveDeltaRemaining = MoveDelta * MoveHitTimeRemaining;
	float RemainingDeltaSeconds = DeltaSeconds * MoveHitTimeRemaining;

	// If collision is inside other collision due to the movement, handle it
	if (MoveHitResult.bStartPenetrating)
	{
		HandleImpact(MoveHitResult, DeltaSeconds, RampVector);
		// Try again by sliding along the surface with the full location delta
		SlideAlongSurface(MoveDelta, 1.0f, MoveHitResult.Normal, MoveHitResult, true);
	}
	else if (MoveHitResult.IsValidBlockingHit())
	{
		// Update the floor
		UpdateFloor(Floor, FloorTraceLength);
		// TODO: Check the normal Z check, I think this is also a limitation of arbitrary slopes
		if (HitWalkableFloor(MoveHitResult) && MoveHitTime > 0.0f && MoveHitResult.Normal.Z > KINDA_SMALL_NUMBER)
		{
			// We hit another walkable surface
			RampVector = ComputeRampVector(MoveDeltaRemaining, MoveHitResult);
			SafeMoveUpdatedComponent(RampVector, UpdatedComponent->GetComponentQuat(), true, MoveHitResult);
		}
		else
		{
			// The current floor was updated by the move updated component call. The floor sweep shape may have hit the same unwalkable surface as
			// the move hit result but the floor line trace should still hit a walkable floor for us to try a step-up.
			if (CanStepUp(MoveHitResult) && HitWalkableFloor(Floor.LineHit()))
			{
				// Try to step up onto the barrier.
				FHitResult StepUpHit;
				
				if (!StepUp(MoveDeltaRemaining, MoveHitResult, Floor, &StepUpHit))
				{
					// The step up was not successful, handle the impact and slide along the surface instead
					HandleImpact(MoveHitResult, DeltaSeconds, RampVector);
					SlideAlongSurface(MoveDelta, MoveHitTimeRemaining, MoveHitResult.Normal, MoveHitResult, true);
					RemainingDeltaSeconds *= 1.f - MoveHitResult.Time;
				}
				else
				{
					bSteppedUp = true;
					// The step-up was successful.
					// @note We can also arrive at this point when hitting a wall (i.e. we didn't actually step up onto anything). This is correct
					// behaviour since it would be difficult to determine beforehand how high the barrier actually is. Sliding along the surface is
					// handled within the step-up function in this case (while processing the forward hit).
				}
			}
			else
			{
				HandleImpact(MoveHitResult, DeltaSeconds, RampVector);
				SlideAlongSurface(MoveDelta, MoveHitTimeRemaining, MoveHitResult.Normal, MoveHitResult, true);
				RemainingDeltaSeconds *= 1.f - MoveHitResult.Time;
			}
		}
		// If we collided with something we need to adjust the velocity
		float AppliedDeltaSeconds = DeltaSeconds - RemainingDeltaSeconds;
		UpdateVelocityFromMovedDistance(AppliedDeltaSeconds);
		// TODO: How does keeping Z velocity when on ground affect this? For now I'm just keeping it hence the added term
		// The new velocity cannot exceed the velocity that was used as the basis for the calculation of the location delta. By clamping we
		// avoid velocity spikes that can could occur due to minor location adjustments.
		UpdateVelocity(FVector(GetVelocity().X, GetVelocity().Y, GetVelocity().Z).GetClampedToMaxSize(MaxSpeed), AppliedDeltaSeconds);
	}

	FVector AppliedDelta = UpdatedComponent->GetComponentLocation() - StartLocation;
	// Make sure that after solving for collision and everything, to continue solving if it produced a move
	// Other case would be we walked into a wall, v * dT is non-zero, but after checking for collision it would be zero
	if (!AppliedDelta.IsNearlyZero())
	{
		UpdateFloor(Floor, FloorTraceLength);
		const bool bStillWithinLineMaxStepDownHeight =
			Floor.HasValidLineData() && Floor.GetLineDistanceToFloor() <= MaxStepDownHeight + MAX_DISTANCE_TO_FLOOR;

		if (HitWalkableFloor(Floor.ShapeHit()) && bStillWithinLineMaxStepDownHeight)
		{
			// This will also make the pawn stick to the floor again if we exceed the max step down height after moving
			MaintainDistanceToFloor(Floor);
		}
		else
		{
			// Updated Floor is not walkable
			if (!bCanWalkOffLedges && bStartedWithinLineMaxStepDownHeight && !bStillWithinLineMaxStepDownHeight)
			{
				// In this case we're set to not being able to walk off ledges, so remove the move and basically treat it as a wall
				// by zeroing velocity
				ScopedMovement.RevertMove();
				AppliedDelta = FVector::ZeroVector;
				UpdateVelocity(FVector::ZeroVector, DeltaSeconds);
				// Update the floor again once we reverted the move
				UpdateFloor(Floor, FloorTraceLength);
			}
		}
	}

	return AppliedDelta;
}

bool UGenCustomMovementComponent::AirbornePhysics(const FVector& LocationDelta, float DeltaSeconds)
{
	// Basically not moving so no need to run any solvers
	if (LocationDelta.IsNearlyZero()) return false;

	// Move and do a sweep
	FHitResult Hit;
	const FQuat PawnRotation = UpdatedComponent->GetComponentQuat();
	SafeMoveUpdatedComponent(LocationDelta, PawnRotation, true, Hit);

	// Above takes time, so we need to update our value of dT, Hit struct gives elapsed time
	float LastMoveTimeSlice = DeltaSeconds;
	float DeltaSecondsRemaining = DeltaSeconds * (1.f - Hit.Time);
	float DeltaSecondsApplied = DeltaSeconds - DeltaSecondsRemaining;

	if (Hit.bBlockingHit)
	{
		if (IsValidLandingSpot((Hit)))
		{
			// Pawn landed on walkable surface
			ProcessLanded(Hit, DeltaSecondsRemaining);
			// Next call will be to GroundedPhysics as it'll handle the rest after we've landed and have dT remaining
			SolvePhysics(DeltaSecondsRemaining);
			return true;
		}
		else
		{
			// We hit a wall
			HandleImpact(Hit, LastMoveTimeSlice, LocationDelta);
			// Similar to adjustvelocityfromhit, but removes what is applied onto the Z
			AdjustVelocityFromHitAirborne(Hit, DeltaSecondsApplied);
			
			const FVector OldHitNormal = Hit.Normal;
			const FVector OldHitImpactNormal = Hit.ImpactNormal;
			const FVector SavedDeltaX = GetVelocity() * DeltaSecondsRemaining;
			FVector Delta = ComputeSlideVector(LocationDelta, 1.f - Hit.Time, OldHitNormal, Hit);

			// Ensure we have enough dT to continue stepping through the integration, and make sure slide vector didn't alter
			// our direction too much for this branch
			if (DeltaSecondsRemaining >= MIN_DELTA_TIME && !DirectionsDiffer(Delta, SavedDeltaX))
			{
				// In this case, its okay to continue moving as the deflection vector since it did not change our direction
				SafeMoveUpdatedComponent(Delta, PawnRotation, true, Hit);

				if (Hit.bBlockingHit)
				{
					// We hit another wall, need to update dT with time consumed for the sweep
					LastMoveTimeSlice = DeltaSecondsRemaining;
					DeltaSecondsRemaining *= 1.f - Hit.Time;
					DeltaSecondsApplied = DeltaSeconds - DeltaSecondsRemaining;

					if (IsValidLandingSpot(Hit))
					{
						ProcessLanded(Hit, DeltaSecondsRemaining);
						SolvePhysics(DeltaSecondsRemaining);
						return true;
					}

					HandleImpact(Hit, LastMoveTimeSlice, Delta);
					AdjustVelocityFromHitAirborne(Hit, DeltaSecondsApplied);

					// Adjust for when moving between two walls
					FVector PreTwoWallDelta = Delta;
					TwoWallAdjust(Delta, Hit, OldHitNormal);
					SafeMoveUpdatedComponent(Delta, PawnRotation, true, Hit);

					// When "bDitch" is true, the pawn is straddling two slopes of which neither are walkable
					// TODO: Understand this some more, does this affect walkable slopes? I don't think so?
					bool bDitch = OldHitNormal.Z > 0.0f
							&&	Hit.ImpactNormal.Z > 0.0f
							&& FMath::Abs(Delta.Z) <= KINDA_SMALL_NUMBER
							&& (Hit.ImpactNormal | OldHitImpactNormal) < 0.0f;

					if (Hit.Time == 0.0f)
					{
						// If we are stuck try to side step
						FVector SideDelta = (OldHitNormal + Hit.ImpactNormal).GetSafeNormal2D();
						if (SideDelta.IsNearlyZero())
						{
							SideDelta = FVector(OldHitNormal.Y, -OldHitNormal.X, 0.0f).GetSafeNormal();
						}
						SafeMoveUpdatedComponent(SideDelta, PawnRotation, true, Hit);
					}

					if (bDitch || IsValidLandingSpot(Hit) || Hit.Time == 0.0f)
					{
						ProcessLanded(Hit, DeltaSecondsRemaining);
						SolvePhysics(DeltaSecondsRemaining);
						return true;
					}
					
				}
			}
		}
	}
	return false;
}

#pragma endregion PHYSICS_METHODS

#pragma region PHYSICS_INTERACTIONS

#pragma region MOVING_BASE

void UGenCustomMovementComponent::MoveWithBase(const FVector& BaseVelocity, FFloorParams& Floor, float DeltaSeconds)
{
	if (BaseVelocity.IsNearlyZero()) return;
	const FVector LocationDelta = BaseVelocity * DeltaSeconds;
	// Do not sweep here because the base may have moved inside the pawn.
	MoveUpdatedComponent(LocationDelta, UpdatedComponent->GetComponentQuat(), false);
	UpdateFloor(Floor, FloorTraceLength);
	MaintainDistanceToFloor(Floor);
}


FVector UGenCustomMovementComponent::ComputeBaseVelocity(UPrimitiveComponent* MovementBase)
{
	if (!MovementBase) return FVector{0};

	FVector BaseVelocity{0};
	if (IsMovable(MovementBase))
	{
		if (const auto PawnMovementBase = Cast<APawn>(MovementBase->GetOwner()))
		{
			// The current movement base is another pawn.
			BaseVelocity = GetPawnBaseVelocity(PawnMovementBase);
		}
		else
		{
			// The current movement base is some form of geometry.
			BaseVelocity = GetLinearVelocity(MovementBase);
			const FVector BaseTangentialVelocity = ComputeTangentialVelocity(GetLowerBound(), MovementBase);
			BaseVelocity += BaseTangentialVelocity;
		}
	}

	return BaseVelocity;
}


FVector UGenCustomMovementComponent::GetPawnBaseVelocity(APawn* PawnMovementBase) const
{
	if (!PawnMovementBase) return FVector{0};

	const auto GenPawnMovementBase = Cast<AGenPawn>(PawnMovementBase);
	const auto GenMovementComponentOther = Cast<UGenMovementComponent>(PawnMovementBase->GetMovementComponent());

	if (GenPawnMovementBase && GenMovementComponentOther)
	{
		// Since angular velocity is not synchronised we only consider the linear velocity of the base pawn.
		return GenMovementComponentOther->GetVelocity();
	}

	// If the above case does not apply just use whatever the implementation provides. For standalone games we could also calculate the base
	// velocity as per usual (combined linear and angular velocity) but it is probably better not to have different behaviour for different
	// net modes.
	return PawnMovementBase->GetVelocity();
}


bool UGenCustomMovementComponent::ShouldImpartVelocityFromBase(UPrimitiveComponent* MovementBase) const
{
	if (!IsMovable(MovementBase))
	{
		return false;
	}
	if (!bMoveWithBase)
	{
		return false;
	}
	if (MovementBase->IsSimulatingPhysics())
	{
		// Do not impart the velocity of the movement base if it is simulating physics (can cause the pawn to receive a very large/unrealistic
		// impulse, especially from small objects).
		return false;
	}
	return true;
}

#pragma endregion MOVING_BASE

void UGenCustomMovementComponent::RootCollisionTouched(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComponent,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult
)
{
	if (!OtherComponent || !OtherComponent->IsAnySimulatingPhysics())
	{
		return;
	}

	FName BoneName = NAME_None;
	if (OtherBodyIndex != INDEX_NONE)
	{
		const auto OtherSkinnedMeshComponent = Cast<USkinnedMeshComponent>(OtherComponent);
		check(OtherSkinnedMeshComponent)
		BoneName = OtherSkinnedMeshComponent->GetBoneName(OtherBodyIndex);
	}

	float TouchForceFactorModified = TouchForceScale;
	if (bScaleTouchForceToMass)
	{
		const auto BodyInstance = OtherComponent->GetBodyInstance(BoneName);
		TouchForceFactorModified *= BodyInstance ? BodyInstance->GetBodyMass() : 1.f;
	}

	float ImpulseStrength = FMath::Clamp(
	  GetVelocity().Size2D() * TouchForceFactorModified,
	  MinTouchForce > 0.f ? MinTouchForce : -FLT_MAX,
	  MaxTouchForce > 0.f ? MaxTouchForce : FLT_MAX
	);

	const FVector OtherComponentLocation = OtherComponent->GetComponentLocation();
	const FVector ShapeLocation = UpdatedComponent->GetComponentLocation();
	FVector ImpulseDirection =
	  FVector(OtherComponentLocation.X - ShapeLocation.X, OtherComponentLocation.Y - ShapeLocation.Y, 0.25f).GetSafeNormal();
	ImpulseDirection = (ImpulseDirection + GetVelocity().GetSafeNormal2D()) * 0.5f;
	ImpulseDirection.Normalize();

	OtherComponent->AddImpulse(ImpulseDirection * ImpulseStrength, BoneName);
}

void UGenCustomMovementComponent::ApplyImpactPhysicsForces(const FHitResult& Impact, const FVector& ImpactAcceleration, const FVector& ImpactVelocity)
{
	if (!Impact.IsValidBlockingHit())
	{
		return;
	}

	const auto ImpactComponent = Impact.GetComponent();
	if (!ImpactComponent)
	{
		return;
	}

	const auto BodyInstance = ImpactComponent->GetBodyInstance(Impact.BoneName);
	if (!BodyInstance || !BodyInstance->IsInstanceSimulatingPhysics())
	{
		return;
	}

	FVector ForceLocation = Impact.ImpactPoint;
	if (bUsePushForceZOffset)
	{
		FVector Center, Extents;
		BodyInstance->GetBodyBounds().GetCenterAndExtents(Center, Extents);
		if (!Extents.IsNearlyZero())
		{
			ForceLocation.Z = Center.Z + Extents.Z * PushForceZOffsetFactor;
		}
	}

	float PushForceModifier = 1.f;
	const FVector ComponentVelocity = ImpactComponent->GetPhysicsLinearVelocity();
	const FVector VirtualVelocity = ImpactAcceleration.IsZero() ? ImpactVelocity : ImpactAcceleration.GetSafeNormal() * GetMaxSpeed();

	if (bScalePushForceToVelocity && !ComponentVelocity.IsNearlyZero())
	{
		const float DotProduct = ComponentVelocity | VirtualVelocity;
		if (DotProduct > 0.f && DotProduct < 1.f)
		{
			PushForceModifier *= DotProduct;
		}
	}

	const float BodyMass = FMath::Max(BodyInstance->GetBodyMass(), 1.f);
	if (bScalePushForceToMass)
	{
		PushForceModifier *= BodyMass;
	}

	FVector ForceToApply = Impact.ImpactNormal * -1.0f;
	ForceToApply *= PushForceModifier;
	if (ComponentVelocity.IsNearlyZero(1.f))
	{
		ForceToApply *= InitialPushForceScale;
		ImpactComponent->AddImpulseAtLocation(ForceToApply, ForceLocation, Impact.BoneName);
	}
	else
	{
		ForceToApply *= PushForceScale;
		ImpactComponent->AddForceAtLocation(ForceToApply, ForceLocation, Impact.BoneName);
	}
}

// TODO: This needs access to the actors gravity! Set it to a default for now
void UGenCustomMovementComponent::ApplyDownwardForce(float DeltaSeconds)
{
	if (!CurrentFloor.HasValidShapeData() || DownwardForceScale == 0.f)
	{
		return;
	}
	
	if (const auto MovementBase = CurrentFloor.ShapeHit().GetComponent())
	{
		const FVector Gravity = FVector{0.f, 0.f, -9.8f};
		if (!Gravity.IsZero() && MovementBase->IsAnySimulatingPhysics())
		{
			MovementBase->AddForceAtLocation(
			  Gravity * Mass * DownwardForceScale,
			  CurrentFloor.ShapeHit().ImpactPoint,
			  CurrentFloor.ShapeHit().BoneName
			);
		}
	}
}

void UGenCustomMovementComponent::ApplyRepulsionForce(float DeltaSeconds)
{
	if (!PawnOwner || !UpdatedPrimitive || RepulsionForce <= 0.f)
	{
		return;
	}

	const TArray<FOverlapInfo>& Overlaps = UpdatedPrimitive->GetOverlapInfos();
	if (Overlaps.Num() == 0)
	{
		return;
	}

	for (int32 Index = 0; Index < Overlaps.Num(); ++Index)
	{
		const FOverlapInfo& Overlap = Overlaps[Index];

		const auto OverlapComponent = Overlap.OverlapInfo.GetComponent();
		if (!OverlapComponent || OverlapComponent->Mobility < EComponentMobility::Movable)
		{
			continue;
		}

		// Use the body instead of the component for cases where multi-body overlaps are enabled.
		FBodyInstance* OverlapBody = nullptr;
		const int32 OverlapBodyIndex = Overlap.GetBodyIndex();
		const auto SkeletalMeshForBody = OverlapBodyIndex != INDEX_NONE ? Cast<USkeletalMeshComponent>(OverlapComponent) : nullptr;
		if (SkeletalMeshForBody)
		{
			OverlapBody = SkeletalMeshForBody->Bodies.IsValidIndex(OverlapBodyIndex) ? SkeletalMeshForBody->Bodies[OverlapBodyIndex] : nullptr;
		}
		else
		{
			OverlapBody = OverlapComponent->GetBodyInstance();
		}

		if (!OverlapBody)
		{
			continue;
		}

		if (!OverlapBody->IsInstanceSimulatingPhysics())
		{
			continue;
		}

		// Trace to get the hit location on the collision.
		FHitResult Hit;
		FCollisionQueryParams CollisionQueryParams(SCENE_QUERY_STAT(ApplyRepulsionForce));
		CollisionQueryParams.bReturnFaceIndex = false;
		CollisionQueryParams.bReturnPhysicalMaterial = false;
		const FVector ShapeLocation = UpdatedPrimitive->GetComponentLocation();
		const FVector BodyLocation = OverlapBody->GetUnrealWorldTransform().GetLocation();
		bool bHasHit = UpdatedPrimitive->LineTraceComponent(
					Hit,
					BodyLocation,
					{ShapeLocation.X, ShapeLocation.Y, BodyLocation.Z},
					CollisionQueryParams
					);

		constexpr float StopBodyDistance = 2.5f;
		bool bIsPenetrating = Hit.bStartPenetrating || Hit.PenetrationDepth > StopBodyDistance;
		FVector HitLocation = Hit.ImpactPoint;

		// If we did not hit the collision, we are inside the shape.
		if (!bHasHit)
		{
			HitLocation = BodyLocation;
			bIsPenetrating = true;
		}

		const FVector BodyVelocity = OverlapBody->GetUnrealWorldVelocity();
		const float DistanceNow = (HitLocation - BodyLocation).SizeSquared2D();
		const float DistanceLater = (HitLocation - (BodyLocation + BodyVelocity * DeltaSeconds)).SizeSquared2D();

		if (bHasHit && DistanceNow < StopBodyDistance && !bIsPenetrating)
		{
			OverlapBody->SetLinearVelocity(FVector::ZeroVector, false);
		}
		else if (DistanceLater <= DistanceNow || bIsPenetrating)
		{
			FVector ForceCenter = ShapeLocation;

			if (bHasHit)
			{
				ForceCenter.Z = HitLocation.Z;
			}
			else
			{
				float ShapeHalfHeight = GetRootCollisionHalfHeight();
				ForceCenter.Z = FMath::Clamp(BodyLocation.Z, ShapeLocation.Z - ShapeHalfHeight, ShapeLocation.Z + ShapeHalfHeight);
			}

			constexpr float ToleranceFactor = 1.2f;
			const float RepulsionForceRadius = ComputeDistanceToRootCollisionBoundaryXY(BodyLocation - ShapeLocation) * ToleranceFactor;
			OverlapBody->AddRadialForceToBody(ForceCenter, RepulsionForceRadius, RepulsionForce * Mass, ERadialImpulseFalloff::RIF_Constant);
		}
	}
}

#pragma endregion PHYSICS_INTERACTIONS

#pragma region COLLISION_QUERIES

// TODO: Check the effect on Z velocity here
void UGenCustomMovementComponent::AdjustVelocityFromHitAirborne_Implementation(const FHitResult& Hit, float DeltaSeconds)
{
	if (IsAirborne())
	{
		const float SavedVelocityZ = GetVelocity().Z;
		AdjustVelocityFromHit(Hit, DeltaSeconds);
		UpdateVelocity({GetVelocity().X, GetVelocity().Y, FMath::Clamp(GetVelocity().Z, -BIG_NUMBER, SavedVelocityZ)}, DeltaSeconds);
		return;
	}

	AdjustVelocityFromHit(Hit, DeltaSeconds);
}

// TODO: This does some Z angle checks too!
float UGenCustomMovementComponent::SlideAlongSurface(const FVector& Delta, float Time, const FVector& Normal, FHitResult& Hit, bool bHandleImpact)
{
	if (!Hit.bBlockingHit)
	{
		return 0.f;
	}
	FVector NewNormal = Normal;
	if (IsMovingOnGround())
	{
		if (Normal.Z > 0.f)
		{
			// Do not push the pawn up an unwalkable surface.
			if (!HitWalkableFloor(Hit))
			{
				NewNormal = NewNormal.GetSafeNormal2D();
			}
		}
		else if (Normal.Z < -KINDA_SMALL_NUMBER)
		{
			if (CurrentFloor.HasValidShapeData() && CurrentFloor.GetShapeDistanceToFloor() < MIN_DISTANCE_TO_FLOOR)
			{
				// Do not push the pawn down further into the floor when the impact point is on the top part of the collision shape.
				const FVector FloorNormal = CurrentFloor.ShapeHit().Normal;
				const bool bFloorOpposedToMovement = (Delta | FloorNormal) < 0.f && (FloorNormal.Z < 1.f - DELTA);
				if (bFloorOpposedToMovement)
				{
					NewNormal = FloorNormal;
				}
				NewNormal = NewNormal.GetSafeNormal2D();
			}
		}
	}
	return Super::SlideAlongSurface(Delta, Time, NewNormal, Hit, bHandleImpact);

}

// TODO: This checks for ground angle too
void UGenCustomMovementComponent::TwoWallAdjust(FVector& Delta, const FHitResult& Hit, const FVector& OldHitNormal) const
{
	const FVector InDelta = Delta;
	Super::TwoWallAdjust(Delta, Hit, OldHitNormal);
	
	if (IsMovingOnGround())
	{
		float GroundAngle = FMath::Acos(Hit.Normal | UpdatedComponent->GetUpVector().GetSafeNormal());
		// Only allow the pawn to slide up walkable surfaces. Unwalkable surfaces are treated as vertical walls.
		if (Delta.Z > 0.f)
		{
			if ((GroundAngle <= WalkableFloorAngle || HitWalkableFloor(Hit)))
			{
				// Maintain horizontal movement.
				const float Time = (1.f - Hit.Time);
				const FVector ScaledDelta = Delta.GetSafeNormal() * InDelta.Size();
				Delta = FVector(InDelta.X, InDelta.Y, ScaledDelta.Z / Hit.Normal.Z) * Time;
				// The delta Z should never exceed the max allowed step up height so we rescale if necessary. This should be rare (the hit normal Z
				// divisor would have to be very small) but we'd rather lose horizontal movement than go too high.
				if (Delta.Z > MaxStepUpHeight)
				{
					const float Rescale = MaxStepUpHeight / Delta.Z;
					Delta *= Rescale;
				}
			}
			else
			{
				Delta.Z = 0.f;
			}
		}
		else if (Delta.Z < 0.f)
		{
			if (CurrentFloor.HasValidShapeData() && CurrentFloor.GetShapeDistanceToFloor() < MIN_DISTANCE_TO_FLOOR)
			{
				// Do not push the pawn further down into the floor.
				Delta.Z = 0.f;
			}
		}
	}
}


FVector UGenCustomMovementComponent::ComputeSlideVector(const FVector& Delta, float Time, const FVector& Normal, const FHitResult& Hit) const
{
	FVector Result = Super::ComputeSlideVector(Delta, Time, Normal, Hit);
	if (IsAirborne())
	{
		// Prevent boosting up slopes.
		Result = HandleSlopeBoosting(Result, Delta, Time, Normal, Hit);
	}
	return Result;
}


FVector UGenCustomMovementComponent::HandleSlopeBoosting(
	  const FVector& SlideResult,
	  const FVector& Delta,
	  float Time,
	  const FVector& Normal,
	  const FHitResult& Hit
	) const
{
	FVector Result = SlideResult;
	if (Result.Z > 0.f)
	{
		// Don't move any higher than we originally intended.
		const float ZLimit = Delta.Z * Time;
		if (Result.Z - ZLimit > KINDA_SMALL_NUMBER)
		{
			if (ZLimit > 0.f)
			{
				// Rescale the entire vector (not just the Z-component), otherwise we would change the direction and most likely head right back
				// into the impact.
				const float UpPercent = ZLimit / Result.Z;
				Result *= UpPercent;
			}
			else
			{
				// We were heading downwards but would have deflected upwards, make the deflection horizontal instead.
				Result = FVector::ZeroVector;
			}
			// Adjust the remaining portion of the original slide result to be horizontal and parallel to impact normal.
			const FVector RemainderXY = (SlideResult - Result) * FVector(1.f, 1.f, 0.f);
			const FVector NormalXY = Normal.GetSafeNormal2D();
			const FVector Adjust = Super::ComputeSlideVector(RemainderXY, 1.f, NormalXY, Hit);
			Result += Adjust;
		}
	}
	return Result;
}

// TODO: Study this later BUG THAT CAUSES YOU TO LAUNCH HALFWAY ACROSS THE WORLD IS DUE TO STEPS
bool UGenCustomMovementComponent::StepUp(const FVector& LocationDelta, const FHitResult& BarrierHit, FFloorParams& Floor, FHitResult* OutForwardHit)
{
	//return false;//
	const FVector StepLocationDelta = FVector(LocationDelta.X, LocationDelta.Y, 0.f);
	if (MaxStepUpHeight <= 0.f || StepLocationDelta.IsNearlyZero())
	{
		return false;
	}

	const FVector StartLocation = UpdatedComponent->GetComponentLocation();
	float PawnHalfHeight{};
	float UpperBoundZ{};
	FVector Extent = GetRootCollisionExtent();
	switch (GetRootCollisionShape())
	{
	case EGenCollisionShape::VerticalCapsule:
		PawnHalfHeight = Extent.Z;
		UpperBoundZ = StartLocation.Z + (PawnHalfHeight - Extent.X);
		break;
	case EGenCollisionShape::Box:
		PawnHalfHeight = Extent.Z;
		UpperBoundZ = StartLocation.Z + PawnHalfHeight;
		break;
	case EGenCollisionShape::HorizontalCapsule:
	case EGenCollisionShape::Sphere:
	  PawnHalfHeight = Extent.X;
		UpperBoundZ = StartLocation.Z;
		break;
	default: checkNoEntry();
	}
	const float BarrierImpactPointZ = BarrierHit.ImpactPoint.Z;
	if (BarrierImpactPointZ > UpperBoundZ)
	{
		// The top of the collision is hitting something so we cannot travel upward at all.
		return false;
	}

	float TravelUpHeight = MaxStepUpHeight;
	float TravelDownHeight = MaxStepUpHeight;
	float StartFloorPointZ = StartLocation.Z - PawnHalfHeight;
	float StartDistanceToFloor = 0.f;
	if (Floor.HasValidShapeData() && HitWalkableFloor(Floor.ShapeHit()))
	{
		StartDistanceToFloor = Floor.GetShapeDistanceToFloor();
		TravelDownHeight = MaxStepUpHeight + 2.f * MAX_DISTANCE_TO_FLOOR;
	}
	else if (HitWalkableFloor(Floor.LineHit()) && StartDistanceToFloor <= MAX_DISTANCE_TO_FLOOR)
	{
		StartDistanceToFloor = Floor.GetLineDistanceToFloor();
		TravelDownHeight = MaxStepUpHeight + 2.f * MAX_DISTANCE_TO_FLOOR;
	}
	// We float a variable distance above the floor so we want to calculate the max step-up height based on that distance if possible.
	TravelUpHeight = FMath::Max(MaxStepUpHeight - StartDistanceToFloor, 0.f);
	StartFloorPointZ -= StartDistanceToFloor;

	if (BarrierImpactPointZ <= StartFloorPointZ)
	{
		// The impact point is under the lower bound of the collision.
		return false;
	}

	FScopedMovementUpdate ScopedMovement(UpdatedComponent, EScopedUpdate::DeferredUpdates);

	// Step up.
	// @attention Barriers are always treated as vertical walls. The procedure is sweeping upward by "TravelUpHeight" (along positive Z),
	// sweeping forward by "StepLocationDelta" (horizontally) and sweeping downward by "TravelDownHeight" (along negative Z).
	FHitResult UpHit;
	const FQuat PawnRotation = UpdatedComponent->GetComponentQuat();
	GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Magenta, TEXT("Step Up: " + FString::SanitizeFloat(TravelUpHeight)));
	MoveUpdatedComponent(FVector(0.f, 0.f, TravelUpHeight), PawnRotation, true, &UpHit);

	if (UpHit.bStartPenetrating)
	{
		// Invalid move, abort the step-up.
		ScopedMovement.RevertMove();
		return false;
	}

	// Step forward.
	FVector ClampedStepLocationDelta = StepLocationDelta;
	if (StepLocationDelta.Size() < MIN_STEP_UP_DELTA)
	{
		// We need to enforce a minimal location delta for the forward sweep, otherwise we could fail a step-up that should actually be feasible
		// when hitting a non-walkable surface (because the forward step would have been very short).
		ClampedStepLocationDelta = StepLocationDelta.GetClampedToSize(MIN_STEP_UP_DELTA, BIG_NUMBER);
	}
	FHitResult ForwardHit;
	MoveUpdatedComponent(ClampedStepLocationDelta, PawnRotation, true, &ForwardHit);
	if (OutForwardHit) *OutForwardHit = ForwardHit;
	if (ForwardHit.bBlockingHit)
	{
		if (ForwardHit.bStartPenetrating)
		{
			// Invalid move, abort the step-up.
			ScopedMovement.RevertMove();
			return false;
		}

		// If we hit something above us and also something ahead of us we should notify about the upward hit as well. If we hit something above
		// us but not in front of us we don't notify about any hit since we are not blocked from moving (in which case we don't enter this
		// branch in the first place).
		if (UpHit.bBlockingHit)
		{
			// Notify upward hit.
			HandleImpact(UpHit);
		}
		// Notify forward hit.
		HandleImpact(ForwardHit);

		// Slide along the hit surface, but do so based on the original step location delta.
		const float ForwardHitTime = ForwardHit.Time;
		const float SlideTime = SlideAlongSurface(StepLocationDelta, 1.f - ForwardHit.Time, ForwardHit.Normal, ForwardHit, true);
		if (ForwardHitTime == 0.f && SlideTime == 0.f)
		{
			// No movement occurred, abort the step-up.
			ScopedMovement.RevertMove();
			return false;
		}
	}


	// Step down.
	FHitResult DownHit;
	MoveUpdatedComponent(FVector(0.f, 0.f, -TravelDownHeight), UpdatedComponent->GetComponentQuat(), true, &DownHit);
	if (DownHit.bStartPenetrating)
	{
		// Invalid move, abort the step-up.
		ScopedMovement.RevertMove();
		return false;
	}
	if (DownHit.bBlockingHit)
	{
		// We do not notify about hits from the downward sweep since its only purpose is to find a valid end location for the step up, it does
		// not go in the direction of actual movement.
		const float DeltaZ = DownHit.ImpactPoint.Z - StartFloorPointZ;
		if (DeltaZ > MaxStepUpHeight)
		{
			// This step sequence would have allowed us to travel higher than the max step-up height.

			ScopedMovement.RevertMove();
			return false;
		}
		// Reject unwalkable surface normals.
		if (!HitWalkableFloor(DownHit))
		{
			if ((StepLocationDelta | DownHit.ImpactNormal) < 0.f)
			{
				// The normal points towards the pawn i.e. opposes the current movement direction.
				ScopedMovement.RevertMove();
				return false;
			}
			if (DownHit.Location.Z > StartLocation.Z)
			{
				// Do not step up to an unwalkable normal above us. It is fine to step down onto an unwalkable normal below us though (we will slide
				// off) since rejecting those moves would prevent us from being able to walk off edges.
				ScopedMovement.RevertMove();
				return false;
			}
		}

		// Reject moves where the downward sweep hit something very close to the edge of the collision. Does not apply to box shapes.
		if (
		  GetRootCollisionShape() != EGenCollisionShape::Box
		  && !IsWithinEdgeTolerance(DownHit.Location, DownHit.ImpactPoint, EDGE_TOLERANCE)
		)
		{
			ScopedMovement.RevertMove();
			return false;
		}
		// Don't step up onto surfaces that are not a valid base.
		if (DeltaZ > 0.f && !CanStepUp(DownHit))
		{
			ScopedMovement.RevertMove();
			return false;
		}
	}

	return true;
}


// TODO: What do we want to do here? This gives us our position up the slope  so do we want to just return a bool here and project the velocity?
// Will leave it as is for now until everythings setup so i can do some testing
FVector UGenCustomMovementComponent::ComputeRampVector(const FVector& LocationDelta, const FHitResult& RampHit) const
{
	//checkfGMC(LocationDelta.Z == 0.f, TEXT("The movement delta should be horizontal."))
	const FVector FloorNormal = RampHit.ImpactNormal;
	const FVector ContactNormal = RampHit.Normal;
	if (
	  HitWalkableFloor(RampHit)
	  && FloorNormal.Z > KINDA_SMALL_NUMBER
	  && FloorNormal.Z < (1.f - KINDA_SMALL_NUMBER)
	  && ContactNormal.Z > KINDA_SMALL_NUMBER
	)
	{
		// Compute a vector that moves parallel to the surface by projecting the horizontal movement onto the ramp.
		const FVector RampVector = FVector(LocationDelta.X, LocationDelta.Y, LocationDelta.Z);
		return RampVector;
	}
	return LocationDelta;
}
	

bool UGenCustomMovementComponent::IsValidLandingSpot(const FHitResult& Hit)
{
	if (!Hit.bBlockingHit) return false;

	if (Hit.bStartPenetrating)
	{
		ResolvePenetration(GetPenetrationAdjustment(Hit), Hit, UpdatedComponent->GetComponentQuat());
		return false;
	}

	if (!HitWalkableFloor(Hit))	return false;

	FHitResult StepDownHeightTest;
	const FVector LineTraceStart = Hit.Location;
	const FVector LineTraceEnd = LineTraceStart + FVector::DownVector * (MaxStepDownHeight + MAX_DISTANCE_TO_FLOOR);
	FCollisionQueryParams CollisionQueryParams(FName(__func__), false, GetOwner());

	if (const auto World = GetWorld())
	{
		World->LineTraceSingleByChannel
			(
			StepDownHeightTest,
			LineTraceStart,
			LineTraceEnd,
			UpdatedComponent->GetCollisionObjectType(),
			CollisionQueryParams
			);
	}

	const bool bCanLandOnLedge =
		StepDownHeightTest.IsValidBlockingHit()
		|| !IsExceedingFallOffThreshold(Hit.ImpactPoint, Hit.Location - FVector(0.0f, 0.0f, GetRootCollisionHalfHeight()), Hit.Location);
	
	if (!bCanLandOnLedge) return false;

	float HitLocationLowerBoundZ{};
	const float HitLocationZ = Hit.Location.Z;
	const FVector Extent = GetRootCollisionExtent();
	// TODO: Do some vector math to support arbitrary rotations here? Just approximations maybe
	switch (GetRootCollisionShape())
	{
	case EGenCollisionShape::VerticalCapsule:
		HitLocationLowerBoundZ = HitLocationZ - (Extent.Z /*HalfHeight*/ - Extent.X /*Radius*/);
		break;
	case EGenCollisionShape::Box:
		HitLocationLowerBoundZ = HitLocationZ - Extent.Z;
		break;
	case EGenCollisionShape::HorizontalCapsule:
	case EGenCollisionShape::Sphere:
	  HitLocationLowerBoundZ = HitLocationZ;
		break;
	default: checkNoEntry();
	}

	if (Hit.ImpactPoint.Z >= HitLocationLowerBoundZ)
	{
		// TODO: Would this need changing?
		// The impact point is on the vertical side of the collision shape or above it
		return false;
	}
	if (!IsWithinEdgeTolerance(Hit.Location, Hit.ImpactPoint, EDGE_TOLERANCE))
	{
		// We reject hits that are barely on the cusp of the outer side of the collision
		return false;
	}

	// The pawn hit a valid landing spot
	return true;
}

void UGenCustomMovementComponent::ProcessLanded(const FHitResult& Hit, float DeltaSeconds, bool bUpdateFloor)
{
	LedgeFallOffDirection = FVector::ZeroVector;

	if (bEnablePhysicsInteraction)
	{
		ApplyImpactPhysicsForces(Hit, GetTransientAcceleration(), GetVelocity());
	}

	AdjustVelocityFromHit(Hit, DeltaSeconds);

	if (bUpdateFloor)
	{
		UpdateFloor(CurrentFloor, FloorTraceLength);
		MaintainDistanceToFloor(CurrentFloor);
	}

	SetGroundingStatus(EGroundingStatus::Grounded);
}

	
// TODO: This checks for valid slopes!
bool UGenCustomMovementComponent::HitWalkableFloor(const FHitResult& Hit) const
{
	if (!Hit.IsValidBlockingHit())	return false;
	if (!CanStepUp(Hit))			return false;

	// TODO: There's a built in walkableslopeoverride thats called here from EngineTypes.h, I don't get it
	float NormalAngle = FMath::RadiansToDegrees(FMath::Acos(Hit.Normal | UpdatedComponent->GetUpVector()));
	float ImpactNormalAngle = FMath::RadiansToDegrees(FMath::Acos(Hit.ImpactNormal | UpdatedComponent->GetUpVector()));

	if (ImpactNormalAngle > 50.f)
	{
		DebugMessage(FColor::Blue, TEXT("Angle: " + FString::SanitizeFloat(ImpactNormalAngle)));
	}
	
	if (ImpactNormalAngle > WalkableFloorAngle)
	{
		if (bLandOnEdges && IsAirborne())
		{
			// TODO: Might want to check this with world up
			// For rounded collision shapes more impacts that are on the hard edge will be recognized as walkable with this check
			if (NormalAngle > WalkableFloorAngle)
			{
				return false;
			}
		}
		else
		{
			return false;
		}
	}
	return true;
}

// TODO: This might need some adjustments as this handles the "stickiness" of the floor
bool UGenCustomMovementComponent::MaintainDistanceToFloor(UPARAM(ref) FFloorParams& Floor)
{
	if (!Floor.HasValidShapeData() || !HitWalkableFloor(Floor.ShapeHit()))
	{
		// We need a valid shape trace that hit a walkable surface to adjust.
		return false;
	}

	float CurrentDistanceToFloor = Floor.GetShapeDistanceToFloor();
	if (CurrentDistanceToFloor >= MIN_DISTANCE_TO_FLOOR && CurrentDistanceToFloor <= MAX_DISTANCE_TO_FLOOR)
	{
		// The pawn is already within the target area.
		return true;
	}

	const FFloorParams StartFloor = Floor;
	const FVector StartLocation = UpdatedComponent->GetComponentLocation();
	const float AvgDistanceToFloor = 0.5f * (MIN_DISTANCE_TO_FLOOR + MAX_DISTANCE_TO_FLOOR);
	const float DeltaZ = AvgDistanceToFloor - CurrentDistanceToFloor;

	// Here we check for undesirable behaviour that can happen with rounded collision shapes. The pawn could unintentionally get pushed up a
	// barrier because the underside of the collision "peeks" over a ledge (i.e. the impact point of the shape hit is higher than the impact
	// point of the line hit).
	if (
		DeltaZ > 0.f
		&& Floor.HasValidLineData()
		&& Floor.ShapeHit().ImpactPoint.Z > Floor.LineHit().ImpactPoint.Z
		&& Floor.GetLineDistanceToFloor() >= MIN_DISTANCE_TO_FLOOR
		&& Floor.GetLineDistanceToFloor() <= MAX_DISTANCE_TO_FLOOR
		)
	{
		return false;
	}

	// Try to maintain the average distance from the floor.
	FHitResult HitResult;
	{
	FScopedMovementUpdate ScopedMovement(UpdatedComponent, EScopedUpdate::DeferredUpdates);
	const FVector AdjustDelta = FVector(0.f, 0.f, DeltaZ);
	SafeMoveUpdatedComponent(AdjustDelta, UpdatedComponent->GetComponentQuat(), true, HitResult);
	UpdateFloor(Floor, FloorTraceLength);

	if (!Floor.HasValidShapeData() || !HitWalkableFloor(Floor.ShapeHit()))
	{
		// The update has invalidated the floor, undo.
		Floor = StartFloor;
		ScopedMovement.RevertMove();
		return false;
	}
	}

	CurrentDistanceToFloor = Floor.GetShapeDistanceToFloor();
	if (CurrentDistanceToFloor >= MIN_DISTANCE_TO_FLOOR && CurrentDistanceToFloor <= MAX_DISTANCE_TO_FLOOR)
	{
		return true;
	}

	return false;
}

// TODO: Study this
bool UGenCustomMovementComponent::IsExceedingFallOffThreshold(const FVector& ImpactPoint, const FVector& PawnLowerBound, const FVector& PawnCenter) const
{
	checkGMC(LedgeFallOffThreshold >= 0.f)
	checkGMC(LedgeFallOffThreshold <= 1.f)

	// If the pawn cannot walk off ledges we always use the center of the collision as fall-off threshold, otherwise the pawn could partially
	// land on a ledge (from an airborne state) and still walk off afterwards.
	const float TestThreshold = bCanWalkOffLedges ? LedgeFallOffThreshold : 0.f;

	if (TestThreshold == 1.f)
	{
		// A threshold of 1 means we do not want to fall off at all.
		return false;
	}

	const FVector CenterToImpactXY = FVector(ImpactPoint.X, ImpactPoint.Y, 0.f) - FVector(PawnCenter.X, PawnCenter.Y, 0.f);
	const FVector ImpactDirectionXY = CenterToImpactXY.GetSafeNormal();
	if (ImpactDirectionXY.IsNearlyZero())
	{
		// The impact point is at the center.
		return false;
	}

	const float DistanceToImpactXY = CenterToImpactXY.Size();
	const float DistanceToBoundaryXY = ComputeDistanceToRootCollisionBoundaryXY(ImpactDirectionXY);
	checkGMC(DistanceToBoundaryXY > 0.f)
	// @attention The relatively large tolerance of one millimeter is imperative here.
	const bool bImpactWithinZConstraints =
	ImpactPoint.Z >= (PawnLowerBound.Z - MAX_DISTANCE_TO_FLOOR - UU_MILLIMETER) && ImpactPoint.Z < (PawnCenter.Z + UU_MILLIMETER);
	const bool bImpactWithinXYConstraints = (DistanceToBoundaryXY + UU_MILLIMETER) > DistanceToImpactXY;
	if (!bImpactWithinZConstraints || !bImpactWithinXYConstraints)
	{
		// The impact point is not on the lower side of the collision shape.
		return false;
	}

	if (GetRootCollisionShape() == EGenCollisionShape::Box)
	{
		// Due to the way Unreal handles box collisions we assume the threshold to be either just 1 (if it is actually >= 0.5) or 0 (if it is
		// actually < 0.5). Any values in between are not reliable because the impact point is not guaranteed to be exactly on the edge of the
		// ledge.
		// @attention Do not check this any earlier than here, the constraints for the impact point must be met.
		return TestThreshold < 0.5f;
	}

	return (DistanceToImpactXY / DistanceToBoundaryXY) > TestThreshold;
}


#pragma endregion COLLISION_QUERIES

#pragma region COLLISION_PENETRATIONS

FHitResult UGenCustomMovementComponent::AutoResolvePenetration()
{
	bStuckInGeometry = false;

	FHitResult Hit = Super::AutoResolvePenetration();

	if (Hit.bStartPenetrating)
	{
		FVector X;
		
		const bool bHitPawn = static_cast<bool>(Cast<APawn>(Hit.GetActor()));
		const FVector AdjustmentDirection = GetPenetrationAdjustment(Hit).GetSafeNormal();
		if (AdjustmentDirection != FVector::ZeroVector)
		{
			const FVector InitialLocation = UpdatedComponent->GetComponentLocation();
			const float MaxDepenetration = bHitPawn ? MaxDepenetrationWithPawn : MaxDepenetrationWithGeometry;
			// Subdivide the max depenetration distance into smaller substeps and incrementally adjust the pawn position.
			// @attention This can lead to an overall greater adjustment than the set max depenetration distance.
			constexpr int32 Resolution = 10;
			const float AdjustmentStepDistance = MaxDepenetration / Resolution;
			for (int NumRetry = 1; NumRetry <= Resolution; ++NumRetry)
			{
				UpdatedComponent->SetWorldLocation(InitialLocation + AdjustmentDirection * NumRetry * AdjustmentStepDistance);
				if (Super::AutoResolvePenetration().bStartPenetrating)
				{
					// If we are still stuck after adjusting undo the location change.
					UpdatedComponent->SetWorldLocation(InitialLocation);
				}
				else
				{
					// The penetration was resolved. We still return the original downward hit.
					return Hit;
				}
			}
		}
		bStuckInGeometry = true;
		OnStuckInGeometry();
	}

	return Hit;
}

FVector UGenCustomMovementComponent::GetPenetrationAdjustment(const FHitResult& Hit) const
{
	FVector Result = Super::GetPenetrationAdjustment(Hit);
	float MaxDistance = MaxDepenetrationWithGeometry;
	if (Cast<APawn>(Hit.GetActor())) MaxDistance = MaxDepenetrationWithPawn;
	Result = Result.GetClampedToMaxSize(MaxDistance);
	return Result;
}

#pragma endregion COLLISION_PENETRATIONS

#pragma region SETTING_INFORMATION

void UGenCustomMovementComponent::SetGroundingStatus(EGroundingStatus NewGroundingStatus)
{
	if (GroundingStatus != NewGroundingStatus)
	{
		EGroundingStatus PreviousGroundingStatus = GroundingStatus;
		GroundingStatus = NewGroundingStatus;
		OnMovementModeUpdated(PreviousGroundingStatus);
	}
}


#pragma endregion SETTING_INFORMATION


#pragma region ANIMATION

void UGenCustomMovementComponent::SetSkeletalMeshReference(USkeletalMeshComponent* Mesh)
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

void UGenCustomMovementComponent::BlockSkeletalMeshPoseTick() const
{
	if (!SkeletalMesh) return;
	SkeletalMesh->bIsAutonomousTickPose = false;
	SkeletalMesh->bOnlyAllowAutonomousTickPose = true;
}

bool UGenCustomMovementComponent::ShouldTickPose(bool* bOutSimulate) const
{
	// @attention The @see bHasAnimRootMotion flag should be false at this time regardless of whether we are currently playing root motion or
	// not. If this function returns true the flag will be set in @see TickPose if required.

	if (bOutSimulate)
	{
		*bOutSimulate = false;
	}

	if (!SkeletalMesh)
	{
		return false;
	}

	// Enforce that these properties are not set.
	SkeletalMesh->bOnlyAllowAutonomousTickPose = false;
	SkeletalMesh->bIsAutonomousTickPose = false;

	const bool bIsPlayingMontage = IsPlayingMontage(SkeletalMesh);
	if (!bIsPlayingMontage)
	{
		// The manual pose tick is only relevant when playing montages.
		return false;
	}
	
	const bool bIsPlayingRootMotionMontage = static_cast<bool>(GetRootMotionMontageInstance(SkeletalMesh));
	if (!bIsPlayingRootMotionMontage)
	{
		// In standalone the manual pose tick is only used for applying root motion from animations, but there's no root motion montage
		// currently playing.
		return false;
	}



	return SkeletalMesh->ShouldTickPose();
}

// TODO: Replication role is NONE for singleplayer!!!!
void UGenCustomMovementComponent::TickPose(float DeltaSeconds, bool bSimulate)
{
	// During the pose tick some of the montage instance data (which we still need afterwards) can get cleared.
	UAnimMontage* RootMotionMontage{nullptr};
	float RootMotionMontagePosition{-1.f};
	if (const auto RootMotionMontageInstance = GetRootMotionMontageInstance(SkeletalMesh))
	{
		RootMotionMontage = RootMotionMontageInstance->Montage;
		RootMotionMontagePosition = RootMotionMontageInstance->GetPosition();
	}

	if (bSimulate)
	{
		if (const auto AnimInstance = SkeletalMesh->GetAnimInstance())
		{
			for (auto MontageInstance : AnimInstance->MontageInstances)
			{
				float MontagePosition = MontageInstance->GetPosition();
				MontageInstance->UpdateWeight(DeltaSeconds);
				MontageInstance->SimulateAdvance(DeltaSeconds, MontagePosition, RootMotionParams/*only modified if the montage has root motion*/);
				MontageInstance->SetPosition(MontagePosition);
				auto& MontageBlend = MontageInstance->*get(FAnimMontageInstance_Blend());
				if (MontageInstance->IsStopped() && MontageBlend.IsComplete())
				{
					// During the pose tick most data will be cleared on the montage instance when it terminates. However, when we use
					// @see FAnimMontageInstance::SimulateAdvance no instance data is touched so we need to reproduce this behaviour manually to get
					// consistent results across all execution modes.
					AnimInstance->ClearMontageInstanceReferences(*MontageInstance);
					MontageBlend.SetCustomCurve(NULL);

					MontageBlend.SetBlendOption(EAlphaBlendOption::Linear);
					MontageInstance->Montage = nullptr;
				}
			}
		}
	}
	else
	{
		const bool bWasPlayingRootMotion = IsPlayingRootMotion(SkeletalMesh);
		SkeletalMesh->TickPose(DeltaSeconds, true);
		const bool bIsPlayingRootMotion = IsPlayingRootMotion(SkeletalMesh);
		if (bWasPlayingRootMotion || bIsPlayingRootMotion)
		{
			RootMotionParams = SkeletalMesh->ConsumeRootMotion();
		}
	}

	checkGMC(!bHasAnimRootMotion)
	if (RootMotionParams.bHasRootMotion)
	{
		if (ShouldDiscardRootMotion(RootMotionMontage, RootMotionMontagePosition))
		{
			RootMotionParams = FRootMotionMovementParams();
			return;
		}
		bHasAnimRootMotion = true;

		// The root motion translation can be scaled by the user.
		RootMotionParams.ScaleRootMotionTranslation(GetAnimRootMotionTranslationScale());
		// Save the root motion transform in world space.
		RootMotionParams.Set(SkeletalMesh->ConvertLocalRootMotionToWorld(RootMotionParams.GetRootMotionTransform()));
		// Calculate the root motion velocity from the world space root motion translation.
		CalculateAnimRootMotionVelocity(DeltaSeconds);
		// Apply the root motion rotation now. Translation is applied with the next update from the calculated velocity. Splitting root motion
		// translation and rotation up like this may not be optimal but the alternative is to replicate the rotation for replay which is far
		// more undesirable.
		ApplyAnimRootMotionRotation(DeltaSeconds);
	}
}

bool UGenCustomMovementComponent::ShouldDiscardRootMotion(UAnimMontage* RootMotionMontage, float RootMotionMontagePosition) const
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

// TODO: This might need changes for artbirary rotation?
void UGenCustomMovementComponent::ApplyAnimRootMotionRotation(float DeltaSeconds)
{
	const FQuat RootMotionQuat = RootMotionParams.GetRootMotionTransform().GetRotation();
	if (RootMotionQuat.IsIdentity(KINDA_SMALL_NUMBER))
	{
		return;
	}

	const FRotator RootMotionRotator = RootMotionQuat.Rotator();
	const FRotator RootMotionRotatorXY = FRotator(RootMotionRotator.Pitch, 0.f, RootMotionRotator.Roll);
	const FRotator RootMotionRotatorZ = FRotator(0.f, RootMotionRotator.Yaw, 0.f);

	if (!RootMotionRotatorZ.IsNearlyZero(0.01f))
	{
		const FRotator NewRotationZ = UpdatedComponent->GetComponentRotation() + RootMotionRotatorZ;
		// Passing a rate of 0 will set the target rotation directly while still adjusting for collisions.
		RotateYawTowardsDirectionSafe(NewRotationZ.Vector(), 0.f, DeltaSeconds);
	}
	if (!RootMotionRotatorXY.IsNearlyZero(0.01f))
	{
		const FRotator NewRotationXY = UpdatedComponent->GetComponentRotation() + RootMotionRotatorXY;
		// We set the roll- and pitch-rotations in a safe way but we won't get any adjustment in case of collisions.
		SetRootCollisionRotationSafe(NewRotationXY);
	}
}

void UGenCustomMovementComponent::CalculateAnimRootMotionVelocity(float DeltaSeconds)
{
	FVector RootMotionDelta = RootMotionParams.GetRootMotionTransform().GetTranslation();

	// Ignore components with very small delta values.
	RootMotionDelta.X = FMath::IsNearlyEqual(RootMotionDelta.X, 0.f, 0.01f) ? 0.f : RootMotionDelta.X;
	RootMotionDelta.Y = FMath::IsNearlyEqual(RootMotionDelta.Y, 0.f, 0.01f) ? 0.f : RootMotionDelta.Y;
	RootMotionDelta.Z = FMath::IsNearlyEqual(RootMotionDelta.Z, 0.f, 0.01f) ? 0.f : RootMotionDelta.Z;

	FVector RootMotionVelocity = RootMotionDelta / DeltaSeconds;
	UpdateVelocity(PostProcessAnimRootMotionVelocity(RootMotionVelocity, DeltaSeconds));
}

// TODO: Might wanna mess around with this some more to allow Z root motion, depending on the case, maybe if it's non-zero
FVector UGenCustomMovementComponent::PostProcessAnimRootMotionVelocity_Implementation(const FVector& RootMotionVelocity, float DeltaSeconds)
{
	FVector FinalVelocity = RootMotionVelocity;

	// TODO: Added an extra check here, didn't think it through so much though.
	if (IsAirborne() && FMath::IsNearlyEqual(FinalVelocity.Z, 0.f, 0.01f))
	{
		// Ignore changes to the Z-velocity when airborne.
		FinalVelocity.Z = GetVelocity().Z;
	}

	return FinalVelocity;
}


#pragma endregion ANIMATION


#pragma region AI_RVO


#pragma endregion AI_RVO

