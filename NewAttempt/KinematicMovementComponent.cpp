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

	TransformToCapsuleCenter = Capsule->GetCenterOfMass();
	TransformToCapsuleBottom = TransformToCapsuleCenter + (-CachedWorldUp * (CapsuleHeight*0.5f));
	TransformToCapsuleTop = TransformToCapsuleCenter + (CachedWorldUp * CapsuleHeight * 0.5f);
	TransformToCapsuleBottomHemi = TransformToCapsuleBottom + CachedWorldUp * CapsuleRadius;
	TransformToCapsuleTopHemi = TransformToCapsuleTop + CachedWorldUp * CapsuleRadius;

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
	AutoResolvePenetration();
	
	PreMovementUpdate(DeltaTime);

	MovementUpdate(DeltaTime);

}

#pragma region Solver Updates

/// @brief  Okay so here I need to do a few things.
///				1.) Handle dirty MovePosition calls, so we make a call to InternalCharacterMove() with the supplied Delta
///				2.) Solve initial overlaps & penetrations due to previous movements
///				3.) Store those overlaps that we compute above
///				4.) After that, we probe ground. If we want to unground, move up slightly otherwise probe the ground and perform snapping. If we just landed, project our velocity.
///				5.) Handle Movement & Rotation due to moving platforms at the end. We do this before we perform our move. Alternatively, we can leave it for later and do it after our move has been performed.
/// @param  DeltaTime DeltaTime of the tick
void UKinematicMovementComponent::PreMovementUpdate(float DeltaTime)
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
	TransientRotation = UpdatedComponent->GetComponentQuat();

	/* Cache our initial physics state before we perform any updates */
	InitialSimulatedPosition = UpdatedComponent->GetComponentLocation();;
	InitialSimulatedRotation = TransientRotation;

	OverlapsCount = 0; 
	bLastSolvedOverlapNormalDirty = false; 

	/* Handle MovePosition Direct Calls */
	if (bMovePositionDirty)
	{
		FVector TmpVelocity = GetVelocityFromMovement(MovePositionTarget - InitialSimulatedPosition, DeltaTime);
		if (bSolveMovementCollisions)
		{
			InternalCharacterMove(TmpVelocity, DeltaTime);
		}
		else
		{
			MoveUpdatedComponent(TmpVelocity * DeltaTime, UpdatedComponent->GetComponentQuat(), false);
		}
		bMovePositionDirty = false;
	}

	/* Setup Grounding Status For This Tick */
	LastGroundingStatus.CopyFrom(GroundingStatus);

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
		AutoResolvePenetration();
	}

	/* Handle ground probing and snapping */
	if (bSolveGrounding)
	{
		SolveGrounding(DeltaTime);
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

/// @brief  What we want to do here is as follows:
///				1.) Evt: UpdateRotation, get the new rotation from the character controller
///				2.) Handle dirty calls to Rotation (no need to sweep just yet)
///				3.) Resolve Penetrations Again (Maybe not?)
///				4.) Request New Velocity from character controller
///				5.) Call InternalCharacterMove to do the complicated shit
/// @param  DeltaTime DeltaTime of the tick
void UKinematicMovementComponent::MovementUpdate(float DeltaTime)
{
	/* EVENT: Update Rotation */
	UpdateRotation(TransientRotation, DeltaTime);

	/* Handle call to direct setting of rotation */
	if (bMoveRotationDirty)
	{
		UpdatedComponent->SetWorldRotation(MoveRotationTarget.Rotator());
		bMoveRotationDirty = false;
	}

	if (bSolveMovementCollisions) AutoResolvePenetration();
	
	if (GroundingStatus.bIsStableOnGround)
	{
		const float VelMag = Velocity.Size();
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
			MoveUpdatedComponent(Velocity * DeltaTime, UpdatedComponent->GetComponentQuat(), false);
		}
	}
	
	/* EVENT: After Character Update */
	AfterCharacterUpdate(DeltaTime);
}

void UKinematicMovementComponent::SolveGrounding(float DeltaTime)
{
	/* If ForceUnground() has been called, then move us up ever so slightly to bypass snapping and leave ground */
	if (MustUnground())
	{
		MoveUpdatedComponent(CharacterUp * (MinimumGroundProbingDistance * 1.5f), UpdatedComponent->GetComponentQuat(), false);
	}
	else
	{
		// Choose appropriate ground probing distance
		float SelectedGroundProbingDistance = MinimumGroundProbingDistance;
		if (!LastGroundingStatus.bSnappingPrevented && (LastGroundingStatus.bIsStableOnGround || bLastMovementIterationFoundAnyGround))
		{
			if (StepHandling != EStepHandlingMethod::None)
			{
				SelectedGroundProbingDistance = FMath::Max(CapsuleRadius, MaxStepHeight);
			}
			else
			{
				SelectedGroundProbingDistance = CapsuleRadius;
			}
			SelectedGroundProbingDistance += GroundDetectionExtraDistance;
		}
		
		ProbeGround(UpdatedComponent->GetComponentLocation(), UpdatedComponent->GetComponentQuat(), SelectedGroundProbingDistance, GroundingStatus);

		if (!LastGroundingStatus.bIsStableOnGround && GroundingStatus.bIsStableOnGround)
		{
			/* Landing! */
			Velocity = FVector::VectorPlaneProject(Velocity, CharacterUp);
			Velocity = GetDirectionTangentToSurface(Velocity, GroundingStatus.GroundNormal) * Velocity.Size();
		}
	}
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

// TODO: Temporarily changed this to be against world up for testing
bool UKinematicMovementComponent::IsStableOnNormal(const FVector Normal)
{
	const float NormalAngle = FMath::RadiansToDegrees(FMath::Acos(Normal.GetSafeNormal() | CachedWorldUp.GetSafeNormal()));
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
		float DotPlanes = FMath::Abs(CurHitNormal | PrevHitNormal);
		bool bIsVelocityConstrainedByCrease = false;

		// Avoid calculations if the two planes are the same
		if (DotPlanes < 0.999f)
		{
			//DebugEvent("Crease Plane Dot Product = " + FString::SanitizeFloat(DotPlanes), FColor::Purple);
			if (DotPlanes < 0.001f && false)
			{
				DebugEvent("Perpendicular Planes, each directed at: (" + FString::SanitizeFloat(CurHitNormal.X) + ", " + FString::SanitizeFloat(CurHitNormal.Y) + ", "
					+ FString::SanitizeFloat(CurHitNormal.Z) + ") And ("
					+ FString::SanitizeFloat(PrevHitNormal.X) + ", " + FString::SanitizeFloat(PrevHitNormal.Y) + ", "
					+ FString::SanitizeFloat(PrevHitNormal.Z) + ")");
			}
			const FVector NormalOnCreasePlaneA = FVector::VectorPlaneProject(CurHitNormal, TmpBlockingCreaseDirection).GetSafeNormal();
			const FVector NormalOnCreasePlaneB = FVector::VectorPlaneProject(PrevHitNormal, TmpBlockingCreaseDirection).GetSafeNormal();
			const float DotPlanesOnCreasePlane = NormalOnCreasePlaneA | NormalOnCreasePlaneB;

			const FVector EnteringVelocityDirectionOnCreasePlane = FVector::VectorPlaneProject(PrevCharacterVelocity, TmpBlockingCreaseDirection).GetSafeNormal();

			if (DotPlanesOnCreasePlane <= (FMath::Abs((-EnteringVelocityDirectionOnCreasePlane) | NormalOnCreasePlaneA) + 0.001f) &&
				DotPlanesOnCreasePlane <= (FMath::Abs((-EnteringVelocityDirectionOnCreasePlane) | NormalOnCreasePlaneB) + 0.001f))
			{
				//DebugEvent("Dot Velocity With Plane A = " + FString::SanitizeFloat(((-EnteringVelocityDirectionOnCreasePlane) | NormalOnCreasePlaneA) + 0.001f));
				//DebugEvent("Dot Velocity With Plane A = " + FString::SanitizeFloat(((-EnteringVelocityDirectionOnCreasePlane) | NormalOnCreasePlaneB) + 0.001f));
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
		if (!HitCollider->IsSimulatingPhysics())
		{
			DetectSteps(atCharPosition, atCharRotation, HitPoint, InnerHitDirection, StabilityReport);
			if (StabilityReport.bValidStepDetected)
			{
				StabilityReport.bIsStable = true;
			}
		}
	}

	// Call event on character controller
}


#pragma endregion Evaluations


#pragma region Velocity & Movement Handlers


/// @brief  What happens here is as follows:
///				1.) Project velocity against all overlaps previously stored in the movement updates (Theres never anything????)
///				2.) Enter a while loop until consumed sweep iterations or were successful in the move without any blocking hits
///				3.) At the beginning of each iteration, check for overlaps. Iterate through each penetrating overlap to find the most obstructing one
///				4.) If no overlaps found, sweep and get the first blocking hit
///				5.) If No Hit was Found, our move was successful... Exit...
///				6.) Otherwise, Evaluate the stability of the hit
/// @param  MoveVelocity Velocity for the move iteration
/// @param  DeltaTime DeltaTime of this tick
/// @return False if movement could not be solved until the end
bool UKinematicMovementComponent::InternalCharacterMove(FVector& MoveVelocity, float DeltaTime)
{
	// TODO: Curious as to when this is the case
	if (DeltaTime <= 0.f) return false;
	
	bool bWasCompleted = true;

	FVector RemainingMovementDirection = MoveVelocity.GetSafeNormal();
	float RemainingMovementMagnitude = MoveVelocity.Size() * DeltaTime;

	FVector OriginalVelocityDirection = RemainingMovementDirection;

	int SweepsMade = 0;
	bool bHitSomethingThisSweepIteration = true;
	
	bool bPreviousHitIsStable = false;

	FVector PreviousVelocity = CachedZeroVector;
	FVector PreviousObstructionNormal = CachedZeroVector;

	EMovementSweepState SweepState = EMovementSweepState::Initial;

	UpdatedPrimitive->SetGenerateOverlapEvents(true);
	int OverlapCount = UpdatedPrimitive->GetOverlapInfos().Num();


	
	// Now sweep the desired movement to detect collisions
	while (RemainingMovementMagnitude > 0.f && (SweepsMade <= MaxMovementIterations) && bHitSomethingThisSweepIteration)
	{
		/* Setup sweep data */
		bool bFoundClosestHit = false;
		FVector ClosestSweepHitPoint = CachedZeroVector;
		FVector ClosestSweepHitNormal = CachedZeroVector;
		float ClosestSweepHitDistance = 0.f;
		FHitResult ClosestSweepHit;

		// TODO: This whole thing, seems important! Given that we use SafeMoveUpdatedComponent, that might solve this for us however
		/* Quick check for overlaps before performing movement */
		
		if (!bFoundClosestHit)
		{
			FHitResult SweepHit;
			/* We are only interested in the hit result from the sweep at this point */
			MoveUpdatedComponent(RemainingMovementDirection * (RemainingMovementMagnitude), TransientRotation, true, &SweepHit);

			//if (SweepHit.bStartPenetrating) exit(0);
			if (SweepHit.bBlockingHit)
			{
				ClosestSweepHitNormal = SweepHit.ImpactNormal;
				ClosestSweepHitPoint = SweepHit.ImpactPoint;
				ClosestSweepHit = SweepHit;
				ClosestSweepHitDistance = SweepHit.Distance;

				// Dispatch Hit?
				//PawnOwner->DispatchBlockingHit(Capsule, SweepHit.GetComponent(), true, SweepHit);

				bFoundClosestHit = true;
			}
		}

		/* If we hit something during the move, begin solving. Otherwise the move was successful. */
		if (bFoundClosestHit)
		{
			if (ClosestSweepHit.bStartPenetrating)
			{
				DebugEvent("Most recent move put us in penetration!");
			}
			/* Calculate movement from this iteration, now we commit to the move */
			FVector SweepMovement = RemainingMovementDirection * FMath::Max(0.f, ClosestSweepHitDistance);// - CollisionOffset);
			// THE COLLISION OFFSET FROM ABOVE WAS THE REASON THE OVERLAP/HIT EVENTS WEREN'T DISPATCH WITH THE BELOW MOVE!!!
			RemainingMovementMagnitude -= SweepMovement.Size();

			/* Evaluate if hit is stable */
			FHitStabilityReport MoveHitStabilityReport;
			EvaluateHitStability(ClosestSweepHit.GetComponent(), ClosestSweepHitNormal, ClosestSweepHitPoint, UpdatedComponent->GetComponentLocation(), TransientRotation, MoveVelocity, MoveHitStabilityReport);
			
			/* Handle stepping up steps higher than bottom capsule radius */
			bool bFoundValidStepHit = false;

			if (bSolveGrounding && StepHandling != EStepHandlingMethod::None && MoveHitStabilityReport.bValidStepDetected)
			{
				float ObstructionCorrelation = FMath::Abs(ClosestSweepHitNormal | CharacterUp);
				if (ObstructionCorrelation <= CorrelationForVerticalObstruction)
				{
					/* Initialize Stepping Parameters */
					FVector StepForwardDirection = FVector::VectorPlaneProject(-ClosestSweepHitNormal, CharacterUp).GetSafeNormal();
					FVector StepCastStartPoint = (UpdatedComponent->GetComponentLocation() + (StepForwardDirection * SteppingForwardDistance)) + (CharacterUp * MaxStepHeight);
					FHitResult ClosestStepHit(NoInit);
					
					/* Cast downward from the top of the stepping height */
					int NumStepHits = CollisionSweeps(
									StepCastStartPoint,
									TransientRotation,
									-CharacterUp,
									MaxStepHeight,
									ClosestStepHit,
									InternalCharacterHits);

					/* Check for hit corresponding to stepped collider */
					for (int i = 0; i < NumStepHits; i++)
					{
						
						if (InternalCharacterHits[i].GetComponent() == MoveHitStabilityReport.SteppedCollider)
						{
							FVector EndStepPosition = StepCastStartPoint + (-CharacterUp * (InternalCharacterHits[i].Distance - CollisionOffset));
							/* Don't sweep step movement */
							MoveUpdatedComponent(EndStepPosition - UpdatedComponent->GetComponentLocation(), TransientRotation, false);
							bFoundValidStepHit = true;

							/* Project velocity on ground normal at step */
							MoveVelocity = FVector::VectorPlaneProject(MoveVelocity, CharacterUp);
							RemainingMovementDirection = MoveVelocity.GetSafeNormal();

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
				FVector VelocityBeforeProj = MoveVelocity;

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
								MoveVelocity,
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
			//MoveUpdatedComponent(RemainingMovementDirection * (RemainingMovementMagnitude + CollisionOffset), TransientRotation, true);
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
				MoveVelocity = CachedZeroVector;
			}
			bWasCompleted = true;
		}
	}

	/* Move position for the remaining of the movement */
	FHitResult DummyHit;
	MoveUpdatedComponent(RemainingMovementDirection * (RemainingMovementMagnitude + CollisionOffset), TransientRotation, true);

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

void UKinematicMovementComponent::HandleVelocityProjection(FVector& MoveVelocity, FVector ObstructionNormal, bool bStableOnHit)
{

	if (GroundingStatus.bIsStableOnGround && !MustUnground())
	{
		/* On stable slopes, simply reorient the movement without any loss */
		if (bStableOnHit)
		{
			MoveVelocity = GetDirectionTangentToSurface(MoveVelocity, ObstructionNormal) * MoveVelocity.Size();
		}
		/* On blocking hits, project the movement on the obstruction while following the grounding plane */
		else
		{
			MoveVelocity = FVector::VectorPlaneProject(MoveVelocity, ObstructionNormal);
		}
	}
	/* In air just project against any obstructions */
	else
	{
		MoveVelocity = FVector::VectorPlaneProject(MoveVelocity, ObstructionNormal);
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
			//GEngine->DeferredCommands.Add(TEXT("pause"));
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

		FVector CharPositionAtHit = StepCheckStartPost + (-LocalCharUp * (FarthestHit.Distance));// - CollisionOffset));

		// TODO: I SET THIS TO ZERO BEFORE
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
				if (IsStableOnNormal(OuterSlopeHit.ImpactNormal) || true)
				{
					FHitResult TmpUpObstructionHit;
					// Cast Upward To Detect Any Obstructions To Moving There
					// TODO: Could use MoveUpdatedComponent here with deferred scope and revert the move if penetrating 
					if (CollisionSweeps(
						CharPosition,
						CharRotation,
						LocalCharUp,
						MaxStepHeight - FarthestHit.Distance,
						TmpUpObstructionHit,
						InternalCharacterHits) <= 0 || true)
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

void UKinematicMovementComponent::ProbeGround(FVector ProbingPosition, FQuat AtRotation, float ProbingDistance, FGroundingReport& GroundingReport)
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
					MoveUpdatedComponent(GroundSweepDirection * (GroundSweepHit.Distance - CollisionOffset), AtRotation, true);
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

	if (World)
	{
		/* ============ POSSIBILITIES =================*/

		//World->OverlapBlockingTestByChannel()
		//MovementComponentCVars::PenetrationOverlapCheckInflation;
		
		/* ============================================*/

		
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
			return OverlappedColliders.Num();
		}
	}
	
	return 0;
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
				const float HitDistance = Hit.Distance;
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


bool UKinematicMovementComponent::ComputePenetration(FHitResult& OutHit, bool bAttemptResolve)
{
	
	const FQuat CurrentRotation = UpdatedComponent->GetComponentQuat();
	const FVector TestDelta = {0.01f, 0.01f, 0.01f};

	const auto TestMove = [&](const FVector& Delta)
	{
		FScopedMovementUpdate ScopedMovement(UpdatedComponent, EScopedUpdate::DeferredUpdates);

		// Check for penetrations by applying a small location delta.
		MoveUpdatedComponent(Delta, CurrentRotation, true, &OutHit);

		// Revert the movement again afterwards, we are only interested in the hit result.
		ScopedMovement.RevertMove();
	};

	TestMove(TestDelta);

	if (!OutHit.bBlockingHit || OutHit.IsValidBlockingHit())
	{
		// Apply the test delta in the opposite direction.
		TestMove(-TestDelta);

		if (!OutHit.bBlockingHit || OutHit.IsValidBlockingHit())
		{
			// The pawn is not in penetration, no action is needed.
			return false;
		}
	}

	if (bAttemptResolve)
	{
		SafeMoveUpdatedComponent(TestDelta, CurrentRotation, true, OutHit);
		SafeMoveUpdatedComponent(-TestDelta, CurrentRotation, true, OutHit);

		return !OutHit.bStartPenetrating;
	}
	
	return true;
}

void UKinematicMovementComponent::AutoResolvePenetration()
{

	int IterationsMade = 0;
	bool bOverlapSolved = false;

	while (IterationsMade < MaxDecollisionIterations && !bOverlapSolved)
	{
		/* Respect Decollision Iteration Count, Exit when overlap is solved*/
		int NumOverlaps = CollisionOverlaps(UpdatedComponent->GetComponentLocation(), TransientRotation, InternalProbedColliders);
		if (NumOverlaps > 0)
		{
			for (int i = 0; i < NumOverlaps; i++)
			{
				if (InternalProbedColliders[i].bStartPenetrating)
				{
					FHitResult PenetrationOverlap = InternalProbedColliders[i];
					FHitStabilityReport MockReport;
					MockReport.bIsStable = IsStableOnNormal(PenetrationOverlap.Normal);
					FVector ResolutionDirection = GetObstructionNormal(PenetrationOverlap.Normal, MockReport.bIsStable);

					// Solve Overlap
					FVector ResolutionMovement = ResolutionDirection * (PenetrationOverlap.PenetrationDepth + CollisionOffset);
					// TODO: Might want to sweep here and use it as a condition to determine when to exit this
					// Typically we want to depenetrate regardless of direction, so we can get all the way out of penetration quickly.
					// Our rules for "moving with depenetration normal" only get us so far out of the object. We'd prefer to pop out by the full MTD amount.
					// Depenetration moves (in ResolvePenetration) then ignore blocking overlaps to be able to move out by the MTD amount.
					MoveUpdatedComponent(ResolutionMovement, TransientRotation, false);

					// Remember Overlaps
					if (OverlapsCount < StoredOverlaps.Num())
					{
						StoredOverlaps[OverlapsCount] = PenetrationOverlap;
						OverlapsCount++;
					}

					break;
				}
			}
		}
		else
		{
			bOverlapSolved = true;
		}
		IterationsMade++;
	}
}


#pragma endregion Collision Checks


#pragma region FLUFF

FVector UKinematicMovementComponent::GetVelocityFromMovement(FVector MoveDelta, float DeltaTime)
{
	if (DeltaTime <= 0.f) return CachedZeroVector;

	return MoveDelta / DeltaTime;
}


#pragma endregion FLUFF