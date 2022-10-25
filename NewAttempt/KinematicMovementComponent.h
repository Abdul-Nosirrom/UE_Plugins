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

/// @brief	How to handle stepping
UENUM(BlueprintType)
enum EStepHandlingMethod
{
	None,
	Standard,
	Extra
};

/// @brief  Sweep state during a movement update
enum EMovementSweepState
{
	Initial,
	AfterFirstHit,
	FoundBlockingCrease,
	FoundBlockingCorner
};

#pragma endregion Enums

#pragma region Probing Data

/// @brief  Contains all information for the Motor's grounding status.
struct FGroundingReport
{
public:
	/// @brief  True if pawn is standing on anything in StableGroundLayers regardless of its geometry.
	bool bFoundAnyGround;
	/// @brief  True if pawn is standing on anything in StableGroundLayers that obeys the geometric restrictions set.
	bool bIsStableOnGround;
	/// @brief  True if IsStableWithSpecialCases returns false. Meaning snapping is disabled when exceeded a certain velocity
	///			for ledge snapping, distance from a ledge beyond a certain threshold, or slope angle greater than certain denivelation (Delta) angle.
	///			Will always be false if bLedgeAndDenivelationHandling is not enabled.
	bool bSnappingPrevented;

	FVector GroundNormal;
	FVector InnerGroundNormal;
	FVector OuterGroundNormal;

	/// @brief  Copy over another grounding report into this one. Full copy.
	/// @param  transientGroundingReport Grounding report to copy from
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

/// @brief  Represents the entire state of a pawns motor that is pertinent for simulation.
///			Can be used to save a state or revert to a past state.
struct FMotorState
{
public:
	FVector Position;
	FQuat Rotation;
	FVector Velocity;

	/// @brief  Prevents snapping to ground. Must be manually set to disable ground snapping.
	bool bMustUnground;
	/// @brief  Will force an ungrounding if time > 0.
	float MustUngroundTime;
	bool bLastMovementIterationFoundAnyGround;

	FGroundingReport GroundingStatus;

	/* Note for self, we are ignoring the AttachedRigidBody (Moving Base) because we can get it dynamically from current floor */
};

/// @brief  Describes an overlap between the pawn capsule and another collider, data retrieved from Penetration
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

/// @brief  Contains all the information from a hit stability evaluation
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
	/* UActorComponent Defaults */
	UKinematicMovementComponent();
	void InitializeComponent() override;
	void SetUpdatedComponent(USceneComponent* NewUpdatedComponent);
	void BeginPlay() override;
	void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	/* ======================= */

#pragma region Updates

	void PreSimulationInterpolationUpdate(float DeltaTime);
	void Simulate(float DeltaTime);
	void PostSimulationInterpolationUpdate(float DeltaTime);
	void CustomInterpolationUpdate(float DeltaTime);

	/// @brief  Update phase 1 is meant to be called after physics movers have calculated their velocities, but
	///			before they have simulated their goal positions/rotations. It is responsible for: 
	///			- Initializing all values for update
	///			- Handling MovePosition calls
	///			- Solving initial collision overlaps
	///			- Ground probing
	///			- Handle detecting potential interactable rigidbodies
	/// @param  DeltaTime DeltaTime of the update
	void UpdatePhase1(float DeltaTime);
	
	/// @brief  Update phase 2 is meant to be called after physics movers have simulated their goal positions/rotations. 
	///			At the end of this, the TransientPosition/Rotation values will be up-to-date with where the motor should be at the end of its move. 
	///			It is responsible for:
	///			- Solving Rotation
	///			- Handle MoveRotation calls
	///			- Solving potential attached rigidbody overlaps
	///			- Solving Velocity
	///			- Applying planar constraint
	/// @param  DeltaTime DeltaTime of the update 
	void UpdatePhase2(float DeltaTime);

#pragma endregion Updates

#pragma region Parameters

#pragma region Tick Groups

	FPrePhysicsTickFunction PrePhysicsTick;
	FPostPhysicsTickFunction PostPhysicsTick;

#pragma endregion Tick Groups

#pragma region Collider Component

	/// @brief  PrimitiveComponent of the owner - Restricted to capsules.
	UCapsuleComponent* Capsule;
	/// @brief  Capsule Collider Radius.
	float CapsuleRadius = 0.5f;
	/// @brief  Capsule Collider Total Height.
	float CapsuleHeight = 2.f;
	/// @brief  Capsule Collider Half-Height.
	float CapsuleHalfHeight = 1.f;

#pragma endregion Collider Component 


#pragma region Grounding Settings

	float GroundDetectionExtraDistance = 0.f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	/// @brief  Maximum ground slope angle relative to the actor up direction.
	float MaxStableSlopeAngle = 60.f;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	/// @brief  Ground Layers that the actor is considered stable on/grounded, i.e meets slope limits etc...
	TArray<ECollisionChannel> StableGroundLayers;

	bool bDiscreteCollisionEvents = false;

#pragma endregion Grounding Settings

#pragma region Step Settings

	/// @brief  Handles properly detecting grounding status on step, but has a performance cost.
	EStepHandlingMethod StepHandling = EStepHandlingMethod::Standard;
	/// @brief  Maximum height of a step which the pawn can climb.
	float MaxStepHeight = 0.5f;
	/// @brief  Can the pawn step up obstacles even if it is not currently stable?
	bool bAllowSteppingWithoutStableGrounding = false;
	/// @brief  Minimum length of a step that the character can step on (Used in Extra stepping method. Use this to let the pawn
	///			step on steps that are smaller than it's radius
	float MinRequiredStepDepth = 0.1f;

#pragma endregion Step Settings

#pragma region Ledge Settings

	/// @brief  Flag to enable handling properly detecting ledge information and grounding status. Has a performance cost.
	UPROPERTY(EditDefaultsOnly, Category= "Motor | Ledge Settings")
	bool bLedgeAndDenivelationHandling = true;

	/// @brief  Distance from the capsule central axis at which the character can stand on a ledge and still be stable.
	UPROPERTY(EditDefaultsOnly, Category= "Motor | Ledge Settings", meta=(EditCondition = "bLedgeAndDenivelationHandling", EditConditionHides, ClampMin="0", UIMin="0"))
	float MaxStableDistanceFromLedge = 0.5f;

	/// @brief Prevents snapping to ground on ledges beyond a certain velocity.
	UPROPERTY(EditDefaultsOnly, Category= "Motor | Ledge Settings", meta=(EditCondition = "bLedgeAndDenivelationHandling", EditConditionHides, ClampMin="0", UIMin="0"))
	float MaxVelocityForLedgeSnap = 0.f;

	/// @brief  Maximum downward slope angle DELTA that the actor can be subjected to and still be snapping to the ground
	UPROPERTY(EditDefaultsOnly, Category= "Motor | Ledge Settings", meta=(EditCondition = "bLedgeAndDenivelationHandling", EditConditionHides, ClampMin="0", ClampMax="180", UIMin="0", UIMax="180"))
	float MaxStableDenivelationAngle = 180.f;

#pragma endregion Ledge Settings

#pragma region Physics Interaction Settings
	/*
	 * Storing information about forces to impart when root collision touched (See GMC)
	 * As well as information regarding moving bases and imparting velocity from them (See GMC)
	 * Regarding momentum, might take from the KCC implementation but use GMC as a guide for the Unreal methods.
	 */
#pragma endregion Physics Interaction Settings

#pragma region Motor Settings

	int MaxMovementIterations = 5;
	int MaxDecollisionIterations = 1;
	bool bCheckMovementInitialOverlaps = true;
	bool bKillVelocityWhenExceedMaxMovementIterations = true;
	bool bKillRemainingMovementWhenExceedMovementIterations = true;

#pragma endregion Motor Settings

#pragma region Const
	
	/* CONSTANTS FOR SWEEP AND PHYSICS CALCULATIONS */
	static inline const int MaxHitBudget = 16;
	static inline const int MaxCollisionBudget = 16;
	static inline const int MaxGroundSweepIterations = 2;
	static inline const int MaxSteppingSweepIterations = 3;
	static inline const int MaxRigidBodyOverlapsCount = 16;
	static inline const float CollisionOffset = 0.01f;
	static inline const float GroundProveReboundDistance = 0.02f;
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

	/// @brief  Information about ground in current update.
	FGroundingReport* GroundingStatus = new FGroundingReport();
	/// @brief  Information about ground in previous update.
	FGroundingReport* LastGroundingStatus = new FGroundingReport();

	// TODO: I have concerns about this and how KCC generates it. It's not visible but instead generated automatically from the physics settings
	// Something we can do I think since we have access to the ObjectType we're attached to. There's a chance that this is actually auto-handled
	// when we do traces and sweeps. That is, if we're ObjectTypeA, then those sweeps will conform to the defaults of ObjectTypeA and the settings are for
	// adjustments on top of that.
	
	/// @brief  List of all collision channels we can collider with, including stable ground channels.
	/// @link	https://www.unrealengine.com/en-US/blog/collision-filtering
	TArray<ECollisionChannel> CollidableLayers;

	UPROPERTY(BlueprintReadOnly)
	FTransform Transform;
	UPROPERTY(BlueprintReadOnly)
	FVector CharacterUp;
	UPROPERTY(BlueprintReadOnly)
	FVector CharacterForward;
	UPROPERTY(BlueprintReadOnly)
	FVector CharacterRight;

	/// @brief  Transform from transform position to capsule center.
	FVector TransformToCapsuleCenter;
	/// @brief  Vector from transform position to capsule bottom.
	FVector TransformToCapsuleBottom;
	/// @brief  Vector from transform position to capsule top.
	FVector TransformToCapsuleTop;
	/// @brief  Vector from transform position to capsule bottom hemisphere center.
	FVector TransformToCapsuleBottomHemi;
	/// @brief  Vector from transform position to capsule top hemisphere center.
	FVector TransformToCapsuleTopHemi;

	/// @brief  Remembers initial position before all simulations are done.
	FVector InitialTickPosition;
	/// @brief  Remembers initial rotation before all simulations are done.
	FQuat InitialTickRotation;

	/// @brief  Position When Movement Calculations Begin
	FVector InitialSimulatedPosition;
	/// @brief  Rotation When Movement Calculations Begin
	FQuat InitialSimulatedRotation;

	/// @brief  Goal position in movement calculations.
	///			Always equal to the current position during the update phase.
	FVector TransientPosition;
	/// @brief  Goal rotation in movement calculations.
	///			Always equal to the current position during the update phase.
	FQuat TransientRotation;

	// TODO: Might not need to use the custom class
	/// @brief  Stores overlap information from depentration computation.
	///			UpdatePhase1 - Checks for InternalProbedColliders and stores the colliders
	///			as well as the depenetration direction/normal from the FMTDResult.
	TArray<FCustomOverlapResult> Overlaps;
	/// @brief  Overlap counts to compute depenetration. Reset at the beginning of the update.
	int OverlapsCount;

	/// @brief  Did the last sweep collision detection find any ground?
	bool bLastMovementIterationFoundAnyGround;
	
	/// @brief  Velocity of platform actor is currently standing on. (Equivalent to AttachedRigidBodyVelocity from KCC)
	FVector PlatformVelocity;

#pragma endregion Motor Information

#pragma region Storing Probing Data

	/// @brief  I don't remember.
	TArray<FHitResult> InternalCharacterHits;
	/// @brief  List to keep track of colliders we've overlapped with during sweep check to later compute depenetration.
	TArray<FOverlapResult> InternalProbedColliders;

	/// @brief  If false pawn is essentially in noclip.
	bool bSolveMovementCollisions = true;
	/// @brief  If false pawn is never aware of any ground.
	bool bSolveGrounding = true;

	/// @brief  Internal flag to check if SetPosition has been called externally.
	bool bMovePositionDirty = false;
	/// @brief  Target of SetPosition calls.
	FVector MovePositionTarget = FVector::ZeroVector;
	/// @brief  Internal flag to check if SetRotation has been called externally.
	bool bMoveRotationDirty = false;
	/// @brief  Target of SetRotation calls.
	FQuat MoveRotationTarget = FQuat::Identity;

	bool bLastSolvedOverlapNormalDirty = false;
	FVector LastSolvedOverlapNormal = FVector::ForwardVector;

	/// @brief  True if currently grounded and ForceUnground was called. Disables snapping and allows for leaving ground.
	bool bMustUnground = false;
	/// @brief  Counter to force ungrounding within the set time. Alternative to bMustUnground.
	float MustUngroundTimeCounter = 0.f;

	/* Caching world vectors for convenience */
	FVector CachedWorldUp = FVector::UpVector;
	FVector CachedWorldForward = FVector::ForwardVector;
	FVector CachedWorldRight = FVector::RightVector;
	FVector CachedZeroVector = FVector::ZeroVector;

#pragma endregion Storing Probing Data 

#pragma endregion Parameters

#pragma region Update Evaluations
	
	void EvaluateCrease(FVector CurCharacterVelocity, FVector PrevCharacterVelocity, FVector CurHitNormal, FVector PrevHitNormal, bool bCurrentHitIsStable, bool bPreviousHitIsStable, bool bCharIsStable, bool& bIsValidCrease, FVector& CreaseDirection);
	void EvaluateHitStability(UPrimitiveComponent* HitCollider, FVector HitNormal, FVector HitPoint, FVector atCharPosition, FQuat atCharRotation, FVector withCharVelocity, FHitStabilityReport& StabilityReport);

	bool IsStableOnNormal(FVector Normal);
	bool IsStableWithSpecialCases(FHitStabilityReport& StabilityReport, FVector CharVelocity);

#pragma endregion Update Evaluations

#pragma region Data Handling

	void ValidateData();
	
	void SetCapsuleCollisionsActivation(bool bCollisionsActive);
	void SetMovementCollisionsSolvingActivations(bool bMovementCollisionsSolvingActive);
	void SetGroundingSolvingActivation(bool bStabilitySolvingActive);

#pragma endregion Data Handling

#pragma region Character State Handling

	void ForceUnground(float time = 0.1f);
	bool MustUnground();

	void SetPosition(FVector Position, bool bBypassInterpolation = true);
	void SetTransientPosition(FVector NewPos);
	void SetRotation(FQuat Rotation, bool bBypassInterpolation = true);
	void SetPositionAndRotation(FVector Position, FQuat Rotation, bool bBypassInterpolation);
	void MoveCharacter(FVector ToPosition);
	void RotateCharacter(FQuat ToRotation);

	FMotorState GetState();
	void ApplyState(FMotorState State, bool bBypassInterpolation = true);
	

#pragma endregion Character State Handling

#pragma region Velocity & Movement Handlers

	// Can move these to inlines maybe
	FVector GetVelocityFromMovePosition(FVector FromPosition, FVector ToPosition, float DeltaTime);
	FVector GetVelocityFromMovement(FVector Movement, float DeltaTime);

	bool InternalCharacterMove(FVector& TrnasientVelocity, float DeltaTime);
	void InternalHandleVelocityProjection(bool bStableOnHit, FVector HitNormal, FVector ObstructionNormal, FVector OriginalDistance, EMovementSweepState& SweepState, bool bPreviousHitIsStable,
										FVector PrevVelocity, FVector PrevObstructionNormal, FVector& TransientVelocity, float& RemainingMovementMagnitude, FVector& RemainingMovementDirection);

	// Another thing that may be overriden
	void HandleVelocityProjection(FVector& TransientVelocity, FVector ObstructionNormal, bool bStableOnHit);

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

	int CollisionOverlaps(FVector Position, FQuat Rotation, TArray<FOverlapResult>& OverlappedColliders, float Inflate = 0.f, bool bAcceptOnlyStableGroundLayer = false);
	int CharacterOverlaps(FVector Position, FQuat Rotation, TArray<FOverlapResult>& OverlappedColliders, ECollisionChannel TraceChannel, FCollisionResponseParams ResponseParams, float Inflate = 0.f);

	int CollisionSweeps(FVector Position, FQuat Rotation,  FVector Direction, float Distance, FHitResult& ClosestHit, TArray<FHitResult>& Hits, float Inflate = 0.f, bool bAcceptOnlyStableGroundLayer = false);
	int CharacterSweeps(FVector Position, FQuat Rotation,  FVector Direction, float Distance, FHitResult& ClosestHit, TArray<FHitResult>& Hits, ECollisionChannel TraceChannel, FCollisionResponseParams ResponseParams, float Inflate = 0.f);

	bool GroundSweep(FVector Position, FQuat Rotation, FVector Direction, float Distance, FHitResult& ClosestHit);
	int CollisionLineCasts(FVector Position, FVector Direction, float Distance, FHitResult& ClosestHit, TArray<FHitResult>& Hits, bool bAcceptOnlyStableGroundLayer = false);

	bool HandleDepenetration(UPrimitiveComponent* OverlappedComponent, FVector& ResolutionDirection, float& ResolutionDistance);

#pragma endregion Collision Checks

#pragma region Utility

	// I think this is fine since all it does is tell it to ignore us, object query params does more of the heavy lifting
	FCollisionQueryParams CachedQueryParams;
	
	FVector GetDirectionTangentToSurface(FVector Direction, FVector SurfaceNormal);

	FCollisionQueryParams SetupQueryParams() const;
	FCollisionObjectQueryParams SetupObjectQueryParams(TArray<ECollisionChannel> &CollisionChannels) const;

#pragma endregion Utility


#pragma region Virtual Methods Or BP Events

	UFUNCTION(BlueprintNativeEvent, Category="Movement Controller")
	void UpdateRotation(FQuat& CurrentRotation, float DeltaTime);
	virtual void UpdateRotation_Implementation(FQuat& CurrentRotation, float DeltaTime) {return;};

	UFUNCTION(BlueprintNativeEvent, Category="Movement Controller")
	void UpdateVelocity(FVector& CurrentVelocity, float DeltaTime);
	virtual void UpdateVelocity_Implementation(FVector& CurrentVelocity, float DeltaTime) {return;};

	UFUNCTION(BlueprintNativeEvent, Category="Movement Controller")
	void BeforeCharacterUpdate(float DeltaTime);
	virtual void BeforeCharacterUpdate_Implementation(float DeltaTime) {return;}

	UFUNCTION(BlueprintNativeEvent, Category="Movement Controller")
	void PostGroundingUpdate(float DeltaTime);
	virtual void PostGroundingUpdate_Implementation(float DeltaTime) {return;}

	UFUNCTION(BlueprintNativeEvent, Category="Movement Controller")
	void AfterCharacterUpdate(float DeltaTime);
	virtual void AfterCharacterUpdate_Implementation(float DeltaTime) {return;}

	UFUNCTION(BlueprintNativeEvent, Category="Movement Controller")
	void OnLanded(UPrimitiveComponent* HitCollider, FVector HitNormal, FVector HitPoint, FHitStabilityReport& HitStabilityReport);
	virtual void OnLanded_Implementation(UPrimitiveComponent* HitCollider, FVector HitNormal, FVector HitPoint, FHitStabilityReport& HitStabilityReport) {return;}

#pragma endregion Virtual Methods Or BP Events

};

// Might not want to inline this if we want it to be blueprintable 
FORCEINLINE FVector UKinematicMovementComponent::GetDirectionTangentToSurface(FVector Direction, FVector SurfaceNormal)
{
	return (SurfaceNormal ^ (Direction ^ CharacterUp)).GetSafeNormal();
}

// This is fine however
FORCEINLINE FCollisionQueryParams UKinematicMovementComponent::SetupQueryParams() const
{
	FCollisionQueryParams QueryParams;

	// Ignore Self
	QueryParams.AddIgnoredActor(PawnOwner);

	// I don't know what this does because the documentation is a bitch
	QueryParams.bDebugQuery = true;


	FMaskFilter IgnoreMask; // Specific Channels to ignore? That's below though.

	return QueryParams;
}

// It might not actually inline given the existence of the for loop inside it
FORCEINLINE FCollisionObjectQueryParams UKinematicMovementComponent::SetupObjectQueryParams(TArray<ECollisionChannel> &CollisionChannels) const
{
	FCollisionObjectQueryParams ObjectQueryParams;

	for (auto Channel : CollisionChannels)
	{
		ObjectQueryParams.AddObjectTypesToQuery(Channel);
	}
	
	return ObjectQueryParams;
	
}

