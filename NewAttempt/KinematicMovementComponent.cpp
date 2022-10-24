// Fill out your copyright notice in the Description page of Project Settings.


#include "KinematicMovementComponent.h"

#include "AbcObject.h"


// Sets default values for this component's properties
UKinematicMovementComponent::UKinematicMovementComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	// ...
	PrePhysicsTick.TickGroup = TG_PrePhysics;
	PrePhysicsTick.bCanEverTick = true;
	PrePhysicsTick.bStartWithTickEnabled = true;

	PostPhysicsTick.TickGroup = TG_PostPhysics;
	PostPhysicsTick.bCanEverTick = true;
	PostPhysicsTick.bStartWithTickEnabled = true;
}


// Called when the game starts
void UKinematicMovementComponent::BeginPlay()
{
	Super::BeginPlay();

	// WANT THIS TICK TO CALL EVERYTHING ELSE
	SetTickGroup(ETickingGroup::TG_DuringPhysics);
	// WANT THIS TICK TO CALL CUSTOM INTERPOLATION
	SetTickGroup(ETickingGroup::TG_PostPhysics);

	// Initialize it and cache it
	CachedQueryParams = SetupQueryParams();

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

	// Post Simulation Update
	PostSimulationInterpolationUpdate(DeltaTime);
}

#pragma region Simulation Updates

void UKinematicMovementComponent::PreSimulationInterpolationUpdate(float DeltaTime)
{
	InitialTickPosition = TransientPosition;
	InitialTickRotation = TransientRotation;

	// These take deltas
	UpdatedComponent->SetWorldLocationAndRotation(TransientPosition, TransientRotation);
}


void UKinematicMovementComponent::Simulate(float DeltaTime)
{
	UpdatePhase1(DeltaTime);
	UpdatePhase2(DeltaTime);

	UpdatedComponent->SetWorldLocationAndRotation(TransientPosition, TransientRotation);
}

void UKinematicMovementComponent::PostSimulationInterpolationUpdate(float DeltaTime)
{
	UpdatedComponent->SetWorldLocationAndRotation(InitialTickPosition, InitialTickRotation);
}


void UKinematicMovementComponent::CustomInterpolationUpdate(float DeltaTime)
{
	float InterpolationFactor;// = FMath::Clamp(Time)

	const FVector InterpolatedPosition = FMath::Lerp(InitialTickPosition, TransientPosition, InterpolationFactor);
	const FQuat InterpolatedRotation = FQuat::Slerp(InitialTickRotation, TransientRotation, InterpolationFactor);

	UpdatedComponent->SetWorldLocationAndRotation(InterpolatedPosition, InterpolatedRotation);
}


#pragma endregion Simulation Updates


#pragma region Solver Updates
/// <summary>
/// Update phase 1 is meant to be called after physics movers have calculated their velocities, but
/// before they have simulated their goal positions/rotations. It is responsible for:
/// - Initializing all values for update
/// - Handling MovePosition calls
/// - Solving initial collision overlaps
/// - Ground probing
/// - Handle detecting potential interactable rigidbodies
/// </summary>
void UKinematicMovementComponent::UpdatePhase1(float DeltaTime)
{
	// NaN Propagation Safety Stop
	if (FMath::IsNaN(BaseVelocity.X) || FMath::IsNaN(BaseVelocity.Y) || FMath::IsNaN(BaseVelocity.Z))
	{
		BaseVelocity = FVector::ZeroVector;
	}
	// TODO: Similar NaN check for the moving base if one exists. Might not be needed though since it wouldnt operate under this system

	TransientPosition = UpdatedComponent->GetComponentLocation();
	TransientRotation = UpdatedComponent->GetComponentRotation().Quaternion();
	InitialSimulatedPosition = TransientPosition;
	InitialSimulatedRotation = TransientRotation;

	OverlapsCount = 0;
	bLastSolvedOverlapNormalDirty = false;

	// Handle move position calls
	if (bMovePositionDirty)
	{
		if (bSolveMovementCollisions)
		{
			FVector TmpVelocity = GetVelocityFromMovement(MovePositionTarget - TransientPosition, DeltaTime);
			if (InternalCharacterMove(TmpVelocity, DeltaTime))
			{
				//TODO: Overlaps will be stored from this move, handle them if physics objects
			}
		}
		else
		{
			TransientPosition = MovePositionTarget;
		}
		bMovePositionDirty = false;
	}

	LastGroundingStatus->CopyFrom(*GroundingStatus);
	//TODO: Might be a better way of initializing this mebe
	GroundingStatus = new FGroundingReport();
	GroundingStatus->GroundNormal = CharacterUp;

	if (bSolveMovementCollisions)
	{
		// Solve Initial Overlaps
		FVector ResolutionDirection = CachedWorldUp;
		float ResolutionDistance = 0.f;
		int IterationsMade = 0;
		bool bOverlapSolved = false;

		while (IterationsMade < MaxDecollisionIterations && !bOverlapSolved)
		{
			int NumOverlaps = CollisionOverlaps(TransientPosition, TransientRotation, InternalProbedColliders);

			if (NumOverlaps > 0)
			{
				// Solver overlaps that have nothing to do with moving platforms or physics bodies
				for (int i = 0; i < NumOverlaps; i++)
				{
					bool IsInternalProbedCollidersiAPhysicsBody; // Temp variable for reference
					if (!IsInternalProbedCollidersiAPhysicsBody)
					{
						// Process Overlap
						FMTDResult PenetrationResult;
						// TODO: Check if this wants input of the overlapped object, or me
						// Also oh shit i can pass inflate here. Should use the getcollisionshape method for the collision tests below
						if (Capsule->ComputePenetration(PenetrationResult, Capsule->GetCollisionShape(0), TransientPosition, TransientRotation))
						{
							// Resolve along obstruction direction
							FHitStabilityReport* MockReport = new FHitStabilityReport;
							MockReport->bIsStable = IsStableOnNormal(PenetrationResult.Direction);
							ResolutionDirection = GetObstructionNormal(ResolutionDirection, MockReport->bIsStable);

							// Solve overlap
							const FVector ResolutionMovement = ResolutionDirection * (ResolutionDistance + CollisionOffset);
							TransientPosition += ResolutionMovement;

							// Remember Overlaps
							if (OverlapsCount < Overlaps.Num())
							{
								// TODO: Data management (Can just not use New Keyword here?)
								Overlaps[OverlapsCount] = FCustomOverlapResult(ResolutionDirection, InternalProbedColliders[i].GetComponent());
								OverlapsCount++;
							}
							break;
						}
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

	// Handling Ground Probing And Snapping
	if (bSolveGrounding)
	{
		if (MustUnground())
		{
			TransientPosition += CharacterUp * (MinimumGroundProbingDistance * 1.5f);
		}
		else
		{
			// Choose appropriate ground probing distance
			float SelectedGroundProbingDistance = MinimumGroundProbingDistance;
			if (!LastGroundingStatus->bSnappingPrevented && (LastGroundingStatus->bIsStableOnGround || bLastMovementIterationFoundAnyGround))
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

			if (!LastGroundingStatus->bIsStableOnGround && GroundingStatus->bIsStableOnGround)
			{
				// Handle Landing
				// TODO: Maybe call an OnLanded event here
				BaseVelocity = FVector::VectorPlaneProject(BaseVelocity, CharacterUp);
				BaseVelocity = GetDirectionTangentToSurface(BaseVelocity, GroundingStatus->GroundNormal) * BaseVelocity.Size();
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
		// TODO: Call event
	}

	// TODO: HERES WHERE YOUD ADD THE CODE FOR MOVING PLATFORMS
	
}

/// <summary>
/// Update phase 2 is meant to be called after physics movers have simulated their goal positions/rotations. 
/// At the end of this, the TransientPosition/Rotation values will be up-to-date with where the motor should be at the end of its move. 
/// It is responsible for:
/// - Solving Rotation
/// - Handle MoveRotation calls
/// - Solving potential attached rigidbody overlaps
/// - Solving Velocity
/// - Applying planar constraint
/// </summary>
void UKinematicMovementComponent::UpdatePhase2(float DeltaTime)
{
	// TODO: Call UpdateRotation Event

	// Handle move rotation
	if (bMoveRotationDirty)
	{
		TransientRotation = MoveRotationTarget;
		bMoveRotationDirty = false;
	}

	// Handle physics interactions here?
	if (bSolveMovementCollisions && bPhysicsInteractions)
	{
		if (bPhysicsInteractions)
		{
			// Solve potential physics body overlaps
			// The base velocity should be calculated before moving along the floor but applied afterwards.
			//const FVector BaseVelocity = bMoveWithBase ? ComputeBaseVelocity(CurrentFloor.ShapeHit().GetComponent()) : FVector{0};
			//if (bMoveWithBase) MoveWithBase(BaseVelocity, CurrentFloor, DeltaSeconds);

			// TODO: The shit here
		}
	}

	// TODO: Call UpdateVelocity Event

	// TODO: Choose better names if we wanna refer to a moving platform as a base
	if (BaseVelocity.Size() < MinVelocityMagnitude)
	{
		BaseVelocity = FVector::ZeroVector;
	}

	// Calculate character movement from velocity
	if (BaseVelocity.Size() > 0.f)
	{
		if (bSolveMovementCollisions)
		{
			InternalCharacterMove(BaseVelocity, DeltaTime);
		}
		else
		{
			TransientPosition += BaseVelocity * DeltaTime;
		}
	}

	if (bPhysicsInteractions)
	{
		// TODO: Okay This Makes Sense, Imparting Velocity After Its Been Calculated And Us Moved
		// Impart force onto them based on our movement velocity
	}

	// This is using from daddy UPawnMovement
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
	// TODO: Call Event
}


#pragma endregion Solver Updates


#pragma region Evaluations

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

	FVector atCharUp = atCharRotation * CachedWorldUp;
	FVector InnerHitDirection = FVector::VectorPlaneProject(HitNormal, atCharUp).GetSafeNormal();

	StabilityReport.bIsStable = IsStableOnNormal(HitNormal);

	StabilityReport.bFoundInnerNormal = false;
	StabilityReport.bFoundOuterNormal = false;
	StabilityReport.InnerNormal = HitNormal;
	StabilityReport.OuterNormal = HitNormal;

	if (bLedgeAndDenivelationHandling)
	{
		float LedgeCheckHeight = MinDistanceForLedge;
		if (StepHandling != EStepHandlingMethod::None)
		{
			LedgeCheckHeight = MaxStepHeight;
		}

		bool bIsStableLedgeInner = false;
		bool bIsStableLedgeOuter = false;

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
		// TODO: This
		// I think we ignore everything except static and dynamic. Pawn,PhysicsBody,Vehicle,Destructible are the equivalent of our rigidbodies?
		auto Type = HitCollider->GetCollisionObjectType();
		switch (Type)
		{
		case ECC_WorldStatic: break;
		case ECC_WorldDynamic: break;
		case ECC_Pawn: break;
		case ECC_Visibility: break;
		case ECC_Camera: break;
		case ECC_PhysicsBody: break;
		case ECC_Vehicle: break;
		case ECC_Destructible: break;
		case ECC_EngineTraceChannel1: break;
		case ECC_EngineTraceChannel2: break;
		case ECC_EngineTraceChannel3: break;
		case ECC_EngineTraceChannel4: break;
		case ECC_EngineTraceChannel5: break;
		case ECC_EngineTraceChannel6: break;
		case ECC_GameTraceChannel1: break;
		case ECC_GameTraceChannel2: break;
		case ECC_GameTraceChannel3: break;
		case ECC_GameTraceChannel4: break;
		case ECC_GameTraceChannel5: break;
		case ECC_GameTraceChannel6: break;
		case ECC_GameTraceChannel7: break;
		case ECC_GameTraceChannel8: break;
		case ECC_GameTraceChannel9: break;
		case ECC_GameTraceChannel10: break;
		case ECC_GameTraceChannel11: break;
		case ECC_GameTraceChannel12: break;
		case ECC_GameTraceChannel13: break;
		case ECC_GameTraceChannel14: break;
		case ECC_GameTraceChannel15: break;
		case ECC_GameTraceChannel16: break;
		case ECC_GameTraceChannel17: break;
		case ECC_GameTraceChannel18: break;
		case ECC_OverlapAll_Deprecated: break;
		case ECC_MAX: break;
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
		FVector OverlapNormal = Overlaps[i].Normal;
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
		bool bFoundClosestHit = false;
		FVector ClosestSweepHitPoint, ClosestSweepHitNormal;
		float ClosestSweepHitDistance = 0.f;
		UPrimitiveComponent* ClosestSweepHitCollider;

		if (bCheckMovementInitialOverlaps)
		{
			int NumOverlaps = CollisionOverlaps(
				TmpMovedPosition,
				TransientRotation,
				InternalProbedColliders);

			if (NumOverlaps > 0)
			{
				ClosestSweepHitDistance = 0.f;
				float MostObstructingOverlapNormalDotProduct = 2.f;

				FVector InitialPositionCache = UpdatedComponent->GetComponentLocation();
				FQuat InitialRotationCache = UpdatedComponent->GetComponentQuat();
				
				// Temporarily move capsule then move it back so we can test penetration at TmpMovedPosition
				// TODO: A GREAT CHANCE TO USE THE MOVEMENTSCOPE DEFERRED!!!!!!!!!!!!
				// DOING THE ABOVE MIGHT ALSO MEAN WE DONT EVEN NEED TO GET OVERLAPS OUR SELVES WITH CAPSULE CASTS IT'LL DO IT
				UpdatedComponent->SetWorldLocationAndRotation(TmpMovedPosition, TransientRotation);

				for (int i = 0; i < NumOverlaps; i++)
				{
					FOverlapResult TmpCollider = InternalProbedColliders[i];

					// TODO: Wait think about this some more, the capsule isnt at TmpPosition, we just wanna test at that position after our initial projections
					// TODO: Can maybe use MoveUpdated Component
					// TODO: For now just gonna set capsule position then set it back just for safety
					FMTDResult PenetrationResult;
					FVector OverlapPosition = TmpCollider.GetComponent()->GetComponentLocation();
					FQuat OverlapRotation = TmpCollider.GetComponent()->GetComponentQuat();
					
					if (Capsule->ComputePenetration(PenetrationResult, TmpCollider.GetComponent()->GetCollisionShape(), OverlapPosition, OverlapRotation))
					{
						const float DotProduct = RemainingMovementDirection | PenetrationResult.Direction;
						if (DotProduct < 0.f && DotProduct < MostObstructingOverlapNormalDotProduct)
						{
							MostObstructingOverlapNormalDotProduct = DotProduct;

							ClosestSweepHitNormal = PenetrationResult.Direction;
							ClosestSweepHitCollider = TmpCollider.GetComponent();
							ClosestSweepHitPoint = TmpMovedPosition + (TransientRotation * TransformToCapsuleCenter) + PenetrationResult.Distance * PenetrationResult.Direction;

							bFoundClosestHit = true;
						}
					}
				}

				// Move back after penetrations were computed
				UpdatedComponent->SetWorldLocationAndRotation(InitialPositionCache, InitialRotationCache);
			}
		}

		FHitResult ClosestSweepHit;
		if (!bFoundClosestHit && CollisionSweeps(
			TmpMovedPosition, // Position
			TransientRotation, // ROtation
			RemainingMovementDirection, // Direction
			RemainingMovementMagnitude + CollisionOffset, // Distance
			ClosestSweepHit, // Closest Hit
			InternalCharacterHits) > 0) // All Hits
		{
			ClosestSweepHitNormal = ClosestSweepHit.ImpactNormal;
			ClosestSweepHitDistance = ClosestSweepHit.Distance;
			ClosestSweepHitCollider = ClosestSweepHit.GetComponent();
			ClosestSweepHitPoint = ClosestSweepHit.ImpactPoint;

			bFoundClosestHit = true;
		}

		if (bFoundClosestHit)
		{
			// Calculate Movement From This Iteration
			FVector SweepMovement = (RemainingMovementDirection * (FMath::Max(0.f, ClosestSweepHitDistance - CollisionOffset)));
			TmpMovedPosition += SweepMovement;
			RemainingMovementMagnitude -= SweepMovement.Size();

			// Evaluate If Hit Is Stable
			FHitStabilityReport MoveHitStabilityReport;
			EvaluateHitStability(ClosestSweepHitCollider, ClosestSweepHitNormal, ClosestSweepHitPoint, TmpMovedPosition, TransientRotation, TransientVelocity, MoveHitStabilityReport);

			// Handle Stepping Up Steps Points Higher Than Bottom Capsule Radius
			bool bFoundValidStepHit = false;
			if (bSolveGrounding && StepHandling != EStepHandlingMethod::None && MoveHitStabilityReport.bValidStepDetected)
			{
				float ObstructionCorrelation = FMath::Abs(ClosestSweepHitNormal | CharacterUp);
				if (ObstructionCorrelation <= CorrelationForVerticalObstruction)
				{
					FVector StepForwardDirection = FVector::VectorPlaneProject(-ClosestSweepHitNormal, CharacterUp).GetSafeNormal();
					FVector StepCastStartPoint = (TmpMovedPosition + (StepForwardDirection * SteppingForwardDistance)) + (CharacterUp * MaxStepHeight);

					// Cast Downward From The Top Of The Stepping Height
					FHitResult ClosestStepHit;
					int NumStepHits = CollisionSweeps(
						StepCastStartPoint, // Position
						TransientRotation, // Rotation
						-CharacterUp, // Direction
						MaxStepHeight, // Distance
						ClosestStepHit, // Closest hit
						InternalCharacterHits,
						0.f,
						true);

					// Check For Hit Corresponding To Stepped Collider
					for (int i = 0; i < NumStepHits; i++)
					{
						// TODO: THIS IS WRONG, NOT CHECKING FOR EQUALITY PROPERLY HERE
						if (InternalCharacterHits[i].GetComponent() == MoveHitStabilityReport.SteppedCollider)
						{
							FVector EndStepPosition = StepCastStartPoint + (-CharacterUp * (InternalCharacterHits[i].Distance - CollisionOffset));
							TmpMovedPosition = EndStepPosition;
							bFoundValidStepHit = true;

							// Project Velocity On Ground Normal At Step
							TransientVelocity = FVector::VectorPlaneProject(TransientVelocity, CharacterUp);
							RemainingMovementDirection = TransientVelocity.GetSafeNormal();

							break;
						}
					}
				}
			}

			// Handle Movement Solving If Collision Wasn't Valid Step
			if (!bFoundValidStepHit)
			{
				FVector ObstructionNormal = GetObstructionNormal(ClosestSweepHitNormal, MoveHitStabilityReport.bIsStable);

				// TODO: Movement Hit Callback EVENT

				// Handle remembering physics interaction hits
				// TODO STORES BUMPED PAWNS/RIGIDBODIES/PHYSICSOBJECTS

				bool bStableOnHit = MoveHitStabilityReport.bIsStable && !MustUnground();
				FVector VelocityBeforeProjection = TransientVelocity;

				// Project Velocity For Next Iteration
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
			}
		}
		// Else we hit nothing
		else
		{
			bHitSomethingThisSweepIteration = false;
		}

		// Safety for exceeding max sweeps allowed
		SweepsMade++;
		if (SweepsMade > MaxMovementIterations)
		{
			if (bKillRemainingMovementWhenExceedMovementIterations) RemainingMovementMagnitude = 0;
			if (bKillVelocityWhenExceedMaxMovementIterations) TransientVelocity = FVector::ZeroVector;

			bWasCompleted = false;
		}
	}

	// Move Position For the Remainder Of The Movement
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
				GroundingStatus->bIsStableOnGround && !MustUnground(),
				bFoundCrease,
				CreaseDirection);

			if (bFoundCrease)
			{
				if (GroundingStatus->bIsStableOnGround && !MustUnground())
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


#pragma endregion Velocity & Movement Handlers

#pragma region Step Handling

//TODO: Need to fiure out pointer assignment
void UKinematicMovementComponent::DetectSteps(FVector CharPosition, FQuat CharRotation, FVector HitPoint, FVector InnerHitDirection, FHitStabilityReport& StabilityReport)
{
	int NumStepHits = 0;
	UPrimitiveComponent* TmpCollider;
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
	if (CheckStepValidity(NumStepHits, CharPosition, CharRotation, InnerHitDirection, StepCheckStartPos, TmpCollider))
	{
		StabilityReport.bValidStepDetected = true;
		StabilityReport.SteppedCollider = TmpCollider;
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

		if (CheckStepValidity(NumStepHits, CharPosition, CharRotation, InnerHitDirection, StepCheckStartPos, TmpCollider))
		{
			StabilityReport.bValidStepDetected = true;
			StabilityReport.SteppedCollider = TmpCollider;
		}
	}
}

// TODO: Check for when to use normal, impact normal or location, impact point. Also best way to store InternalCharacterHits and all that
bool UKinematicMovementComponent::CheckStepValidity(int NumStepHits, FVector CharPosition, FQuat CharRotation, FVector InnerHitDirection, FVector StepCheckStartPost, UPrimitiveComponent* HitCollider)
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
							*HitCollider = *(FarthestHit.GetComponent());
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
	if (!InternalIsColliderValidForCollisions(Collider)) return false;

	return true;
}

bool UKinematicMovementComponent::InternalIsColliderValidForCollisions(UPrimitiveComponent* Collider)
{
	// There's rigidbody stuff here that I'm not quite sure how to deal with, for the most part otherwise these checks dont do much
	// and might fall into the physics interactions category

	// TODO: There's a way to check the object type of an actor or something, whether it is static, dynamic, movable. I think movable could be what we're looking for,
	// or maybe not
	
	return true;
}



#pragma endregion Collision Validity


#pragma region Collision Checks

// TODO: Add Ignore Actor (Self) In FCollisionQueryParams. Then In CheckIfColliderValidForCollisions we can skip the first check

int UKinematicMovementComponent::CollisionOverlaps(FVector Position, FQuat Rotation, TArray<FOverlapResult>& OverlappedColliders, float Inflate, bool AcceptOnlyStableGroundLayer)
{
	const UWorld* World = GetWorld();
	const FCollisionObjectQueryParams ObjectQueryParams = SetupObjectQueryParams({CollidableLayers, StableGroundLayers});

	const FCollisionShape SweepShape = Capsule->GetCollisionShape(Inflate);

	int NumHits = 0;
	World->OverlapMultiByObjectType(OverlappedColliders,
		Position,
		Rotation,
		ObjectQueryParams,
		SweepShape,
		CachedQueryParams);

	int NumUnfilteredHits = OverlappedColliders.Num();
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
	}

	return NumHits;
}


// TODO: NOTE this is actually not used [Actually, It us used but not by it here, but rather is a manual query (e.g for when wanting to uncrouch and checking if the area we're in can be uncrounched from
// I think this is why these also use the QueryTriggerInteraction (unity specific) because it lets us define our own behavior for the overlap somewhat or something
int UKinematicMovementComponent::CharacterOverlaps(FVector Position, FQuat Rotation, TArray<FOverlapResult>& OverlappedColliders, ECollisionChannel TraceChannel, FCollisionResponseParams ResponseParams, float Inflate)
{
	UWorld* World = GetWorld();
	
	FCollisionShape SweepShape = Capsule->GetCollisionShape(Inflate);

	// Filter Out Self
	FCollisionQueryParams CollisionQueryParams = FCollisionQueryParams::DefaultQueryParam;
	CollisionQueryParams.AddIgnoredActor(GetOwner());

	int NumHits = 0;
	World->OverlapMultiByChannel(OverlappedColliders,
		Position,
		Rotation,
		TraceChannel,
		SweepShape,
		CollisionQueryParams,
		ResponseParams);

	return OverlappedColliders.Num();
	
}


int UKinematicMovementComponent::CollisionSweeps(FVector Position, FQuat Rotation, FVector Direction, float Distance, FHitResult& ClosestHit, TArray<FHitResult>& Hits, float Inflate, bool AcceptOnlyStableGroundLayer)
{
	UWorld* World = GetWorld();
	const FCollisionShape SweepShape = Capsule->GetCollisionShape(Inflate);
	
	FCollisionObjectQueryParams ObjectQueryParams = SetupObjectQueryParams(CollidableLayers);
	
	// Capsule Cast
	int NumHits = 0;
	World->SweepMultiByObjectType(Hits,
		Position - Direction * GroundProbingBackstepDistance,
		Position + Direction * (Distance + SweepProbingBackstepDistance - GroundProbingBackstepDistance),
		Rotation,
		ObjectQueryParams,
		SweepShape,
		CachedQueryParams);

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

	return NumHits;
}


// TODO: NOTE This is actually not used
int UKinematicMovementComponent::CharacterSweeps(FVector Position, FQuat Rotation, FVector Direction, float Distance, FHitResult& ClosestHit, TArray<FHitResult>& Hits, ECollisionChannel TraceChannel, FCollisionResponseParams ResponseParams, float Inflate)
{
	UWorld* World = GetWorld();
	// Setup Sweep Collision Shape

	FCollisionShape SweepShape = Capsule->GetCollisionShape(Inflate);


	
	// Capsule Cast
	int NumHits = 0;
	World->SweepMultiByChannel(Hits,
		Position ,
		Position + Direction * Distance,
		Rotation,
		TraceChannel,
		SweepShape,
		FCollisionQueryParams::DefaultQueryParam,
		ResponseParams);

	int NumUnfilteredHits = Hits.Num();

	float ClosestDistance = INFINITY;
	NumHits = NumUnfilteredHits;
	
	for (int i = 0; i < NumUnfilteredHits; i++)
	{
		FHitResult Hit = Hits[i];
		float HitDistance = Hit.Distance;

		// Filter out the character capsule
		if (HitDistance <= 0.f || Hit.GetComponent()->IsA(UCapsuleComponent::StaticClass()))
		{
			NumHits--;
			if (i < NumHits)
			{
				Hits[i] = Hits[NumHits];
			}
		}
		else
		{
			// Remember closest valid hit
			if (HitDistance < ClosestDistance)
			{
				ClosestHit = Hit;
				ClosestDistance = HitDistance;
			}
		}
	}

	return NumHits;
}


// TODO: More Research On Sweep Capsule Orientations!
bool UKinematicMovementComponent::GroundSweep(FVector Position, FQuat Rotation, FVector Direction, float Distance, FHitResult& ClosestHit)
{
	UWorld* World = GetWorld();

	// Setup Sweep Collision Shape
	const FCollisionShape SweepShape = Capsule->GetCollisionShape();
	
	FVector CapsuleBot = Position + (Rotation * TransformToCapsuleBottomHemi) - (Direction * GroundProbingBackstepDistance);
	FVector CapsuleTop = Position + (Rotation * TransformToCapsuleTopHemi) - (Direction * GroundProbingBackstepDistance);

	// Other attempt is to sweep by two spheres since we already know their positions given the rotation
	FCollisionShape TopSphereShape = FCollisionShape::MakeSphere(CapsuleRadius);
	FCollisionShape BotSphereShape = FCollisionShape::MakeSphere(CapsuleRadius);

	// This is wrong, collidablelayers and stablegroundlayers are in and of themselves, lists (they should be I mean). Can setup it up better methinks, check this later
	// and see if you can make an array of channels 
	FCollisionObjectQueryParams ObjectQueryParams = SetupObjectQueryParams(CollidableLayers);

	World->SweepMultiByObjectType(InternalCharacterHits,
		Position - (Direction * GroundProbingBackstepDistance),
		Position + Direction * (Distance - GroundProbingBackstepDistance),
		Rotation,
		ObjectQueryParams,
		SweepShape,
		CachedQueryParams);
	
	int NumUnfilteredHits = InternalCharacterHits.Num();

	bool bFoundValidHit = false;
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

	return bFoundValidHit;
}



int UKinematicMovementComponent::CollisionLineCasts(FVector Position, FVector Direction, float Distance, FHitResult& ClosestHit, TArray<FHitResult>& Hits, bool AcceptOnlyStableGroundLayer)
{
	const UWorld* World = GetWorld();
	const FCollisionObjectQueryParams ObjectQueryParams = SetupObjectQueryParams(CollidableLayers);

	
	int NumHits = 0;
	bool DidHit = World->LineTraceMultiByObjectType(Hits, Position, Position + Direction * Distance, ObjectQueryParams, CachedQueryParams);
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

	return NumHits;
}


#pragma endregion Collision Checks


