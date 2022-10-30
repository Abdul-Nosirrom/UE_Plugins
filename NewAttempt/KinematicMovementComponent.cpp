// Fill out your copyright notice in the Description page of Project Settings.


#include "KinematicMovementComponent.h"

#include "Kismet/KismetSystemLibrary.h"


// Sets default values for this component's properties
UKinematicMovementComponent::UKinematicMovementComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	// ...
	//PrePhysicsTick.TickGroup = TG_PrePhysics;
	//PrePhysicsTick.bCanEverTick = true;
	//PrePhysicsTick.bStartWithTickEnabled = true;

	//PostPhysicsTick.TickGroup = TG_PostPhysics;
	//PostPhysicsTick.bCanEverTick = true;
	//PostPhysicsTick.bStartWithTickEnabled = true;
}


// Called when the game starts
void UKinematicMovementComponent::BeginPlay()
{
	Super::BeginPlay();

	// WANT THIS TICK TO CALL EVERYTHING ELSE
	//SetTickGroup(ETickingGroup::TG_DuringPhysics);
	// WANT THIS TICK TO CALL CUSTOM INTERPOLATION
	//SetTickGroup(ETickingGroup::TG_PostPhysics);

	// Initialize it and cache it
	SetUpdatedComponent(UpdatedComponent);
	CharacterUp = UpdatedComponent->GetUpVector();
	CharacterForward = UpdatedComponent->GetForwardVector();
	CharacterRight = UpdatedComponent->GetRightVector();

}

// TODO: Implement These
void UKinematicMovementComponent::InitializeComponent()
{
	Super::InitializeComponent();
}

void UKinematicMovementComponent::SetUpdatedComponent(USceneComponent* NewUpdatedComponent)
{
	Super::SetUpdatedComponent(NewUpdatedComponent);
	Capsule = dynamic_cast<UCapsuleComponent*>(UpdatedComponent);
	CapsuleHeight = Capsule->GetScaledCapsuleHalfHeight() * 2.f;
	CapsuleRadius = Capsule->GetScaledCapsuleRadius();
	CapsuleHalfHeight = Capsule->GetScaledCapsuleHalfHeight();

	if (bEnablePhysicsInteractions)
	{
		//Capsule->OnComponentBeginOverlap.AddUniqueDynamic(this, &UKinematicMovementComponent::RootCollisionTouched);
	}
}



// Called every frame
void UKinematicMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                                FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	
	// Presimulation Update
	PreSimulationInterpolationUpdate(DeltaTime);
	
	// Simulation Update
	Simulate(DeltaTime);

}

#pragma region Simulation Updates

void UKinematicMovementComponent::PreSimulationInterpolationUpdate(float DeltaTime)
{
	InitialTickPosition = TransientPosition;
	InitialTickRotation = TransientRotation;

	const FVector MoveDelta = TransientPosition - UpdatedComponent->GetComponentLocation();

	UpdatedComponent->SetWorldLocationAndRotation(TransientPosition, TransientRotation);
	//return;
	for (int NumRetry = 1; NumRetry <= 10; ++NumRetry)
	{
		if (AutoResolvePenetration())
		{
			break;
		}
	}
	InitialTickPosition = UpdatedComponent->GetComponentLocation();
	InitialTickRotation = UpdatedComponent->GetComponentQuat();
	TransientPosition = InitialTickPosition;
	TransientRotation = InitialTickRotation;
}


void UKinematicMovementComponent::Simulate(float DeltaTime)
{
	UpdatePhase1(DeltaTime);
	UpdatePhase2(DeltaTime);

	MoveUpdatedComponent(TransientPosition - InitialTickPosition, TransientRotation, true);
	//UpdatedComponent->SetWorldLocationAndRotation(TransientPosition, TransientRotation);
}

#pragma endregion Simulation Updates


#pragma region Solver Updates

void UKinematicMovementComponent::UpdatePhase1(float DeltaTime)
{
	/*  NAN Propagation Safety Stop */
	if (FMath::IsNaN(Velocity.X) || FMath::IsNaN(Velocity.Y) || FMath::IsNaN(Velocity.Z))
	{
		Velocity = FVector::ZeroVector;
	}
	// TODO: Similar NaN check for the moving base if one exists. Might not be needed though since it wouldnt operate under this system
	/* EVENT: Before Update */
	BeforeCharacterUpdate(DeltaTime);
	
	/* Get Current Up-To-Date Information About Our Physics State */
	TransientPosition = UpdatedComponent->GetComponentLocation();
	TransientRotation = UpdatedComponent->GetComponentQuat();

	/* Cache our initial physics state before we perform any updates */
	InitialSimulatedPosition = TransientPosition;
	InitialSimulatedRotation = TransientRotation;

	OverlapsCount = 0; 
	bLastSolvedOverlapNormalDirty = false; 

	/* Handle MovePosition Direct Calls */
	if (bMovePositionDirty)
	{
		if (bSolveMovementCollisions)
		{
			// TODO: Implement This!
			FVector TmpVelocity = GetVelocityFromMovement(MovePositionTarget - TransientPosition, DeltaTime);
			if (InternalCharacterMove(TmpVelocity, DeltaTime))
			{
				// TODO: Overlaps will be stored from this move, handle them if physics objects
			}
		}
		else
		{
			TransientPosition = MovePositionTarget;
		}
		bMovePositionDirty = false;
	}

	/* Setup Grounding Status For This Tick */
	LastGroundingStatus.CopyFrom(GroundingStatus);
	//TODO: Might be a better way of initializing this mebe
	GroundingStatus = FGroundingReport();
	GroundingStatus.GroundNormal = CharacterUp;

	/*
	 * What we're essentially doing here is solving for collisions from our last movement tick.
	 * Transient Position/Rotation has not been updated with this ticks velocity, we're just iteratively going through
	 * each possible overlap and moving the transient position/rotation out of any potential penetrations.
	 * This whole block below is something that could be done with MoveUpdatedComponent perhaps.
	 * If not, then we can at least use Deferred Movement Scopes so we don't have to actually set positions and rotations
	 * just to compute depenetration (which we do for NumOverlap times as Transients are updated with each depenetration)
	 */
	if (bSolveMovementCollisions)
	{
		/* Initiate Overlap Solving Data */
		int IterationsMade = 0;
		bool bOverlapSolved = false;

		/* Respect Decollision Iteration Count, Exit when overlap is solved*/
		while (IterationsMade < MaxDecollisionIterations && !bOverlapSolved)
		{
			if (ResolveOverlaps())
			{
				bOverlapSolved = true;
			}
			IterationsMade++;
		}
	}

	/* Handle ground probing and snapping */
	if (bSolveGrounding)
	{
		/* If ForceUnground() has been called, then move us up ever so slightly to bypass snapping and leave ground */
		if (MustUnground())
		{
			TransientPosition += CharacterUp * (MinimumGroundProbingDistance * 1.5f);
		}
		else
		{
			// Choose appropriate ground probing distance
			float SelectedGroundProbingDistance = MinimumGroundProbingDistance;
			if (!LastGroundingStatus.bSnappingPrevented && (LastGroundingStatus.bIsStableOnGround || bLastMovementIterationFoundAnyGround))
			{
				if (StepHandling == EStepHandlingMethod::None)
				{
					SelectedGroundProbingDistance = FMath::Max(CapsuleRadius, MaxStepHeight);
				}
				else
				{
					SelectedGroundProbingDistance = CapsuleRadius;
				}
				SelectedGroundProbingDistance += GroundDetectionExtraDistance;
			}
			ProbeGround(TransientPosition, TransientRotation, SelectedGroundProbingDistance, GroundingStatus);

			if (!LastGroundingStatus.bIsStableOnGround && GroundingStatus.bIsStableOnGround)
			{
				/* Landing! */
				Velocity = FVector::VectorPlaneProject(Velocity, CharacterUp);
				Velocity = GetDirectionTangentToSurface(Velocity, GroundingStatus.GroundNormal) * Velocity.Size();
			}
		}
	}

	bLastMovementIterationFoundAnyGround = false;

	if (MustUngroundTimeCounter > 0.f)
	{
		MustUngroundTimeCounter -= DeltaTime;
	}
	bMustUnground = false;
	// Region end

	if (bSolveGrounding)
	{
		// TODO: Call Generic Event That We Have Probed The Ground (Doesn't Mean We Found or Didn't Find Ground)
	}

	// TODO: HERES WHERE YOUD ADD THE CODE FOR MOVING PLATFORMS
	
}

/*
 * We're gonna handle actual interactions with "physics bodies" or "pawns" through RootCollisionTouched event (See GMC)
 * In KCC it's handled here.
 */
void UKinematicMovementComponent::UpdatePhase2(float DeltaTime)
{
	/* EVENT: Update Rotation */
	UpdateRotation(TransientRotation, DeltaTime);

	/* Handle call to direct setting of rotation */
	if (bMoveRotationDirty)
	{
		TransientRotation = MoveRotationTarget;
		bMoveRotationDirty = false;
	}

	// Handle physics interactions here?
	if (bSolveMovementCollisions && bEnablePhysicsInteractions && false)
	{
		if (bEnablePhysicsInteractions)
		{
			// Solve potential physics body overlaps
			// The base velocity should be calculated before moving along the floor but applied afterwards.
			//const FVector BaseVelocity = bMoveWithBase ? ComputeBaseVelocity(CurrentFloor.ShapeHit().GetComponent()) : FVector{0};
			//if (bMoveWithBase) MoveWithBase(BaseVelocity, CurrentFloor, DeltaSeconds);

			// TODO: The shit here
			/* Solve for potential overlap with moving platform we're on */
			{
				
			}

			/* Solve for overlaps that coudld've been caused by movement of other pawns or actors*/
			/* We have not moved here since we did overlap checks, but other things might have. The depenetration call in
			 * Phase1 is for our movement after we are in Transient transform. The check here is for other objects movement that
			 * we potentially might've overlapped with due to them not us. I think just solving the overlap is enough, and we
			 * can leave the physics interactions part to the RootCollisionTouched delegate.
			 */
			{
				// Initialize Data
				int IterationsMade = 0;
				bool bOverlapSolved = false;
				
				// Loop until we've solved overlaps or consumed all iterations
				while (IterationsMade < MaxDecollisionIterations && !bOverlapSolved)
				{
					if (ResolveOverlaps())
					{
						bOverlapSolved = true;
					}
					IterationsMade++;
				}
			}
		}
	}

	if (GroundingStatus.bIsStableOnGround)
	{
		float VelMag = Velocity.Size();
		Velocity = GetDirectionTangentToSurface(Velocity, GroundingStatus.GroundNormal).GetSafeNormal() * VelMag;
	}

	/* EVENT: Update Velocity */
	UpdateVelocity(Velocity, DeltaTime);

	/*
	 * We might as well just call this Velocity/use the one from our parent. We're never actually using the "Velocity"
	 * from KCC, it's just a getter (NOTE: I DID THIS NOW). Given that Platform Velocity is never really factored into
	 * our pawns velocity, it's just used quickly to get a movement delta INDEPENDENTLY
	 */
	if (Velocity.Size() < MinVelocityMagnitude)
	{
		Velocity = FVector::ZeroVector;
	}

	// Calculate character movement from velocity
	if (Velocity.SizeSquared() > 0.f)
	{
		if (bSolveMovementCollisions)
		{
			InternalCharacterMove(Velocity, DeltaTime);
		}
		else
		{
			TransientPosition += Velocity * DeltaTime;
		}
	}

	// TODO: This might not be needed here as well if we're using the RootCollisionTouched method!
	// We could however store hits in RootCollisionTouched and then process them elsewhere...
	if (bEnablePhysicsInteractions)
	{
		// TODO: Okay This Makes Sense, Imparting Velocity After Its Been Calculated And Us Moved
		// Impart force onto them based on our movement velocity
	}

	// TODO: I THINK I MEANT THAT THERE ARE PLANAR CONSTRAINTS WE INHERIT SO WE CAN USE THOSE INSTEAD, CHECK DOCS
	// This is using from daddy UPawnMovement uWu
	// Handle Planar Constraint
	if (bConstrainToPlane)
	{
		TransientPosition = InitialSimulatedPosition + FVector::VectorPlaneProject(TransientPosition - InitialSimulatedPosition, PlaneConstraintNormal);	
	}

	// Discrete Collision Detection
	if (bDiscreteCollisionEvents)
	{
		int NumOverlaps = CollisionOverlaps(TransientPosition, TransientRotation, InternalProbedColliders, CollisionOffset * 2.f);
		for (int i = 0; i < NumOverlaps; i++)
		{
			// TODO: Call Event
		}
	}
	
	/* EVENT: After Character Update */
	AfterCharacterUpdate(DeltaTime);
}


#pragma endregion Solver Updates

#pragma region Grounding Calls

void UKinematicMovementComponent::ForceUnground(float time)
{
	bMustUnground = true;
	MustUngroundTimeCounter = time;
}


bool UKinematicMovementComponent::MustUnground() const
{
	return bMustUnground || MustUngroundTimeCounter > 0.f;
}


#pragma endregion Grounding Calls 

#pragma region Evaluations

bool UKinematicMovementComponent::IsStableOnNormal(const FVector Normal)
{
	const float NormalAngle = FMath::RadiansToDegrees(FMath::Acos(Normal.GetSafeNormal() | CharacterUp.GetSafeNormal()));
	return NormalAngle <= MaxStableSlopeAngle;
}

// TODO: Comment this in more depth, this is a really important method for launching off slopes and snapping and the likes
bool UKinematicMovementComponent::IsStableWithSpecialCases(const FHitStabilityReport& StabilityReport, const FVector CharVelocity)
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



void UKinematicMovementComponent::EvaluateCrease(FVector CurCharacterVelocity, FVector PrevCharacterVelocity, FVector CurHitNormal, FVector PrevHitNormal, bool bCurrentHitIsStable, bool bPreviousHitIsStable, bool bCharIsStable, bool& bIsValidCrease, FVector& CreaseDirection)
{
	bIsValidCrease = false;
	CreaseDirection = FVector::ZeroVector;

	if (!bCharIsStable || !bCurrentHitIsStable || !bPreviousHitIsStable)
	{
		FVector TmpBlockingCreaseDirection = (CurHitNormal ^ PrevHitNormal).GetSafeNormal();
		float DotPlanes = CurHitNormal | PrevHitNormal;
		bool bIsVelocityConstrainedByCrease = false;

		// Avoid calculations if the two planes are the same
		if (DotPlanes < 0.999f)
		{
			const FVector NormalOnCreasePlaneA = FVector::VectorPlaneProject(CurHitNormal, TmpBlockingCreaseDirection).GetSafeNormal();
			const FVector NormalOnCreasePlaneB = FVector::VectorPlaneProject(PrevHitNormal, TmpBlockingCreaseDirection).GetSafeNormal();
			const float DotPlanesOnCreasePlane = NormalOnCreasePlaneA | NormalOnCreasePlaneB;

			const FVector EnteringVelocityDirectionOnCreasePlane = FVector::VectorPlaneProject(PrevCharacterVelocity, TmpBlockingCreaseDirection).GetSafeNormal();

			if (DotPlanesOnCreasePlane <= ((-EnteringVelocityDirectionOnCreasePlane | NormalOnCreasePlaneA) + 0.001f) &&
				DotPlanesOnCreasePlane <= ((-EnteringVelocityDirectionOnCreasePlane | NormalOnCreasePlaneB) + 0.001f))
			{
				bIsVelocityConstrainedByCrease = true;
			}
		}

		if (bIsVelocityConstrainedByCrease)
		{
			// Flip Crease Direction To Make It Representative Of The Real Direction Our Velocity Would Be Projected To
			if ((TmpBlockingCreaseDirection | CurCharacterVelocity) < 0)
			{
				TmpBlockingCreaseDirection *= -1;
			}

			bIsValidCrease = true;
			CreaseDirection = TmpBlockingCreaseDirection;
		}
	}
}


// TODO: Step Handling Checks If Collider Is Physics Object
void UKinematicMovementComponent::EvaluateHitStability(UPrimitiveComponent* HitCollider, FVector HitNormal, FVector HitPoint, FVector atCharPosition, FQuat atCharRotation, FVector withCharVelocity, FHitStabilityReport& StabilityReport)
{
	if (!bSolveGrounding)
	{
		StabilityReport.bIsStable = false;
		return;
	}

	// Get character up direction
	FVector atCharUp = atCharRotation * CachedWorldUp;
	// Get the component of the hit normal along the "ground" plane (defined by player up)
	FVector InnerHitDirection = FVector::VectorPlaneProject(HitNormal, atCharUp).GetSafeNormal();

	// Check stability of ground normal
	StabilityReport.bIsStable = IsStableOnNormal(HitNormal);

	/* Initialize Hit Stability Report To Defaults */
	StabilityReport.bFoundInnerNormal = false;
	StabilityReport.bFoundOuterNormal = false;
	StabilityReport.InnerNormal = HitNormal;
	StabilityReport.OuterNormal = HitNormal;

	/* */
	if (bLedgeAndDenivelationHandling)
	{
		float LedgeCheckHeight = MinDistanceForLedge;
		/* Check For Step Up Ledges If Enabled */
		if (StepHandling != EStepHandlingMethod::None)
		{
			LedgeCheckHeight = MaxStepHeight;
		}

		bool bIsStableLedgeInner = false;
		bool bIsStableLedgeOuter = false;

		/*
		 * Inner hit And Outer hit are defined by horizontal distances along the "ground plane"
		 * If one or the other does not hit anything, that is considered a ledge (makes sense)
		 */
		FHitResult InnerLedgeHit, OuterLedgeHit;
		if (CollisionLineCasts(
			HitPoint + (atCharUp * SecondaryProbesVertical) + (InnerHitDirection * SecondaryProbesHorizontal),
			-atCharUp,
			LedgeCheckHeight + SecondaryProbesVertical,
			InnerLedgeHit,
			InternalCharacterHits) > 0)
		{
			FVector InnerLedgeNormal = InnerLedgeHit.ImpactNormal;
			StabilityReport.InnerNormal = InnerLedgeNormal;
			StabilityReport.bFoundInnerNormal = true;
			bIsStableLedgeInner = IsStableOnNormal(InnerLedgeNormal);
		}

		if (CollisionLineCasts(
			HitPoint + (atCharUp * SecondaryProbesVertical) + (-InnerHitDirection * SecondaryProbesHorizontal),
			-atCharUp,
			LedgeCheckHeight + SecondaryProbesVertical,
			OuterLedgeHit,
			InternalCharacterHits) > 0)
		{
			FVector OuterLedgeNormal = OuterLedgeHit.ImpactNormal;
			StabilityReport.OuterNormal = OuterLedgeNormal;
			StabilityReport.bFoundOuterNormal = true;
			bIsStableLedgeOuter = IsStableOnNormal(OuterLedgeNormal);
		}

		// TODO: Good entry point to make an obstruction here to prevent movement off ledges
		StabilityReport.bLedgeDetected = (bIsStableLedgeInner != bIsStableLedgeOuter);

		/* If we found a ledge, get all data we can get from it that is relevant */
		if (StabilityReport.bLedgeDetected)
		{
			StabilityReport.bIsOnEmptySideOfLedge = bIsStableLedgeOuter && !bIsStableLedgeInner;
			StabilityReport.LedgeGroundNormal = bIsStableLedgeOuter ? StabilityReport.OuterNormal : StabilityReport.InnerNormal;
			StabilityReport.LedgeRightDirection = (HitNormal ^ StabilityReport.LedgeGroundNormal).GetSafeNormal();
			StabilityReport.LedgeFacingDirection = FVector::VectorPlaneProject(StabilityReport.LedgeGroundNormal ^ StabilityReport.LedgeRightDirection, CharacterUp).GetSafeNormal();
			StabilityReport.DistanceFromLedge = FVector::VectorPlaneProject((HitPoint - (atCharPosition + (atCharRotation * TransformToCapsuleBottom))), atCharUp).Size();
			StabilityReport.bIsMovingTowardsEmptySideOfLedge = (withCharVelocity.GetSafeNormal() | StabilityReport.LedgeFacingDirection) > 0.f;
		}

		if (StabilityReport.bIsStable)
		{
			StabilityReport.bIsStable = IsStableWithSpecialCases(StabilityReport, withCharVelocity);
		}
	}

	if (StepHandling != EStepHandlingMethod::None && !StabilityReport.bIsStable)
	{
		// TODO: Could we just do this check instead? Rather than querying by object type which I feel like is finicky
		if (HitCollider->IsSimulatingPhysics())
		{
			
		}
		// TODO: This
		// I think we ignore everything except static and dynamic. Pawn,PhysicsBody,Vehicle,Destructible are the equivalent of our rigidbodies?
		switch (HitCollider->GetCollisionObjectType())
		{
			/* Still not quite understanding Unreals Collision types like this, but I assume we only need to check for steps on these
			 * while obviously ignoring physics actors and pawns because we can't step on them. These are the only two classes of
			 * collision that are static or moving environments. Maybe we do need to check for pawns too.
			 */
		case ECC_WorldStatic: 
		case ECC_WorldDynamic:
			DetectSteps(atCharPosition, atCharRotation, HitPoint, InnerHitDirection, StabilityReport);
			if (StabilityReport.bValidStepDetected)
			{
				StabilityReport.bIsStable = true;
			}
			break;
		default: ;
		}
	
	}

	// Call event on character controller
}


#pragma endregion Evaluations


#pragma region Velocity & Movement Handlers


// TODO: Incomplete, physics interactions stuff here. I think it just wants us to store it and handle it later, but
// i might just leave all the physics interactions stuff to how GMC does it
bool UKinematicMovementComponent::InternalCharacterMove(FVector& TransientVelocity, float DeltaTime)
{
	// TODO: Curious as to when this is the case
	if (DeltaTime <= 0.f) return false;
	
	bool bWasCompleted = true;

	FVector RemainingMovementDirection = TransientVelocity.GetSafeNormal();
	float RemainingMovementMagnitude = TransientVelocity.Size() * DeltaTime;

	FVector OriginalVelocityDirection = RemainingMovementDirection;

	int SweepsMade = 0;
	bool bHitSomethingThisSweepIteration = true;
	
	FVector TmpMovedPosition = TransientPosition;
	bool bPreviousHitIsStable = false;

	FVector PreviousVelocity = CachedZeroVector;
	FVector PreviousObstructionNormal = CachedZeroVector;

	EMovementSweepState SweepState = EMovementSweepState::Initial;

	// Project Movement Against Current Overlaps Before Doing Sweeps
	for (int i = 0; i < OverlapsCount; i++)
	{
		FVector OverlapNormal = StoredOverlaps[i].Normal;
		if ((RemainingMovementDirection | OverlapNormal) < 0.f)
		{
			bool bStableOnHit = IsStableOnNormal(OverlapNormal) && !MustUnground();
			FVector VelocityBeforeProjection = TransientVelocity;
			FVector ObstructionNormal = GetObstructionNormal(OverlapNormal, bStableOnHit);

			InternalHandleVelocityProjection(
				bStableOnHit,
				OverlapNormal,
				ObstructionNormal,
				OriginalVelocityDirection,
				SweepState,
				bPreviousHitIsStable,
				PreviousVelocity,
				PreviousObstructionNormal,
				TransientVelocity,
				RemainingMovementMagnitude,
				RemainingMovementDirection);

			bPreviousHitIsStable = bStableOnHit;
			PreviousVelocity = VelocityBeforeProjection;
			PreviousObstructionNormal = ObstructionNormal;
		}
	}
	
	// Now sweep the desired movement to detect collisions
	while (RemainingMovementMagnitude > 0.f && (SweepsMade <= MaxMovementIterations) && bHitSomethingThisSweepIteration)
	{
		/* Setup sweep data */
		bool bFoundClosestHit = false;
		FVector ClosestSweepHitPoint = CachedZeroVector;
		FVector ClosestSweepHitNormal = CachedZeroVector;
		float ClosestSweepHitDistance = 0.f;
		UPrimitiveComponent* ClosestSweepHitCollider{nullptr};

		// TODO: This whole thing, seems important! Given that we use SafeMoveUpdatedComponent, that might solve this for us however
		/* Quick check for overlaps before performing movement */
		if (bCheckMovementInitialOverlaps)
		{
			FHitResult OverlapResult;
			bFoundClosestHit = GetClosestOverlap(TmpMovedPosition, RemainingMovementDirection, OverlapResult, ClosestSweepHitNormal, ClosestSweepHitPoint);
			if (bFoundClosestHit) ClosestSweepHitCollider = OverlapResult.GetComponent();
		}

		FHitResult SweepHit(NoInit);
		const int Sweeps = CollisionSweeps(TmpMovedPosition, TransientRotation, RemainingMovementDirection, RemainingMovementMagnitude + CollisionOffset, SweepHit, InternalCharacterHits);

		if (!bFoundClosestHit && Sweeps > 0)
		{
			ClosestSweepHitNormal = SweepHit.ImpactNormal;
			ClosestSweepHitPoint = SweepHit.ImpactPoint;
			ClosestSweepHitCollider = SweepHit.GetComponent();
			ClosestSweepHitDistance = SweepHit.Distance;

			bFoundClosestHit = true;
		}

		/* If we hit something during the move, begin solving. Otherwise the move was successful. */
		if (bFoundClosestHit)
		{
			/* Calculate movement from this iteration */
			FVector SweepMovement = RemainingMovementDirection * FMath::Max(0.f, ClosestSweepHitDistance - CollisionOffset);
			TmpMovedPosition += SweepMovement;
			RemainingMovementMagnitude -= SweepMovement.Size();

			/* Evaluate if hit is stable */
			FHitStabilityReport MoveHitStabilityReport;
			EvaluateHitStability(ClosestSweepHitCollider, ClosestSweepHitNormal, ClosestSweepHitPoint, TmpMovedPosition, TransientRotation, TransientVelocity, MoveHitStabilityReport);
			
			/* Handle stepping up steps higher than bottom capsule radius */
			bool bFoundValidStepHit = false;

			if (bSolveGrounding && StepHandling != EStepHandlingMethod::None && MoveHitStabilityReport.bValidStepDetected)
			{
				float ObstructionCorrelation = FMath::Abs(ClosestSweepHitNormal | CharacterUp);
				if (ObstructionCorrelation <= CorrelationForVerticalObstruction)
				{
					/* Initialize Stepping Parameters */
					FVector StepForwardDirection = FVector::VectorPlaneProject(-ClosestSweepHitNormal, CharacterUp).GetSafeNormal();
					FVector StepCastStartPoint = (TmpMovedPosition + (StepForwardDirection * SteppingForwardDistance)) + (CharacterUp * MaxStepHeight);
					FHitResult ClosestStepHit(NoInit);
					
					/* Cast downward from the top of the stepping height */
					int NumStepHits = CollisionSweeps(
									StepCastStartPoint,
									TransientRotation,
									-CharacterUp,
									MaxStepHeight,
									ClosestStepHit,
									InternalCharacterHits,
									0.f, true);

					/* Check for hit corresponding to stepped collider */
					for (int i = 0; i < NumStepHits; i++)
					{
						if (InternalCharacterHits[i].GetComponent() == MoveHitStabilityReport.SteppedCollider)
						{
							FVector EndStepPosition = StepCastStartPoint + (-CharacterUp * (InternalCharacterHits[i].Distance - CollisionOffset));
							TmpMovedPosition = EndStepPosition;
							bFoundValidStepHit = true;

							/* Project velocity on ground normal at step */
							TransientVelocity = FVector::VectorPlaneProject(TransientVelocity, CharacterUp);
							RemainingMovementDirection = TransientVelocity.GetSafeNormal();

							break;
						}
					}
				}
			}

			/* Handle movement solving */
			if (!bFoundValidStepHit)
			{
				FVector ObstructionNormal = GetObstructionNormal(ClosestSweepHitNormal, MoveHitStabilityReport.bIsStable);

				/* EVENT: OnMovementHit */

				bool bStableOnHit = MoveHitStabilityReport.bIsStable && !MustUnground();
				FVector VelocityBeforeProj = TransientVelocity;

				/* Project Velocity For Next Iteration */
				InternalHandleVelocityProjection(
								bStableOnHit,
								ClosestSweepHitNormal,
								ObstructionNormal,
								OriginalVelocityDirection,
								SweepState,
								bPreviousHitIsStable,
								PreviousVelocity,
								PreviousObstructionNormal,
								TransientVelocity,
								RemainingMovementMagnitude,
								RemainingMovementDirection);

				bPreviousHitIsStable = bStableOnHit;
				PreviousVelocity = VelocityBeforeProj;
				PreviousObstructionNormal = ObstructionNormal;
			}
		}
		/* Else if we hit nothing */
		else
		{
			bHitSomethingThisSweepIteration = false;
		}

		/* Safety for exceeding max sweeps allowed */
		SweepsMade++;
		if (SweepsMade > MaxMovementIterations)
		{
			if (bKillRemainingMovementWhenExceedMovementIterations)
			{
				RemainingMovementMagnitude = 0.f;
			}

			if (bKillVelocityWhenExceedMaxMovementIterations)
			{
				TransientVelocity = CachedZeroVector;
			}
			bWasCompleted = true;
		}
	}

	/* Move position for the remaining of the movement */
	TmpMovedPosition += (RemainingMovementDirection * RemainingMovementMagnitude);
	TransientPosition = TmpMovedPosition;

	return bWasCompleted;
}

void UKinematicMovementComponent::InternalHandleVelocityProjection(bool bStableOnHit, FVector HitNormal, FVector ObstructionNormal, FVector OriginalDistance, EMovementSweepState& SweepState, bool bPreviousHitIsStable,
									FVector PrevVelocity, FVector PrevObstructionNormal, FVector& TransientVelocity, float& RemainingMovementMagnitude, FVector& RemainingMovementDirection)
{
	if (TransientVelocity.SizeSquared() <= 0)
	{
		return; // No need to project if zero velocity
	}

	const FVector VelocityBeforeProjection = TransientVelocity;

	if (bStableOnHit)
	{
		bLastMovementIterationFoundAnyGround = true;
		HandleVelocityProjection(TransientVelocity, ObstructionNormal, bStableOnHit);
	}
	else
	{
		// Handle Projection
		if (SweepState == EMovementSweepState::Initial)
		{
			HandleVelocityProjection(TransientVelocity, ObstructionNormal, bStableOnHit);
			SweepState = EMovementSweepState::AfterFirstHit;
		}
		// Blocking Crease Handling
		else if (SweepState == EMovementSweepState::AfterFirstHit)
		{
			bool bFoundCrease;
			FVector CreaseDirection;
			EvaluateCrease(
				TransientVelocity,
				PrevVelocity,
				ObstructionNormal,
				PrevObstructionNormal,
				bStableOnHit,
				bPreviousHitIsStable,
				GroundingStatus.bIsStableOnGround && !MustUnground(),
				bFoundCrease,
				CreaseDirection);

			if (bFoundCrease)
			{
				if (GroundingStatus.bIsStableOnGround && !MustUnground())
				{
					TransientVelocity = FVector::ZeroVector;
					SweepState = EMovementSweepState::FoundBlockingCorner;
				}
				else
				{
					TransientVelocity = TransientVelocity.ProjectOnTo(CreaseDirection);
					SweepState = EMovementSweepState::FoundBlockingCrease;
				}
			}
			else
			{
				HandleVelocityProjection(TransientVelocity, ObstructionNormal, bStableOnHit);
			}
		}
		// Blocking Corner Handling
		else if (SweepState == EMovementSweepState::FoundBlockingCrease)
		{
			TransientVelocity = FVector::ZeroVector;
			SweepState = EMovementSweepState::FoundBlockingCorner;
		}
	}

	const float NewVelocityFactor = TransientVelocity.Size() / VelocityBeforeProjection.Size();
	RemainingMovementMagnitude *= NewVelocityFactor;
	RemainingMovementDirection = TransientVelocity.GetSafeNormal();
}

void UKinematicMovementComponent::HandleVelocityProjection(FVector& TransientVelocity, FVector ObstructionNormal, bool bStableOnHit)
{

	if (GroundingStatus.bIsStableOnGround && !MustUnground())
	{
		/* On stable slopes, simply reorient the movement without any loss */
		if (bStableOnHit)
		{
			TransientVelocity = GetDirectionTangentToSurface(TransientVelocity, ObstructionNormal) * TransientVelocity.Size();
		}
		/* On blocking hits, project the movement on the obstruction while following the grounding plane */
		else
		{
			TransientVelocity = FVector::VectorPlaneProject(TransientVelocity, ObstructionNormal);
		}
	}
	/* In air just project against any obstructions */
	else
	{
		TransientVelocity = FVector::VectorPlaneProject(TransientVelocity, ObstructionNormal);
	}
}


#pragma endregion Velocity & Movement Handlers

#pragma region Step Handling

//TODO: Need to fiure out pointer assignment
void UKinematicMovementComponent::DetectSteps(FVector CharPosition, FQuat CharRotation, FVector HitPoint, FVector InnerHitDirection, FHitStabilityReport& StabilityReport)
{
	int NumStepHits = 0;
	FHitResult TmpHit(NoInit);
	FHitResult OuterStepHitResult;

	FVector LocalCharUp = CharRotation * CachedWorldUp;
	FVector VerticalCharToHit = (HitPoint - CharPosition).ProjectOnToNormal(LocalCharUp).GetSafeNormal();
	FVector HorizontalCharToHitDirection = FVector::VectorPlaneProject((HitPoint - CharPosition), LocalCharUp).GetSafeNormal();
	FVector StepCheckStartPos = (HitPoint - VerticalCharToHit) + (LocalCharUp * MaxStepHeight) + (HorizontalCharToHitDirection * CollisionOffset * 3.f);

	// Do outer step check with capsule cast on hit point
	NumStepHits = CollisionSweeps(
		StepCheckStartPos,
		CharRotation,
		-LocalCharUp,
		MaxStepHeight + CollisionOffset,
		OuterStepHitResult,
		InternalCharacterHits,
		0.f, true);

	// Check for overlaps and obstructions at the hit point
	if (CheckStepValidity(NumStepHits, CharPosition, CharRotation, InnerHitDirection, StepCheckStartPos, TmpHit))
	{
		StabilityReport.bValidStepDetected = true;
		StabilityReport.SteppedCollider = TmpHit.GetComponent();
	}

	if (StepHandling == EStepHandlingMethod::Extra && !StabilityReport.bValidStepDetected)
	{
		// Do min reach step check with capsule cast on hit point
		StepCheckStartPos = CharPosition + (LocalCharUp * MaxStepHeight) + (-InnerHitDirection * MinRequiredStepDepth);
		NumStepHits = CollisionSweeps(
			StepCheckStartPos,
			CharRotation,
			-LocalCharUp,
			MaxStepHeight - CollisionOffset,
			OuterStepHitResult,
			InternalCharacterHits,
			0.f, true);
		
		if (CheckStepValidity(NumStepHits, CharPosition, CharRotation, InnerHitDirection, StepCheckStartPos, TmpHit))
		{
			StabilityReport.bValidStepDetected = true;
			StabilityReport.SteppedCollider = TmpHit.GetComponent();
		}
	}
}

// TODO: Check for when to use normal, impact normal or location, impact point. Also best way to store InternalCharacterHits and all that
bool UKinematicMovementComponent::CheckStepValidity(int NumStepHits, FVector CharPosition, FQuat CharRotation, FVector InnerHitDirection, FVector StepCheckStartPost, FHitResult& OutHit)
{
	//*HitCollider = NULL;
	FVector LocalCharUp = CharRotation * CachedWorldUp;

	// Find the farthest valid hit for stepping
	bool bFoundValidStepPosition = false;

	while (NumStepHits > 0 && !bFoundValidStepPosition)
	{
		// Get farthest hit among the remaining hits
		FHitResult FarthestHit;
		float FarthestDistance = 0.f;
		int FarthestIndex = 0;
		for (int i = 0; i < NumStepHits; i++)
		{
			float HitDistance = InternalCharacterHits[i].Distance;
			if (HitDistance > FarthestDistance)
			{
				FarthestDistance = HitDistance;
				FarthestHit = InternalCharacterHits[i];
				FarthestIndex = i;
			}
		}

		FVector CharPositionAtHit = StepCheckStartPost + (-LocalCharUp * (FarthestHit.Distance - CollisionOffset));

		int AtStepOverlaps = CollisionOverlaps(CharPositionAtHit, CharRotation, InternalProbedColliders);
		if (AtStepOverlaps <= 0)
		{
			FHitResult OuterSlopeHit;
			// Check for outer hit slope normal stability at the step position
			if (CollisionLineCasts(
				FarthestHit.ImpactPoint + (LocalCharUp * SecondaryProbesVertical) + (-InnerHitDirection * SecondaryProbesHorizontal),
				-LocalCharUp,
				MaxStepHeight + SecondaryProbesVertical,
				OuterSlopeHit,
				InternalCharacterHits,
				true) > 0)
			{
				if (IsStableOnNormal(OuterSlopeHit.ImpactNormal))
				{
					FHitResult TmpUpObstructionHit;
					// Cast Upward To Detect Any Obstructions To Moving There
					if (CollisionSweeps(
						CharPosition,
						CharRotation,
						LocalCharUp,
						MaxStepHeight - FarthestHit.Distance,
						TmpUpObstructionHit,
						InternalCharacterHits) <= 0)
					{
						// Do Inner Step Check
						bool bInnerStepValid = false;
						FHitResult InnerStepHit;

						if (bAllowSteppingWithoutStableGrounding) bInnerStepValid = true;
						else
						{
							// At the capsule center at the step height
							if (CollisionLineCasts(
								CharPosition + (CharPositionAtHit - CharPosition).ProjectOnToNormal(LocalCharUp),
								-LocalCharUp,
								MaxStepHeight,
								InnerStepHit,
								InternalCharacterHits,
								true) > 0)
							{
								if (IsStableOnNormal(InnerStepHit.ImpactNormal))
								{
									bInnerStepValid = true;
								}
							}
						}

						if (!bInnerStepValid)
						{
							// At the inner step of the step point
							if (CollisionLineCasts(
								FarthestHit.ImpactPoint + (InnerHitDirection * SecondaryProbesHorizontal),
								-LocalCharUp,
								MaxStepHeight,
								InnerStepHit,
								InternalCharacterHits,
								true) > 0)
							{
								if (IsStableOnNormal(InnerStepHit.ImpactNormal))
								{
									bInnerStepValid = true;
								}
							}
						}

						// Final validation of the step
						if (bInnerStepValid)
						{
							OutHit = FarthestHit;
							bFoundValidStepPosition = true;
							return true;
						}
					}
				}
			}
		}

		// Discard Hit if not valid step
		if (!bFoundValidStepPosition)
		{
			NumStepHits--;
			if (FarthestIndex < NumStepHits)
			{
				InternalCharacterHits[FarthestIndex] = InternalCharacterHits[NumStepHits];
			}
		}
	}

	return false;
}



#pragma endregion Step Handling

#pragma region Collision Validity

bool UKinematicMovementComponent::CheckIfColliderValidForCollisions(UPrimitiveComponent* Collider)
{

	if (Collider == Capsule) return false;

	// TODO: Ignore Moving Base!
	/* If component is the moving base, we ignore it */
	//if (Collider == MovingBase) return false;

	return true;
}

#pragma endregion Collision Validity


#pragma region Collision Checks

void UKinematicMovementComponent::ProbeGround(FVector& ProbingPosition, FQuat AtRotation, float ProbingDistance, FGroundingReport& GroundingReport)
{
	/* Validate Input */
	if (ProbingDistance < MinimumGroundProbingDistance)
	{
		ProbingDistance = MinimumGroundProbingDistance;
	}

	/* Initialize Data Needed For Ground Probing */
	int GroundSweepsMade = 0;
	FHitResult GroundSweepHit{NoInit};
	bool bGroundSweepingIsOver = false;
	FVector GroundSweepPosition = ProbingPosition;
	FVector GroundSweepDirection = (AtRotation * -CachedWorldUp);	// Ensure ground probing at arbitrary rotation
	float GroundProbeDistanceRemaining = ProbingDistance;

	/* Sweep until we detect something (bGroundSweepingIsOver), Until Iterations Consumed, or Nothing Found Within Probing Distance */
	while (GroundProbeDistanceRemaining > 0 && (GroundSweepsMade <= MaxGroundSweepIterations) && !bGroundSweepingIsOver)
	{
		/* Sweep For Ground Detection */
		if (GroundSweep(GroundSweepPosition, AtRotation, GroundSweepDirection, GroundProbeDistanceRemaining, GroundSweepHit))
		{
			/* If we hit ground, this is our snapping target - Evaluate its stability first before actually snapping */
			FVector TargetPosition = GroundSweepPosition + (GroundSweepDirection * GroundSweepHit.Distance);
			FHitStabilityReport GroundHitStabilityReport{};
			EvaluateHitStability(GroundSweepHit.GetComponent(), GroundSweepHit.ImpactNormal, GroundSweepHit.ImpactPoint, TargetPosition, TransientRotation, Velocity, GroundHitStabilityReport);

			/* Fill out our grounding report based on the evaluation and sweep */
			GroundingReport.bFoundAnyGround = true;
			GroundingReport.GroundNormal = GroundSweepHit.ImpactNormal;
			GroundingReport.InnerGroundNormal = GroundHitStabilityReport.InnerNormal;
			GroundingReport.OuterGroundNormal = GroundHitStabilityReport.OuterNormal;
			GroundingReport.GroundHit = GroundSweepHit;
			GroundingReport.GroundPoint = GroundSweepHit.ImpactPoint;
			GroundingReport.bSnappingPrevented = false;

			/* If we found stable ground */
			if (GroundHitStabilityReport.bIsStable)
			{
				/* Find all scenarios where ground snapping should be cancelled */
				GroundingReport.bSnappingPrevented = !IsStableWithSpecialCases(GroundHitStabilityReport, Velocity);

				GroundingReport.bIsStableOnGround = true;

				/* Ground snapping */
				if (!GroundingReport.bSnappingPrevented)
				{
					ProbingPosition = GroundSweepPosition + (GroundSweepDirection * (GroundSweepHit.Distance - CollisionOffset));
				}

				/* EVENT: On Landed */
				if (!LastGroundingStatus.bFoundAnyGround)
				{
					OnLanded(GroundSweepHit.GetComponent(), GroundSweepHit.ImpactNormal, GroundSweepHit.ImpactPoint, GroundHitStabilityReport);
				}
				bGroundSweepingIsOver = true;
			}
			else
			{
				/* Calculate movement from this iteration and advance position */
				FVector SweepMovement = (GroundSweepDirection * GroundSweepHit.Distance) + ((AtRotation * CachedWorldUp) * FMath::Max(CollisionOffset, GroundSweepHit.Distance));
				GroundSweepPosition = GroundSweepPosition + SweepMovement;

				// Set remaining distance
				GroundProbeDistanceRemaining = FMath::Min(GroundProbeReboundDistance, FMath::Max(GroundProbeDistanceRemaining - SweepMovement.Size(), 0.f));

				// Reorient direction
				GroundSweepDirection = FVector::VectorPlaneProject(GroundSweepDirection, GroundSweepHit.ImpactNormal).GetSafeNormal();
			}
		}
		else
		{
			bGroundSweepingIsOver = true;
		}

		GroundSweepsMade++;
	}
}


// TODO: Add Ignore Actor (Self) In FCollisionQueryParams. Then In CheckIfColliderValidForCollisions we can skip the first check

int UKinematicMovementComponent::CollisionOverlaps(FVector Position, FQuat Rotation, TArray<FHitResult>& OverlappedColliders, float Inflate, bool AcceptOnlyStableGroundLayer)
{
	const UWorld* World = GetWorld();

	int NumHits = 0;
	if (World)
	{
		/* Initialize Sweep Parameters */
		FComponentQueryParams Params;
		FCollisionQueryParams QueryParams = FCollisionQueryParams::DefaultQueryParam;
		QueryParams.AddIgnoredActor(PawnOwner);
		QueryParams.AddIgnoredComponent(Capsule);
		FCollisionResponseParams ResponseParams;
		Capsule->InitSweepCollisionParams(Params, ResponseParams);
		/* The "Test Channel" Sets up what overlaps we detect */
		if (World->SweepMultiByChannel(
					OverlappedColliders,
					Position,
					Position,
					Rotation,
					Capsule->GetCollisionObjectType(),
					Capsule->GetCollisionShape(Inflate),
					QueryParams,
					ResponseParams))
		{
			const int NumUnfilteredHits = OverlappedColliders.Num();
			NumHits = NumUnfilteredHits;
			for (int i = NumUnfilteredHits - 1; i >= 0; i--)
			{
				if (!CheckIfColliderValidForCollisions(OverlappedColliders[i].GetComponent()))
				{
					NumHits--;
					if (i < NumHits)
					{
						OverlappedColliders[i] = OverlappedColliders[NumHits];
					}
				}
				else if (OverlappedColliders[i].bStartPenetrating)
				{
					DebugEvent("Penetrating...", FColor::Purple);
				}
			}
		}
	}
	
	return NumHits;
}


int UKinematicMovementComponent::CollisionSweeps(FVector Position, FQuat Rotation, FVector Direction, float Distance, FHitResult& ClosestHit, TArray<FHitResult>& Hits, float Inflate, bool AcceptOnlyStableGroundLayer)
{
	UWorld* World = GetWorld();

	int NumHits = 0;
	if (World)
	{
		FVector TraceStart = Position - Direction * GroundProbingBackstepDistance;
		FVector TraceEnd = Position + Direction * (Distance + SweepProbingBackstepDistance - GroundProbingBackstepDistance);

		/* Initialize Sweep Parameters */
		FComponentQueryParams Params;
		FCollisionQueryParams QueryParams = FCollisionQueryParams::DefaultQueryParam;
		QueryParams.AddIgnoredActor(PawnOwner);
		FCollisionResponseParams ResponseParams;
		Capsule->InitSweepCollisionParams(Params, ResponseParams);

		/* Perform Sweep */
		if (World->SweepMultiByChannel(
					Hits,
					TraceStart,
					TraceEnd,
					Rotation,
					UpdatedComponent->GetCollisionObjectType(),
					Capsule->GetCollisionShape(Inflate),
					QueryParams,
					ResponseParams))
		{
			int NumUnfilteredHits = Hits.Num();
			float ClosestDistance = INFINITY;
			NumHits = NumUnfilteredHits;

			for (int i = NumUnfilteredHits - 1; i >= 0; i--)
			{
				Hits[i].Distance -= SweepProbingBackstepDistance;

				FHitResult Hit = Hits[i];

				// Filter out invalid hits
				if (const float HitDistance = Hit.Distance; HitDistance <= 0.f || !CheckIfColliderValidForCollisions(Hit.GetComponent()))
				{
					NumHits--;
					if (i < NumHits)
					{
						Hits[i] = Hits[NumHits];
					}
				}
				else
				{
					if (HitDistance < ClosestDistance)
					{
						ClosestHit = Hit;
						ClosestDistance = HitDistance;
					}
				}
			}
		}
	}

	return NumHits;
}


bool UKinematicMovementComponent::GroundSweep(FVector Position, FQuat Rotation, FVector Direction, float Distance, FHitResult& ClosestHit)
{
	UWorld* World = GetWorld();

	bool bFoundValidHit = false;
	if (World)
	{
		/* Initialize Sweep Parameters */
		// TODO: If we want to setup specific ground layers, we'd just add it here
		FComponentQueryParams Params;
		FCollisionQueryParams QueryParams = FCollisionQueryParams::DefaultQueryParam;
		QueryParams.AddIgnoredActor(PawnOwner);
		FCollisionResponseParams ResponseParams;
		Capsule->InitSweepCollisionParams(Params, ResponseParams);

		/* Perform Sweep */
		if (World->SweepMultiByChannel(
					InternalCharacterHits,
					Position - (Direction * GroundProbingBackstepDistance),
					Position + Direction * (Distance - GroundProbingBackstepDistance),
					Rotation,
					UpdatedComponent->GetCollisionObjectType(),
					Capsule->GetCollisionShape(),
					QueryParams,
					ResponseParams))
		{
			int NumUnfilteredHits = InternalCharacterHits.Num();

			float ClosestDistance = INFINITY;

			for (int i = 0; i < NumUnfilteredHits; i++)
			{
				FHitResult Hit = InternalCharacterHits[i];

				// Find closest valid hit
				if (const float HitDistance = Hit.Distance; HitDistance > 0.f && CheckIfColliderValidForCollisions(Hit.GetComponent()))
				{
					if (HitDistance < ClosestDistance)
					{
						ClosestHit = Hit;
						ClosestHit.Distance -= GroundProbingBackstepDistance;
						ClosestDistance = HitDistance;

						bFoundValidHit = true;
					}
				}
			}
		}
	}
	return bFoundValidHit;
}

int UKinematicMovementComponent::CollisionLineCasts(FVector Position, FVector Direction, float Distance, FHitResult& ClosestHit, TArray<FHitResult>& Hits, bool AcceptOnlyStableGroundLayer)
{
	const UWorld* World = GetWorld();

	int NumHits = 0;
	if (World)
	{
		/* Initialize Sweep Parameters */
		FComponentQueryParams Params;
		FCollisionQueryParams QueryParams = FCollisionQueryParams::DefaultQueryParam;
		QueryParams.AddIgnoredActor(PawnOwner);
		FCollisionResponseParams ResponseParams;
		Capsule->InitSweepCollisionParams(Params, ResponseParams);

		if (AcceptOnlyStableGroundLayer)
		{
			// TODO: Maybe here we just need to trace for WorldStatic?
		}
		
		if (World->LineTraceMultiByChannel(
					Hits,
					Position,
					Position + Direction * Distance,
					UpdatedComponent->GetCollisionObjectType(),
					QueryParams,
					ResponseParams))
		{
			const int NumUnfilteredHits = Hits.Num();

			float ClosestDistance = INFINITY;
			NumHits = NumUnfilteredHits;

			for (int i = NumUnfilteredHits - 1; i >= 0; i--)
			{
				FHitResult Hit = Hits[i];

				// This checks for self-collision, though with our QueryParams I don't think it is necessary anymore
				if (const float HitDistance = Hit.Distance; HitDistance <= 0 || !CheckIfColliderValidForCollisions(Hit.GetComponent()))
				{
					NumHits--;
					if (i < NumHits)
					{
						Hits[i] = Hits[NumHits];
					}
				}
				else
				{
					if (HitDistance < ClosestDistance)
					{
						ClosestHit = Hit;
						ClosestDistance = HitDistance;
					}
				}
			}
		}
	}

	return NumHits;
}

bool UKinematicMovementComponent::ResolveOverlaps()
{
	const int NumOverlaps = CollisionOverlaps(TransientPosition, TransientRotation, InternalProbedColliders);
	
	if (NumOverlaps > 0)
	{
		/* Solve All Overlaps */
		for (int i = 0; i < NumOverlaps; i++)
		{
			if (InternalProbedColliders[i].bStartPenetrating)
			{
				/* Ignore Physics Objects */
				if (InternalProbedColliders[i].GetComponent()->IsSimulatingPhysics()) continue;
				
				/* Initialize Penetration Data */
				FVector ResolutionDirection = InternalProbedColliders[i].Normal;
				float ResolutionDistance = InternalProbedColliders[i].PenetrationDepth;

				/* Resolve Along Obstruction Direction */
				FHitStabilityReport MockReport{};
				MockReport.bIsStable = IsStableOnNormal(ResolutionDirection);
				ResolutionDirection = GetObstructionNormal(ResolutionDirection, MockReport.bIsStable);

				/* Solve Overlap */
				DrawDebugShit(InternalProbedColliders[i].ImpactPoint, InternalProbedColliders[i].ImpactPoint + ResolutionDirection * 100.f, FColor::Red);
				const FVector ResolutionMovement = ResolutionDirection * (ResolutionDistance);
				TransientPosition += ResolutionMovement;

				/* Remember Overlaps */
				if (OverlapsCount < StoredOverlaps.Num())
				{
					StoredOverlaps[OverlapsCount] = FCustomOverlapResult(ResolutionDirection, InternalProbedColliders[i].GetComponent());
					OverlapsCount++;
				}
			}
		}
		return false;
	}
	return true;
}

bool UKinematicMovementComponent::GetClosestOverlap(FVector Position, FVector MoveDirection, FHitResult& OutOverlap, FVector& OutNormal, FVector& OutPoint)
{
	bool bFoundClosestHit = false;
	
	const int NumOverlaps = CollisionOverlaps(
						Position,
						TransientRotation,
						InternalProbedColliders);
	
	if (NumOverlaps > 0)
	{
		float ClosestSweepHitDistance = 0.f;
		float MostObstructingOverlapNormalDotProduct = 2.f;

		for (int i = 0; i < NumOverlaps; i++)
		{
			if (InternalProbedColliders[i].bStartPenetrating)
			{
				float DotProduct = MoveDirection | InternalProbedColliders[i].Normal;
				if (DotProduct < 0.f && DotProduct < MostObstructingOverlapNormalDotProduct)
				{
					MostObstructingOverlapNormalDotProduct = DotProduct;

					OutNormal = InternalProbedColliders[i].Normal;
					OutOverlap = InternalProbedColliders[i];
					OutPoint = Position + (TransientRotation * TransformToCapsuleCenter) + (InternalProbedColliders[i].PenetrationDepth * InternalProbedColliders[i].Normal);

					bFoundClosestHit = true;
				}
			}
		}
	}
	
	return bFoundClosestHit;
}

bool UKinematicMovementComponent::AutoResolvePenetration()
{
	const FQuat CurrentRotation = UpdatedComponent->GetComponentQuat();
	const FVector TestDelta = {0.01f, 0.01f, 0.01f};
	FHitResult Hit;

	const auto TestMove = [&](const FVector& Delta)
	{
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
			return true;
		}
	}

	SafeMoveUpdatedComponent(TestDelta, CurrentRotation, true, Hit);
	SafeMoveUpdatedComponent(-TestDelta, CurrentRotation, true, Hit);

	return !Hit.bStartPenetrating;
}


#pragma endregion Collision Checks


#pragma region FLUFF

FVector UKinematicMovementComponent::GetVelocityFromMovement(FVector MoveDelta, float DeltaTime)
{
	if (DeltaTime <= 0.f) return CachedZeroVector;

	return MoveDelta / DeltaTime;
}


#pragma endregion FLUFF