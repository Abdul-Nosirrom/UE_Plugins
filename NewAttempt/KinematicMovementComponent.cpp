// Fill out your copyright notice in the Description page of Project Settings.


#include "KinematicMovementComponent.h"


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


#pragma region Update Evaluations

void UKinematicMovementComponent::EvaluateHitStability(UPrimitiveComponent HitCollider, FVector HitNormal, FVector HitPoint, FVector atCharPosition, FQuat atCharRotation, FVector withCharVelocity, FHitStabilityReport& StabilityReport)
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
		// TODO: This
	}

	if (StepHandling != EStepHandlingMethod::None && !StabilityReport.bIsStable)
	{
		// TODO: This
	}

	// Call event on character controller
}


#pragma endregion Update Evaluations

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
				if (IsStableOnNormal(OuterSlopeHit.Normal))
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
								if (IsStableOnNormal(InnerStepHit.Normal))
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
								if (IsStableOnNormal(InnerStepHit.Normal))
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
	return true;
}



#pragma endregion Collision Validity


#pragma region Collision Checks

// TODO: Add Ignore Actor (Self) In FCollisionQueryParams. Then In CheckIfColliderValidForCollisions we can skip the first check

int UKinematicMovementComponent::CollisionOverlaps(FVector Position, FQuat Rotation, TArray<FOverlapResult> OverlappedColliders, float Inflate, bool AcceptOnlyStableGroundLayer)
{
	UWorld* World = GetWorld();
	ECollisionChannel QueryChannels = CollidableLayers;

	if (AcceptOnlyStableGroundLayer)
	{
		QueryChannels = static_cast<ECollisionChannel>(static_cast<int>(CollidableLayers) & static_cast<int>(StableGroundLayers));
	}

	float HalfHeight = FMath::IsNearlyZero(Inflate) ? CapsuleHeight : CapsuleHeight + Inflate;
	float Radius = FMath::IsNearlyZero(Inflate) ? CapsuleRadius : CapsuleRadius + Inflate;
	
	FCollisionShape SweepShape = FCollisionShape::MakeCapsule(Radius, HalfHeight);

	int NumHits = 0;
	World->OverlapMultiByChannel(OverlappedColliders,
		Position,
		Rotation,
		QueryChannels,
		SweepShape);

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


int UKinematicMovementComponent::CharacterOverlaps(FVector Position, FQuat Rotation, TArray<FOverlapResult> OverlappedColliders, ECollisionChannel TraceChannel, FCollisionResponseParams ResponseParams, float Inflate)
{
	UWorld* World = GetWorld();
	
	float HalfHeight = FMath::IsNearlyZero(Inflate) ? CapsuleHeight : CapsuleHeight + Inflate;
	float Radius = FMath::IsNearlyZero(Inflate) ? CapsuleRadius : CapsuleRadius + Inflate;
	
	FCollisionShape SweepShape = FCollisionShape::MakeCapsule(Radius, HalfHeight);

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


int UKinematicMovementComponent::CollisionSweeps(FVector Position, FQuat Rotation, FVector Direction, float Distance, FHitResult& ClosestHit, TArray<FHitResult> Hits, float Inflate, bool AcceptOnlyStableGroundLayer)
{
	UWorld* World = GetWorld();
	ECollisionChannel QueryChannels = CollidableLayers;

	if (AcceptOnlyStableGroundLayer)
	{
		QueryChannels = static_cast<ECollisionChannel>(static_cast<int>(CollidableLayers) & static_cast<int>(StableGroundLayers));
	}

	float HalfHeight = FMath::IsNearlyZero(Inflate) ? CapsuleHeight : CapsuleHeight + Inflate;
	float Radius = FMath::IsNearlyZero(Inflate) ? CapsuleRadius : CapsuleRadius + Inflate;
	
	FCollisionShape SweepShape = FCollisionShape::MakeCapsule(Radius, HalfHeight);

	FCollisionQueryParams CollisionQueryParams = FCollisionQueryParams::DefaultQueryParam;
	
	// Capsule Cast
	int NumHits = 0;
	World->SweepMultiByChannel(Hits,
		Position - (Direction * GroundProbingBackstepDistance),
		Position + Direction * (Distance + SweepProbingBackstepDistance - GroundProbingBackstepDistance),
		Rotation,
		QueryChannels,
		SweepShape);

	int NumUnfilteredHits = Hits.Num();

	float ClosestDistance = INFINITY;
	NumHits = NumUnfilteredHits;

	for (int i = NumUnfilteredHits - 1; i >= 0; i--)
	{
		Hits[i].Distance -= SweepProbingBackstepDistance;

		FHitResult Hit = Hits[i];
		float HitDistance = Hit.Distance;

		// Filter out invalid hits
		if (HitDistance <= 0.f || !CheckIfColliderValidForCollisions(Hit.GetComponent()))
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


int UKinematicMovementComponent::CharacterSweeps(FVector Position, FQuat Rotation, FVector Direction, float Distance, FHitResult& ClosestHit, TArray<FHitResult> Hits, ECollisionChannel TraceChannel, FCollisionResponseParams ResponseParams, float Inflate)
{
	UWorld* World = GetWorld();
	// Setup Sweep Collision Shape

	float HalfHeight = FMath::IsNearlyZero(Inflate) ? CapsuleHeight : CapsuleHeight + Inflate;
	float Radius = FMath::IsNearlyZero(Inflate) ? CapsuleRadius : CapsuleRadius + Inflate;
	
	FCollisionShape SweepShape = FCollisionShape::MakeCapsule(Radius, HalfHeight);

	
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
	FCollisionShape SweepShape = FCollisionShape::MakeCapsule(CapsuleRadius, CapsuleHeight);
	
	FVector CapsuleBot = Position + (Rotation * TransformToCapsuleBottomHemi) - (Direction * GroundProbingBackstepDistance);
	FVector CapsuleTop = Position + (Rotation * TransformToCapsuleTopHemi) - (Direction * GroundProbingBackstepDistance);

	// Other attempt is to sweep by two spheres since we already know their positions given the rotation
	FCollisionShape TopSphereShape = FCollisionShape::MakeSphere(CapsuleRadius);
	FCollisionShape BotSphereShape = FCollisionShape::MakeSphere(CapsuleRadius);
	
	// Capsule Cast
	World->SweepMultiByChannel(InternalCharacterHits,
		Position - (Direction * GroundProbingBackstepDistance),
		Position + Direction * (Distance - GroundProbingBackstepDistance),
		Rotation,
		static_cast<ECollisionChannel>(static_cast<int>(CollidableLayers) & static_cast<int>(StableGroundLayers)),
		SweepShape);

	int NumUnfilteredHits = InternalCharacterHits.Num();

	bool bFoundValidHit = false;
	float ClosestDistance = INFINITY;

	for (int i = 0; i < NumUnfilteredHits; i++)
	{
		FHitResult Hit = InternalCharacterHits[i];
		float HitDistance = Hit.Distance;

		// Find closest valid hit
		if (HitDistance > 0.f && CheckIfColliderValidForCollisions(Hit.GetComponent()))
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



int UKinematicMovementComponent::CollisionLineCasts(FVector Position, FVector Direction, float Distance, FHitResult& ClosestHit, TArray<FHitResult> Hits, bool AcceptOnlyStableGroundLayer)
{
	UWorld* World = GetWorld();
	ECollisionChannel queryChannels = CollidableLayers;

	if (AcceptOnlyStableGroundLayer)
	{
		queryChannels = static_cast<ECollisionChannel>(static_cast<int>(CollidableLayers) & static_cast<int>(StableGroundLayers));
	}
	
	int NumHits = 0;
	bool DidHit = World->LineTraceMultiByChannel(Hits, Position, Position + Direction, queryChannels);
	const int NumUnfilteredHits = Hits.Num();

	float ClosestDistance = INFINITY;
	NumHits = NumUnfilteredHits;

	for (int i = NumUnfilteredHits - 1; i >= 0; i--)
	{
		FHitResult Hit = Hits[i];
		float HitDistance = Hit.Distance;

		if (HitDistance <= 0 || !CheckIfColliderValidForCollisions(Hit.GetComponent()))
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

