// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PawnMovementComponent.h"

#include "CustomMovementComponent.generated.h"


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

#pragma region Stability Data Structs

/// @brief  Contains all information for the Motor's grounding status.
USTRUCT(BlueprintType)
struct FGroundingReport
{
	GENERATED_BODY()
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

	FHitResult GroundHit;
	FVector GroundPoint;

	/// @brief  Copy over another grounding report into this one. Full copy.
	/// @param  SourceGroundingReport Grounding report to copy from
	void CopyFrom(FGroundingReport SourceGroundingReport)
	{
		bFoundAnyGround = SourceGroundingReport.bFoundAnyGround;
		bIsStableOnGround = SourceGroundingReport.bIsStableOnGround;
		bSnappingPrevented = SourceGroundingReport.bSnappingPrevented;

		GroundNormal = SourceGroundingReport.GroundNormal;
		InnerGroundNormal = SourceGroundingReport.InnerGroundNormal;
		OuterGroundNormal = SourceGroundingReport.OuterGroundNormal;
	}
};

/// @brief  Represents the entire state of a pawns motor that is pertinent for simulation.
///			Can be used to save a state or revert to a past state.
USTRUCT(BlueprintType)
struct FMotorState
{
	GENERATED_BODY()
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

/// @brief  Contains all the information from a hit stability evaluation
USTRUCT(BlueprintType)
struct FHitStabilityReport
{
	GENERATED_BODY()
public:
	bool bIsStable;

	bool bFoundInnerNormal;
	FVector InnerNormal;

	bool bFoundOuterNormal;
	FVector OuterNormal;

	bool bValidStepDetected;
	FHitResult SteppedCollider;

	/* Ledge Data */
	/// @brief  True if an bFoundInnerNormal != bFoundOuterNormal
	bool bLedgeDetected;
	/// @brief  True if bFoundOuterNormal = true && bFoundInnerNormal = false 
	bool bIsOnEmptySideOfLedge;
	bool bIsMovingTowardsEmptySideOfLedge;
	float DistanceFromLedge;

	FVector LedgeGroundNormal;
	FVector LedgeRightDirection;
	FVector LedgeFacingDirection;
};

#pragma endregion Stability Data Structs 

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class PROTOTYPING_API UCustomMovementComponent : public UPawnMovementComponent
{
	GENERATED_BODY()

#pragma region Core Movement Parameters

protected:
#pragma region Ground Stability Settings
	
	/// @brief  Extra probing distance for ground detection.
	UPROPERTY(EditDefaultsOnly, Category= "Motor | Ground Settings")
	float GroundDetectionExtraDistance = 0.f;
	
	/// @brief  Maximum ground slope angle relative to the actor up direction.
	UPROPERTY(EditDefaultsOnly, Category= "Motor | Ground Settings")
	float MaxStableSlopeAngle = 60.f;

	UPROPERTY(BlueprintReadOnly, Category="Motor | Ground Status")
	FGroundingReport GroundingStatus = FGroundingReport();
	UPROPERTY(BlueprintReadOnly, Category="Motor | Ground Status")
	FGroundingReport LastGroundingStatus = FGroundingReport();

	UPROPERTY(BlueprintReadOnly, Category="Motor | Ground Status")
	bool bLastMovementIterationFoundAnyGround;

#pragma endregion Ground Stability Settings

#pragma region Step Settings

	/// @brief  Handles properly detecting grounding status on step, but has a performance cost.
	UPROPERTY(EditDefaultsOnly, Category= "Motor | Step Settings")
	TEnumAsByte<EStepHandlingMethod> StepHandling = EStepHandlingMethod::Standard;
	
	/// @brief  Maximum height of a step which the pawn can climb.
	UPROPERTY(EditDefaultsOnly, Category= "Motor | Step Settings")
	float MaxStepHeight = 50.f;
	
	/// @brief  Can the pawn step up obstacles even if it is not currently on stable ground (e.g from air)?
	UPROPERTY(EditDefaultsOnly, Category= "Motor | Step Settings")
	bool bAllowSteppingWithoutStableGrounding = false;

	/// @brief  Minimum length of a step that the character can step on (Used in Extra stepping method. Use this to let the pawn
	///			step on steps that are smaller than it's radius
	UPROPERTY(EditDefaultsOnly, Category= "Motor | Step Settings", meta=(EditCondition = "StepHandlingMethod == EStephandlingMethod::Extra", EditConditionHides))
	float MinRequiredStepDepth = 10.f;

#pragma endregion Step Settings

#pragma region Ledge Settings
	
	/// @brief  Flag to enable handling properly detecting ledge information and grounding status. Has a performance cost.
	UPROPERTY(EditDefaultsOnly, Category= "Motor | Ledge Settings")
	bool bLedgeAndDenivelationHandling = true;

	/// @brief  Distance from the capsule central axis at which the character can stand on a ledge and still be stable.
	UPROPERTY(EditDefaultsOnly, Category= "Motor | Ledge Settings", meta=(EditCondition = "bLedgeAndDenivelationHandling", EditConditionHides, ClampMin="0", UIMin="0"))
	float MaxStableDistanceFromLedge = 50.f;

	/// @brief Prevents snapping to ground on ledges beyond a certain velocity.
	UPROPERTY(EditDefaultsOnly, Category= "Motor | Ledge Settings", meta=(EditCondition = "bLedgeAndDenivelationHandling", EditConditionHides, ClampMin="0", UIMin="0"))
	float MaxVelocityForLedgeSnap = 0.f;

	/// @brief  Maximum downward slope angle DELTA that the actor can be subjected to and still be snapping to the ground
	UPROPERTY(EditDefaultsOnly, Category= "Motor | Ledge Settings", meta=(EditCondition = "bLedgeAndDenivelationHandling", EditConditionHides, ClampMin="0", ClampMax="180", UIMin="0", UIMax="180"))
	float MaxStableDenivelationAngle = 180.f;

#pragma endregion Ledge Settings

#pragma region Simulation Parameters

	UPROPERTY(EditDefaultsOnly, Category= "Motor | Simulation Settings", AdvancedDisplay)
	int32 MaxMovementIterations = 5;
	UPROPERTY(EditDefaultsOnly, Category= "Motor | Simulation Settings", AdvancedDisplay)
	int32 MaxDecollisionIterations = 1;
	UPROPERTY(EditDefaultsOnly, Category= "Motor | Simulation Settings", AdvancedDisplay)
	bool bCheckMovementInitialOverlaps = true;
	UPROPERTY(EditDefaultsOnly, Category= "Motor | Simulation Settings", AdvancedDisplay)
	bool bKillVelocityWhenExceedMaxMovementIterations = true;
	UPROPERTY(EditDefaultsOnly, Category= "Motor | Simulation Settings", AdvancedDisplay)
	bool bKillRemainingMovementWhenExceedMovementIterations = true;

#pragma endregion Simulation Parameters

#pragma endregion Core Movement Parameters

#pragma region Const Simulation Parameters

private:
	
	/* CONSTANTS FOR SWEEP AND PHYSICS CALCULATIONS */
	static constexpr int32 MAX_HIT_BUDGET = 16;
	static constexpr int32 MAX_COLLISION_BUDGET = 16;
	static constexpr int32 MAX_GROUND_SWEEP_ITERATIONS = 2;
	static constexpr int32 MAX_STEPPING_SWEEP_ITERATIONS = 3;
	static constexpr int32 MAX_OVERLAPS_COUNT = 16;
	static constexpr float COLLISION_OFFSET = 1.f;
	static constexpr float GROUND_PROBING_REBOUND_DISTANCE = 2.f;
	static constexpr float MINIMUM_GROUND_PROBING_DISTANCE = 0.5f;
	static constexpr float GROUND_PROBING_BACKSTEP_DISTANCE = 10.f;
	static constexpr float SWEEP_PROBING_BACKSTEP_DISTANCE = 0.2f;
	static constexpr float SECONDARY_PROBES_VERTICAL = 2.f;
	static constexpr float SECONDARY_PROBES_HORIZONTAL = 0.1f;
	static constexpr float MIN_VELOCITY_MAGNITUDE = 1.f;
	static constexpr float STEPPING_FORWARD_DISTANCE = 5.f;
	static constexpr float MIN_DISTANCE_FOR_LEDGE = 5.f;
	static constexpr float CORRELATION_FOR_VERTICAL_OBSTRUCTION = 1.f;
	static constexpr float EXTRA_STEPPING_FORWARD_DISTANCE = 1.f;
	static constexpr float EXTRA_STEPPING_HEIGHT_PADDING = 1.f;

#pragma endregion Const Simulation Parameters
	
public:
	// Sets default values for this component's properties
	UCustomMovementComponent();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
	                           FActorComponentTickFunction* ThisTickFunction) override;


/* Overriding methods from parent */
#pragma region Movement Component Overrides

	virtual void SetUpdatedComponent(USceneComponent* NewUpdatedComponent) override;

#pragma endregion Movement Component Overrides

#pragma region Exposed Calls

	void HaltMovement();
	void EnableMovement();
	void DisableMovement();

	/// @brief  Unground and prevent snapping to allow for leaving ground
	void ForceUnground();

	/// @brief  Set a target position to be moved to during the update phase. Movement sweeps are handled properly.
	/// @param  ToPosition Target Position
	UFUNCTION(BlueprintCallable)
	void MoveCharacter(FVector ToPosition);

	/// @brief  Set a target rotation to be rotated to during the update phase. Movement sweeps are handled properly.
	/// @param  ToRotation Target rotation
	UFUNCTION(BlueprintCallable)
	void RotateCharacter(FQuat ToRotation);

	/// @brief  Get the velocity required to move [MoveDelta] amount in [DeltaTime] time.
	/// @param  MoveDelta Movement Delta To Compute Velocity
	/// @param  DeltaTime Delta Time In Which Move Would Be Performed
	/// @return Velocity corresponding to MoveDelta/DeltaTime if valid delta time was passed through
	UFUNCTION(BlueprintCallable)
	FVector GetVelocityFromMovement(FVector MoveDelta, float DeltaTime);

	/// @brief  Apply a specific movement state (e.g position, rotation, velocity, etc...)
	/// @param  StateToApply Motor state to go to, with its velocity, position, rotation and grounding status
	UFUNCTION(BlueprintCallable)
	void ApplyState(FMotorState StateToApply);

#pragma endregion Exposed Calls

/* Implementations of core Unreal Interfaces*/
#pragma region Interfaces

// Animation Interface for Root Motion
#pragma region Animation

protected:
	UPROPERTY(Transient)
	FRootMotionMovementParams RootMotionParams;

	UPROPERTY(BlueprintReadOnly, Category="Motor | Animation")
	bool bHasAnimRootMotion{false};

	UPROPERTY(BlueprintReadOnly, Category="Motor | Animation")
	USkeletalMeshComponent* SkeletalMesh{nullptr};

public:
	UFUNCTION(BlueprintCallable, Category="Motor | Animation")
	void SetSkeletalMeshReference(USkeletalMeshComponent* Mesh);

	UFUNCTION(BlueprintCallable, Category="Motor | Animation")
	bool HasAnimRootMotion() const;

protected:
	
	void BlockSkeletalMeshPose() const;

	bool StepMontage(USkeletalMeshComponent* Mesh, UAnimMontage* Montage, float Position, float Weight, float PlayRate);
	
	void ShouldTickPose();
	void TickPose(float DeltaTime);

	/// @brief  Checks whether root motion should stop. Occurs whether we're done with the montage, have no montage,
	///			or depending on blending parameters (bApplyRootMotionDuringBlendIn/Out)
	/// @param  RootMotionMontage Current montage pawn is playing
	/// @param  RootMotionMontagePosition Position in the montage
	/// @return True if the root motion should be discarded and revert back to manually controlled movement
	bool ShouldDiscardRootMotion(UAnimMontage* RootMotionMontage, float RootMotionMontagePosition) const;

	void CalculateRootMotionVelocity(float DeltaTime);
	void ApplyAnimRootMotionRotation(float DeltaTime);

	// Called in calculate root motion velocity...
	UFUNCTION(BlueprintNativeEvent, Category="Motor | Animation")
	FVector PostProcessRootMotionVelocity(const FVector& RootMotionVelocity, float DeltaTime);
	virtual FVector PostProcessRootMotionVelocity_Implementation(const FVector& RootMotionVelocity, float DeltaTime) {return RootMotionVelocity; }

#pragma endregion Animation

// AI Interface for navigation and RVO
#pragma region AI

	bool IsAIControlled();
	
	
#pragma endregion AI 

#pragma endregion Interfaces
	
/* The core update loop of the movement component */
#pragma region Core Update Loop 

	void PerformMovement(float DeltaTime);
	
	void PreMovementUpdate(float DeltaTime);
	void MovementUpdate(FVector& MoveVelocity, float DeltaTime);
	void PostMovementUpdate(float DeltaTime);

	// Do we wanna place all of that here instead? Closer knitting of Moving With Base & Moving From Our Velocity.
	void InternalMove(FVector& MoveVelocity, float& DeltaTime);

#pragma endregion Core Update Loop

/* Methods to impart forces and evaluate physics interactions*/
#pragma region Physics Interactions

public:

	/* Wanna keep this setting exposed and read/writable */
	/// @brief  Whether the pawn should interact with physics objects in the world
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category= "Motor | Physics Interactions")
	bool bEnablePhysicsInteractions = true;

protected:
	/* ===== Touch Forces ===== */

	/// @brief  If true, the "TouchForceScale" is applied per kg of the affected object
	UPROPERTY(EditAnywhere, Category="Motor | Physics Interactions", meta=(EditCondition="bEnablePhysicsInteractions", EditConditionHides))
	bool bScaleTouchForceToMass{true};

	/// @brief  Multiplier for the force that is applied to physics objects that are touched by the pawn
	UPROPERTY(EditAnywhere, Category="Motor | Physics Interactions", meta=(EditCondition="bEnablePhysicsInteractions", EditConditionHides, ClampMin="0", UIMin="0"))
	float TouchForceScale{1.f};

	/// @brief  The minimum force applied to physics objects touched by the pawn
	UPROPERTY(EditAnywhere, Category="Motor | Physics Interactions", meta=(EditCondition="bEnablePhysicsInteractions", EditConditionHides, ClampMin="0", UIMin="0"))
	float MinTouchForce{0.f};

	/// @brief  The maximum force applied to physics objects touched by the pawn
	UPROPERTY(EditAnywhere, Category="Motor | Physics Interactions", meta=(EditCondition="bEnablePhysicsInteractions", EditConditionHides, ClampMin="0", UIMin="0"))
	float MaxTouchForce{250.f};

	/* ===== Push Forces ===== */
	
	/// @brief  If true, the "PushForceScale" is applied per kg of the affected object
	UPROPERTY(EditAnywhere, Category="Motor | Physics Interactions | Push Forces", meta=(EditCondition="bEnablePhysicsInteractions", EditConditionHides))
	bool bScalePushForceToMass{true};

	UPROPERTY(EditAnywhere, Category="Motor | Physics Interactions | Push Forces", meta=(EditCondition="bEnablePhysicsInteractions", EditConditionHides))
	bool bScalePushForceToVelocity{true};
	
	/// @brief  Multiplier for the force that is applied when the player collides with a blocking physics objects
	UPROPERTY(EditAnywhere, Category="Motor | Physics Interactions | Push Forces", meta=(EditCondition="bEnablePhysicsInteractions", EditConditionHides, ClampMin="0", UIMin="0"))
	float PushForceScale{750000.f};

	/// @brief  Multiplier for the initial impulse force applied when the pawn bounces into a blocking physics object
	UPROPERTY(EditAnywhere, Category="Motor | Physics Interactions | Push Forces", meta=(EditCondition="bEnablePhysicsInteractions", EditConditionHides, ClampMin="0", UIMin="0"))
	float InitialPushForceScale{500.f};
	
	/* ===== Contextual Forces ===== */
	
	/// @brief  Multiplier for the gravity force applied to physics objects the pawn is walking on
	UPROPERTY(EditAnywhere, Category="Motor | Physics Interactions | Contextual Forces", meta=(EditCondition="bEnablePhysicsInteractions", EditConditionHides, ClampMin="0", UIMin="0"))
	float DownwardForceScale{1.f};

	/// @brief  The force applied constantly per kg of mass of the pawn to all overlapping components
	UPROPERTY(EditAnywhere, Category="Motor | Physics Interactions | Contextual Forces", meta=(EditCondition="bEnablePhysicsInteractions", EditConditionHides, ClampMin="0", UIMin="0"))
	float RepulsionForce{2.5f};
	
protected:
	// This being protected might cause issues since its a delegate for OnComponentBeginOverlap
	void RootCollisionTouched(
					UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
					UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex,
					bool bFromSweep, const FHitResult& SweepResult
					);

	void ApplyImpactPhysicsForces(const FHitResult& Impact, const FVector& ImpactAcceleration, const FVector& ImpartVelocity);
	void ApplyDownwardForce(float DeltaTime);
	void ApplyRepulsionForce(float DeltaTime);

#pragma endregion Physics Interactions

/* Methods to handle moving bases */
#pragma region Moving Base

protected:
	
	/// @brief  Whether to move with the platform/base relatively
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category= "Motor | Moving Base Settings")
	bool bMoveWithBase{true};

	/// @brief  Whether to impart the linear velocity of teh current movement base when falling off it. Velocity is never imparted
	///			from a base that is simulating physics.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category= "Motor | Moving Base Settings", meta=(EditCondition="bMoveWithBase", EditConditionHides))
	bool bImpartBaseVelocity{true};

	/// @brief  Whether to impart the angular velocity of the current movement base when falling off it. Angular velocity is never imparted
	///			from a base that is simulating physics
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category= "Motor | Moving Base Settings", meta=(EditCondition="bMoveWithBase", EditConditionHides))
	bool bImpactAngularBaseVelocity{false};

	// TODO: Do we want to consider bConsiderMassOnImpartVelocity?
	
	UPROPERTY(BlueprintSetter="SetMovingBaseOverride")
	AActor* MovingBaseOverride{nullptr};
	
protected:
	AActor* GetMovementBaseActor() const;
	void ShouldImpartVelocityFromBase(UPrimitiveComponent* MovementBase);
	void ComputeBaseVelocity(UPrimitiveComponent* MovementBase);
	void GetPawnBaseVelocity(APawn* PawnMovementBase);

	/// @brief  Specify a moving base to stay attached to and move relative to consistently (REALLY GOOD SHIT!!!)
	/// @param  BaseOverride The base to stay attached to (regardless of grounding status)
	/// @return True if the passed base is valid (e.g does not simulate physics)
	UFUNCTION(BlueprintSetter)
	void SetMovingBaseOverride(AActor* BaseOverride);

#pragma endregion Moving Base 
	
/* Methods to evaluate the stability of a given hit or overlap */
#pragma region Stability Evaluations

	void EvaluateHitStability(FHitResult Hit, FVector MoveVelocity, FHitStabilityReport& OutStabilityReport);

	void ProbeGround(FVector ProbingPosition, FQuat AtRotation, float ProbingDistance, FGroundingReport& OutGroundingReport);
	bool MustUnground() const;
	
	/// @brief  Compares the normals of two blocking hits that are not stable.
	/// @param	MoveVelocity
	/// @param  PrevVelocity 
	/// @param  CurHitNormal 
	/// @param  PrevHitNormal 
	/// @param  bCurrentHitIsStable 
	/// @param  bPreviousHitIsStable 
	/// @param  bCharIsStable 
	/// @param  bIsValidCrease 
	/// @param  CreaseDirection 
	void EvaluateCrease(FVector MoveVelocity, FVector PrevVelocity, FVector CurHitNormal, FVector PrevHitNormal, bool bCurrentHitIsStable, bool bPreviousHitIsStable, bool bCharIsStable, bool& bIsValidCrease, FVector& CreaseDirection);
	
	/// @brief  Determines if the pawn can be considered stable on a given slope normal.
	/// @param  Normal Given ground normal
	/// @return True if the pawn can be considered stable on a given slope normal
	bool IsStableOnNormal(FVector Normal) const;
	bool IsStableWithSpecialCases(const FHitStabilityReport& StabilityReport, FVector CharVelocity);

	void DetectSteps(FVector CharPosition, FQuat CharRotation, FVector HitPoint, FVector InnerHitDirection, FHitStabilityReport& StabilityReport);
	bool CheckStepValidity(int numStepHits, FVector CharPosition, FQuat CharRotation, FVector InnerHitDirection, FVector StepCheckStartPost, FHitResult& OutHit);

#pragma endregion Stability Evaluations


/* Methods to adjust movement based on the stability of a hit or overlap */
#pragma region Collision Adjustments

	void AutoResolvePenetration();

	virtual void HandleImpact(const FHitResult& Hit, float TimeSlice, const FVector& MoveDelta) override;
	virtual void TwoWallAdjust(FVector& Delta, const FHitResult& Hit, const FVector& OldHitNormal) const override;
	virtual float SlideAlongSurface(const FVector& Delta, float Time, const FVector& InNormal, FHitResult& OutHit, bool bHandleImpact) override;
	
	void InternalHandleVelocityProjection(bool bStableOnHit, FVector HitNormal, FVector ObstructionNormal, FVector OriginalDirection, EMovementSweepState& SweepState, bool bPreviousHitIsStable,
										FVector PrevVelocity, FVector PrevObstructionNormal, FVector& MoveVelocity, float& RemainingMoveDistance, FVector& RemainingMoveDirection);

	void HandleVelocityProjection(FVector& MoveVelocity, FVector ObstructionNormal, bool bStableOnHit);

#pragma endregion Collision Adjustments


/* Methods to sweep the environment and retrieve information */
#pragma region Collision Checks

	bool GroundSweep(FVector Position, FQuat Rotation, FVector Direction, float Distance, FHitResult& ClosestHit);

#pragma endregion Collision Checks

#pragma region Utility

#pragma endregion Utility
};
