// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/MovementComponent.h"
#include "GameFramework/PawnMovementComponent.h"
#include "KinematicMovementComponent.generated.h"

#pragma region Tick Group Structs

struct FPrePhysicsTickFunction : public FTickFunction
{
	class UKinematicMovementComponent* Target;
	
	virtual void ExecuteTick(float DeltaTime, ELevelTick TickType, ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent) override;
};

struct FPostPhysicsTickFunction : public FTickFunction
{
	class UKinematicMovementComponent* Target;

	virtual void ExecuteTick(float DeltaTime, ELevelTick TickType, ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent) override;
};

#pragma endregion Tick Group Structs 

#pragma region Enums

UENUM(BlueprintType)
enum EStepHandlingMethod
{
	None,
	Standard,
	Extra
};

enum EMovementSweepState
{
	Initial,
	AfterFirstHit,
	FoundBlockingCrease,
	FoundBlockingCorner
};

#pragma endregion Enums

#pragma region Probing Data

struct FGroundingReport
{
public:
	bool bFoundAnyGround;
	bool bIsStableOnGround;
	bool bSnappingPrevented;

	FVector GroundNormal;
	FVector InnerGroundNormal;
	FVector OuterGroundNormal;

	void CopyFrom(FGroundingReport transientGroundingReport)
	{
		bFoundAnyGround = transientGroundingReport.bFoundAnyGround;
		bIsStableOnGround = transientGroundingReport.bIsStableOnGround;
		bSnappingPrevented = transientGroundingReport.bSnappingPrevented;

		GroundNormal = transientGroundingReport.GroundNormal;
		InnerGroundNormal = transientGroundingReport.InnerGroundNormal;
		OuterGroundNormal = transientGroundingReport.OuterGroundNormal;
	}
};

struct FMotorState
{
public:
	FVector Position;
	FQuat Rotation;
	FVector Velocity;

	bool bMustUnground;
	float MustUngroundTime;
	bool bLastMovementIterationFoundAnyGround;

	FGroundingReport GroundingStatus;
};

struct FCustomOverlapResult
{
public:
	FVector Normal;
	UPrimitiveComponent* Collider;

	FCustomOverlapResult(FVector NewNormal, UPrimitiveComponent* NewCollider)
	{
		Normal = NewNormal;
		Collider = NewCollider;
	}
};

struct FHitStabilityReport
{
public:
	bool bIsStable;

	bool bFoundInnerNormal;
	FVector InnerNormal;

	bool bFoundOuterNormal;
	FVector OuterNormal;

	bool bValidStepDetected;
	UPrimitiveComponent* SteppedCollider;

	bool bLedgeDetected;
	bool bIsOnEmptySideOfLedge;
	float DistanceFromLedge;
	bool bIsMovingTowardsEmptySideOfLedge;

	FVector LedgeGroundNormal;
	FVector LedgeRightDirection;
	FVector LedgeFacingDirection;
};

#pragma endregion Probing Data

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class PROTOTYPING_API UKinematicMovementComponent : public UPawnMovementComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UKinematicMovementComponent();
	void InitializeComponent() override;
	void SetUpdatedComponent(USceneComponent* NewUpdatedComponent);
	void BeginPlay() override;
	void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;


#pragma region Updates

	void PreSimulationInterpolationUpdate(float DeltaTime);
	void Simulate(float DeltaTime);
	void PostSimulationInterpolationUpdate(float DeltaTime);
	void CustomInterpolationUpdate(float DeltaTime);
	
	void UpdatePhase1(float DeltaTime);
	void UpdatePhase2(float DeltaTime);

#pragma endregion Updates

#pragma region Parameters

#pragma region Tick Groups

	FPrePhysicsTickFunction PrePhysicsTick;
	FPostPhysicsTickFunction PostPhysicsTick;

#pragma endregion Tick Groups

#pragma region Collider Component

	UCapsuleComponent* Capsule;
	float CapsuleRadius = 0.5f;
	float CapsuleHeight = 2.f;
	float CapsuleZOffset = 1.f;

#pragma endregion Collider Component 


#pragma region Grounding Settings

	float GroundDetectionExtraDistance = 0.f;
	float MaxStableSlopeAngle = 60.f;
	ECollisionChannel StableGroundLayers;
	bool bDiscreteCollisionEvents = false;

#pragma endregion Grounding Settings

#pragma region Step Settings

	EStepHandlingMethod StepHandling = EStepHandlingMethod::Standard;
	float MaxStepHeight = 0.5f;
	bool bAllowSteppingWithoutStableGrounding = false;
	float MinRequiredStepDepth = 0.1f;

#pragma endregion Step Settings

#pragma region Ledge Settings

	bool bLedgeAndDenivelationHandling = true;
	float MaxStableDistanceFromLedge = 0.5f;
	float MaxVelocityForLedgeSnap = 0.f;
	float MaxStableDenivelationAngle = 180.f;

#pragma endregion Ledge Settings

#pragma region Physics Interaction Settings

#pragma endregion Physics Interaction Settings

#pragma region Motor Settings

	int MaxMovementIterations = 5;
	int MaxDecollisionIterations = 1;
	bool bCheckMovementInitialOverlaps = true;
	bool bKillVelocityWhenExceedMaxMovementIterations = true;
	bool bKillRemainingMovementWhenExceedMovementIterations = true;

#pragma endregion Motor Settings

#pragma region Const

	static inline const int MaxHitsBudget = 16;
	static inline const int MaxCollisionBudget = 16;
	static inline const int MaxGroundingSweepIterations = 2;
	static inline const int MaxSteppingSweepIterations = 3;
	static inline const int MaxRigidbodyOverlapsCount = 16;
	static inline const float CollisionOffset = 0.01f;
	static inline const float GroundProbeReboundDistance = 0.02f;
	static inline const float MinimumGroundProbingDistance = 0.005f;
	static inline const float GroundProbingBackstepDistance = 0.1f;
	static inline const float SweepProbingBackstepDistance = 0.002f;
	static inline const float SecondaryProbesVertical = 0.02f;
	static inline const float SecondaryProbesHorizontal = 0.001f;
	static inline const float MinVelocityMagnitude = 0.01f;
	static inline const float SteppingForwardDistance = 0.03f;
	static inline const float MinDistanceForLedge = 0.05f;
	static inline const float CorrelationForVerticalObstruction = 0.01f;
	static inline const float ExtraSteppingForwardDistance = 0.01f;
	static inline const float ExtraStepHeightPadding = 0.01f;

#pragma endregion const

#pragma region Motor Information

	FGroundingReport* GroundingStatus = new FGroundingReport();
	FGroundingReport* LastGroundingStatus = new FGroundingReport();

	ECollisionChannel CollidableLayers;

	FTransform Transform;
	FVector CharacterUp;
	FVector CharacterForward;
	FVector CharacterRight;

	FVector TransformToCapsuleCenter;
	FVector TransformToCapsuleBottom;
	FVector TransformToCapsuleTop;
	FVector TransformToCapsuleBottomHemi;
	FVector TransformToCapsuleTopHemi;

	FVector InitialTickPosition;
	FQuat InitialTickRotation;
	FVector InitialSimulatedPosition;
	FQuat InitialSimulatedRotation;

	FVector TransientPosition;
	FQuat TransientRotation;

	// TODO: Might not need to use the custom class
	TArray<FCustomOverlapResult> Overlaps;
	int OverlapsCount;

	bool bLastMovementIterationFoundAnyGround;

	FVector BaseVelocity;
	FVector Velocity;

#pragma endregion Motor Information

#pragma region Storing Probing Data

	TArray<FHitResult> InternalCharacterHits;
	TArray<FOverlapResult> InternalProbedColliders;

	bool bSolveMovementCollisions = true;
	bool bSolveGrounding = true;
	
	bool bMovePositionDirty = false;
	FVector MovePositionTarget = FVector::ZeroVector;
	bool bMoveRotationDirty = false;
	FQuat MoveRotationTarget = FQuat::Identity;

	bool bLastSolvedOverlapNormalDirty = false;
	FVector LastSolvedOverlapNormal = FVector::ForwardVector;

	bool bMustUnground = false;
	float MustUngroundTimeCounter = 0.f;

	FVector CachedWorldUp = FVector::UpVector;
	FVector CachedWorldForward = FVector::ForwardVector;
	FVector CachedWorldRight = FVector::RightVector;
	FVector CachedZeroVector = FVector::ZeroVector;

#pragma endregion Storing Probing Data 

#pragma endregion Parameters

#pragma region Update Evaluations
	
	void EvaluateCrease(FVector CurCharacterVelocity, FVector PrevCharacterVelocity, FVector CurHitNormal, FVector PrevHitNormal, bool CurrentHitIsStable, bool PreviousHitIsStable, bool CharIsStable, bool& IsValidCrease, FVector& CreaseDirection);
	void EvaluateHitStability(UPrimitiveComponent HitCollider, FVector HitNormal, FVector HitPoint, FVector atCharPosition, FQuat atCharRotation, FVector withCharVelocity, FHitStabilityReport& StabilityReport);

	bool IsStableOnNormal(FVector Normal);
	bool IsStableWithSpecialCases(FHitStabilityReport& StabilityReport, FVector CharVelocity);

#pragma endregion Update Evaluations

#pragma region Data Handling

	void ValidateData();
	
	void SetCapsuleCollisionsActivation(bool CollisionsActive);
	void SetMovementCollisionsSolvingActivations(bool MovementCollisionsSolvingActive);
	void SetGroundingSolvingActivation(bool StabilitySolvingActive);

#pragma endregion Data Handling

#pragma region Character State Handling

	void ForceUnground(float time = 0.1f);
	bool MustUnground();

	void SetPosition(FVector Position, bool BypassInterpolation = true);
	void SetTransientPosition(FVector NewPos);
	void SetRotation(FQuat Rotation, bool BypassInterpolation = true);
	void SetPositionAndRotation(FVector Position, FQuat Rotation, bool BypassInterpolation);
	void MoveCharacter(FVector ToPosition);
	void RotateCharacter(FQuat ToRotation);

	FMotorState GetState();
	void ApplyState(FMotorState State, bool BypassInterpolation = true);
	

#pragma endregion Character State Handling

#pragma region Velocity & Movement Handlers

	FVector GetVelocityFromMovePosition(FVector FromPosition, FVector ToPosition, float DeltaTime);
	FVector GetVelocityFromMovement(FVector Movement, float DeltaTime);

	bool InternalCharacterMove(FVector& TrnasientVelocity, float DeltaTime);
	void InternalHandleVelocityProjection(bool StableOnHit, FVector HitNormal, FVector ObstructionNormal, FVector OriginalDistance, EMovementSweepState& SweepState, bool PreviousHitIsStable,
										FVector PrevVelocity, FVector PrevObstructionNormal, FVector& TransientVelocity, float& RemainingMovementMagnitude, FVector& RemainingMovementDirection);

#pragma endregion Velocity & Movement Handlers

#pragma region Detection

	void DetectSteps(FVector CharPosition, FQuat CharRotation, FVector HitPoint, FVector InnerHitDirection, FHitStabilityReport& StabilityReport);
	bool CheckStepValidity(int numStepHits, FVector CharPosition, FQuat CharRotation, FVector InnerHitDirection, FVector StepCheckStartPost, UPrimitiveComponent* HitCollider);

#pragma endregion Detection

#pragma region Collision Validity
	
	bool CheckIfColliderValidForCollisions(UPrimitiveComponent* Collider);
	bool InternalIsColliderValidForCollisions(UPrimitiveComponent* Collider);

#pragma endregion Collision Validity


#pragma region Collision Checks

	int CollisionOverlaps(FVector Position, FQuat Rotation, TArray<FOverlapResult> OverlappedColliders, float Inflate = 0.f, bool AcceptOnlyStableGroundLayer = false);
	int CharacterOverlaps(FVector Position, FQuat Rotation, TArray<FOverlapResult> OverlappedColliders, ECollisionChannel TraceChannel, FCollisionResponseParams ResponseParams, float Inflate = 0.f);

	int CollisionSweeps(FVector Position, FQuat Rotation,  FVector Direction, float Distance, FHitResult& ClosestHit, TArray<FHitResult> Hits, float Inflate = 0.f, bool AcceptOnlyStableGroundLayer = false);
	int CharacterSweeps(FVector Position, FQuat Rotation,  FVector Direction, float Distance, FHitResult& ClosestHit, TArray<FHitResult> Hits, ECollisionChannel TraceChannel, FCollisionResponseParams ResponseParams, float Inflate = 0.f);

	bool GroundSweep(FVector Position, FQuat Rotation, FVector Direction, float Distance, FHitResult& ClosestHit);
	int CollisionLineCasts(FVector Position, FVector Direction, float Distance, FHitResult& ClosestHit, TArray<FHitResult> Hits, bool AcceptOnlyStableGroundLayer = false);


#pragma endregion Collision Checks

#pragma region Utility

	FVector GetDirectionTangentToSurface(FVector Direction, FVector SurfaceNormal);

#pragma endregion Utility

};
