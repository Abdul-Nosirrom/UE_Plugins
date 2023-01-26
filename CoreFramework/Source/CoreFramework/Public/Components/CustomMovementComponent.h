// Copyright 2023 Abdulrahmen Almodaimegh. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PawnMovementComponent.h"
#include "CustomMovementComponent.generated.h"

#define CM_DEBUG_BREAK GEngine->DeferredCommands.Add(TEXT("pause"));

/* Profiling */
DECLARE_STATS_GROUP(TEXT("RadicalMovementComponent_Game"), STATGROUP_RadicalMovementComp, STATCAT_Advanced)
/* ~~~~~~~~ */

#pragma region Enums

/// @brief	How to handle stepping
UENUM(BlueprintType)
enum EStepHandlingMethod
{
	None,
	Standard,
	Extra
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
	UPROPERTY(BlueprintReadOnly)
	bool bFoundAnyGround;
	/// @brief  True if pawn is standing on anything in StableGroundLayers that obeys the geometric restrictions set.
	UPROPERTY(BlueprintReadOnly)
	bool bIsStableOnGround;
	/// @brief  True if IsStableWithSpecialCases returns false. Meaning snapping is disabled when exceeded a certain velocity
	///			for ledge snapping, distance from a ledge beyond a certain threshold, or slope angle greater than certain denivelation (Delta) angle.
	///			Will always be false if bLedgeAndDenivelationHandling is not enabled.
	bool bSnappingPrevented;

	UPROPERTY(BlueprintReadOnly)
	FVector GroundNormal;
	FVector InnerGroundNormal;
	FVector OuterGroundNormal;

	UPROPERTY(BlueprintReadOnly)
	FHitResult GroundHit;
	UPROPERTY(BlueprintReadOnly)
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

#pragma region Debug & Logging

struct FSimulationState
{
	uint8 bFoundLedge			: 1;
	uint8 bValidatedSteps		: 1;
	uint8 bSnappingPrevented	: 1;
	uint8 bIsMovingWithBase		: 1;
	uint8 bIsPlayingRM			: 1;
	
	float GroundAngle;
	float Speed;
	float MontagePosition;

	float LastGroundSnappingDistance;
	float LastSuccessfulStepHeight;
	float LastExceededDenivelationAngle;

	FVector MovingBaseVelocity;
	FVector RootMotionVelocity;
};

#pragma endregion Debug & Logging

UCLASS(ClassGroup = "Kinematic Movement", BlueprintType, Blueprintable)
class COREFRAMEWORK_API UCustomMovementComponent : public UPawnMovementComponent
{
	GENERATED_BODY()

#pragma region Core Movement Parameters

public:
#pragma region Ground Stability Settings

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Motor | Ground Settings")
	bool bSolveGrounding{true};
	
	/// @brief  Extra probing distance for ground detection.
	UPROPERTY(EditDefaultsOnly, Category= "Motor | Ground Settings", meta=(EditCondition="bSolveGrounding", EditConditionHides))
	float GroundDetectionExtraDistance = 0.f;
	
	/// @brief  Maximum ground slope angle relative to the actor up direction.
	UPROPERTY(EditDefaultsOnly, Category= "Motor | Ground Settings", meta=(EditCondition="bSolveGrounding", EditConditionHides))
	float MaxStableSlopeAngle = 60.f;

	UPROPERTY(BlueprintReadOnly, Category="Motor | Ground Status")
	FGroundingReport GroundingStatus = FGroundingReport();
	UPROPERTY(BlueprintReadOnly, Category="Motor | Ground Status")
	FGroundingReport LastGroundingStatus = FGroundingReport();

	UPROPERTY(BlueprintReadOnly, Category="Motor | Ground Status")
	bool bLastMovementIterationFoundAnyGround;

	UPROPERTY(BlueprintReadOnly, Category="Motor | Ground Status")
	bool bStuckInGeometry;

	bool bMustUnground;

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
	UPROPERTY(EditDefaultsOnly, Category= "Motor | Step Settings", meta=(EditCondition = "StepHandling == EStephandlingMethod::Extra", EditConditionHides))
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

	UPROPERTY(EditDefaultsOnly, Category= "Motor | Simulation Settings")
	int32 MaxMovementIterations = 5;
	UPROPERTY(EditDefaultsOnly, Category= "Motor | Simulation Settings")
	int32 MaxDecollisionIterations = 1;
	UPROPERTY(EditDefaultsOnly, Category= "Motor | Simulation Settings")
	bool bCheckMovementInitialOverlaps = true;
	UPROPERTY(EditDefaultsOnly, Category= "Motor | Simulation Settings")
	bool bKillVelocityWhenExceedMaxMovementIterations = true;
	UPROPERTY(EditDefaultsOnly, Category= "Motor | Simulation Settings")
	bool bKillRemainingMovementWhenExceedMovementIterations = true;

#pragma endregion Simulation Parameters

#pragma endregion Core Movement Parameters

#pragma region Debug Parameters

	UPROPERTY(EditInstanceOnly, Category = "Motor | Debug Settings")
	bool bDebugGroundSweep{false};
	UPROPERTY(EditInstanceOnly, Category = "Motor | Debug Settings", AdvancedDisplay, meta=(EditCondition="bDebugGroundSweep", EditConditionHides))
	FColor GroundSweepDebugColor{FColor::Red};
	UPROPERTY(EditInstanceOnly, Category = "Motor | Debug Settings", AdvancedDisplay, meta=(EditCondition="bDebugGroundSweep", EditConditionHides))
	FColor GroundSweepHitDebugColor{FColor::Green};
	
	UPROPERTY(EditInstanceOnly, Category = "Motor | Debug Settings")
	bool bDebugLineTrace{false};
	UPROPERTY(EditInstanceOnly, Category = "Motor | Debug Settings", AdvancedDisplay, meta=(EditCondition="bDebugLineTrace", EditConditionHides))
	FColor LineTraceDebugColor{FColor::Red};
	UPROPERTY(EditInstanceOnly, Category = "Motor | Debug Settings", AdvancedDisplay, meta=(EditCondition="bDebugLineTrace", EditConditionHides))
	FColor LineTraceHitDebugColor{FColor::Green};

	FSimulationState DebugSimulationState{None};
	
#pragma endregion Debug Parameters

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
	static constexpr float CORRELATION_FOR_VERTICAL_OBSTRUCTION = 0.01f; // For dot product so we don't scale it for units
	static constexpr float EXTRA_STEPPING_FORWARD_DISTANCE = 1.f;
	static constexpr float EXTRA_STEPPING_HEIGHT_PADDING = 1.f;
	static constexpr float MIN_DELTA_TIME = 1e-6f;

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
protected:

	/* Physics State Parameters */
	UPROPERTY()
	FVector Acceleration;

	//UPROPERTY()
	//float Mass; Exists under Physics interaction, should I move it here?

	/* ~~~~~~~~~~~~~~~~~~~~~~~ */
	
	/* Interface Handling Parameters */
	/// @brief Whether SetVelocity has been called externally
	//uint8 bSetVelocity			: 1		{false};
	
	/// @brief  Whether a direct call to MoveCharacter has been made
	UPROPERTY()
	bool bMovePositionDirty{false};

	/// @brief  Whether a direct call to RotateCharacter has been made
	UPROPERTY()
	bool bMoveRotationDirty{false};

	/// @brief  The target of a direct call to MoveCharacter
	UPROPERTY()
	FVector MovePositionTarget;
	
	/// @brief  The target of a direct call to RotateCharacter
	UPROPERTY()
	FQuat MoveRotationTarget;

	/// @brief Accumulated velocity through AddVelocity between update ticks
	//FVector VelocityToAdd;

	/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
	
public:
	
	void HaltMovement();
	void EnableMovement();
	void DisableMovement();

	/// @brief  Unground and prevent snapping to allow for leaving ground
	UFUNCTION(BlueprintCallable, Category="Physics State")
	void ForceUnground();

	/// @brief  Set a target position to be moved to during the update phase. Movement sweeps are handled properly.
	/// @param  ToPosition Target Position
	UFUNCTION(BlueprintCallable, Category="Physics State")
	void MoveCharacter(FVector ToPosition);

	/// @brief  Set a target rotation to be rotated to during the update phase. Movement sweeps are handled properly.
	/// @param  ToRotation Target rotation
	UFUNCTION(BlueprintCallable, Category="Physics State")
	void RotateCharacter(FQuat ToRotation);

	/// @brief  Get the velocity required to move [MoveDelta] amount in [DeltaTime] time.
	/// @param  MoveDelta Movement Delta To Compute Velocity
	/// @param  DeltaTime Delta Time In Which Move Would Be Performed
	/// @return Velocity corresponding to MoveDelta/DeltaTime if valid delta time was passed through
	UFUNCTION(BlueprintCallable, Category="Physics State")
	FVector GetVelocityFromMovement(FVector MoveDelta, float DeltaTime);

	/// @brief  Apply a specific movement state (e.g position, rotation, velocity, etc...)
	/// @param  StateToApply Motor state to go to, with its velocity, position, rotation and grounding status
	UFUNCTION(BlueprintCallable, Category="Physics State")
	void ApplyState(FMotorState StateToApply);
	
	UFUNCTION(BlueprintCallable, Category="Physics State")
	FORCEINLINE FVector GetVelocity() const { return Velocity; }

	UFUNCTION(BlueprintCallable, Category="Physics State")
	void SetVelocity(const FVector& NewVelocity);

	UFUNCTION(BlueprintCallable, Category="Physics State")
	void AddVelocity(const FVector& AddVelocity);

	UFUNCTION(BlueprintCallable, Category="Physics State")
	void AddImpulse(const FVector& Impulse, bool bVelChange);

#pragma region Events

	/// @brief Entry point for gameplay manipulation of rotation via blueprints or child class
	/// @param CurrentRotation Reference to current rotation to modify
	/// @param DeltaTime Current sub-step delta time
	//UFUNCTION(BlueprintNativeEvent, Category="Movement Controller")
	virtual void UpdateRotation(FQuat& CurrentRotation, float DeltaTime) {};
	//virtual void UpdateRotation_Implementation(FQuat& CurrentRotation, float DeltaTime) {return;};

	/// @brief	Entry point for gameplay manipulation of velocity via blueprints or child class
	///			Velocity should only be modified through here as the order is important to what updates come after
	/// @param	CurrentVelocity Reference to current velocity to modify
	/// @param	DeltaTime Current sub-step delta time
	//UFUNCTION(BlueprintNativeEvent, Category="Movement Controller")
	virtual void UpdateVelocity(FVector& CurrentVelocity, float DeltaTime) {};
	//virtual void UpdateVelocity_Implementation(FVector& CurrentVelocity, float DeltaTime) {return;};

	//UFUNCTION(BlueprintNativeEvent, Category="Movement Controller")
	virtual void SubsteppedTick(FVector& CurrentVelocity, float DeltaTime) {};
	//virtual void SubsteppedTick_Implementation(FVector& CurrentVelocity, float DeltaTime) {return;};

#pragma endregion Events

#pragma endregion Exposed Calls
	
	
/* The core update loop of the movement component */
#pragma region Core Update Loop 

	void PerformMovement(float DeltaTime);
	
	virtual void PreMovementUpdate(float DeltaTime);
	void MovementUpdate(FVector& MoveVelocity, float DeltaTime);
	virtual void PostMovementUpdate(float DeltaTime);


#pragma endregion Core Update Loop

/* Methods and fields to handle root motion */
#pragma region Root Motion

protected:

	UPROPERTY(Transient)
	FRootMotionMovementParams RootMotionParams;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Motor | Animation")
	bool bApplyRootMotionDuringBlendIn{true};

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Motor | Animation")
	bool bApplyRootMotionDuringBlendOut{true};

	UPROPERTY(BlueprintReadWrite, Category="Motor | Animation")
	bool bHasAnimRootMotion{false};
	
	UPROPERTY(BlueprintReadWrite, Category="Motor | Animation")
	float AnimRootMotionTranslationScale{1.f};

	UPROPERTY(BlueprintReadWrite, Category="Motor | Animation")
	USkeletalMeshComponent* SkeletalMesh{nullptr};

	
public:
	
	virtual void SetSkeletalMeshReference(USkeletalMeshComponent* Mesh);

	float GetAnimRootMotionTranslationScale() const;

	void SetAnimRootMotionTranslationScale(float Scale = 1.f);

	UFUNCTION(BlueprintCallable)
	bool IsPlayingMontage() const;

	UFUNCTION(BlueprintCallable)
	bool IsPlayingRootMotion() const;

protected:
	
	/// @brief	Returns the currently playing root motion montage instance (if any)
	/// @return Current root motion montage instance or nullptr if none is currently playing
	FAnimMontageInstance* GetRootMotionMontageInstance() const;
	
	void BlockSkeletalMeshPoseTick() const;

	void TickPose(float DeltaTime);

	virtual void ApplyAnimRootMotionRotation(float DeltaTime);

	virtual void CalculateAnimRootMotionVelocity(float DeltaTime);

	bool ShouldDiscardRootMotion(UAnimMontage* RootMotionMontage, float RootMotionMontagePosition) const;
	
	UFUNCTION(BlueprintNativeEvent, Category = "General Movement Component")
	FVector PostProcessAnimRootMotionVelocity(const FVector& RootMotionVelocity, float DeltaSeconds);
	virtual FVector PostProcessAnimRootMotionVelocity_Implementation(const FVector& RootMotionVelocity, float DeltaSeconds) {return RootMotionVelocity; }

#pragma endregion Root Motion
	
/* Methods to impart forces and evaluate physics interactions*/
#pragma region Physics Interactions

public:
	/* Wanna keep this setting exposed and read/writable */
	/// @brief  Whether the pawn should interact with physics objects in the world
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category= "Motor | Physics Interactions")
	bool bEnablePhysicsInteraction{true};

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motor | Physics Interactions", meta = (ClampMin = "0.0001", UIMin = "1", EditCondition="bEnablePhysicsInteraction"))
	float Mass{100.f};
	
	/// @brief  If true, "TouchForceScale" is applied per kilogram of mass of the affected object.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category= "Motor | Physics Interactions", meta=(EditCondition="bEnablePhysicsInteraction"))
	bool bScaleTouchForceToMass{true};

	/// @brief  Multiplier for the force that is applied to physics objects that are touched by the pawn.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category= "Motor | Physics Interactions", meta=(EditCondition="bEnablePhysicsInteraction", ClampMin = "0", UIMin="0"))
	float TouchForceScale{1.f};

	/// @brief  The minimum force applied to physics objects touched by the pawn.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category= "Motor | Physics Interactions", meta=(EditCondition="bEnablePhysicsInteraction", ClampMin = "0", UIMin="0"))
	float MinTouchForce{0.f};

	/// @brief  The maximum force applied to physics objects touched by the pawn.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category= "Motor | Physics Interactions", meta=(EditCondition="bEnablePhysicsInteraction", ClampMin = "0", UIMin="0"))
	float MaxTouchForce{250.f};

	
	/// @brief Event invoked when the root collision component hits another collision component
	/// @param OverlappedComponent 
	/// @param OtherActor 
	/// @param OtherComponent 
	/// @param OtherBodyIndex 
	/// @param bFromSweep 
	/// @param SweepResult 
	UFUNCTION()
	virtual void RootCollisionTouched(
	  UPrimitiveComponent* OverlappedComponent,
	  AActor* OtherActor,
	  UPrimitiveComponent* OtherComponent,
	  int32 OtherBodyIndex,
	  bool bFromSweep,
	  const FHitResult& SweepResult
	);

#pragma endregion Physics Interactions

/* Methods to handle moving bases */
#pragma region Moving Base

protected:
	
	/// @brief  Whether to move with the platform/base relatively
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category= "Motor | Movement Base Settings")
	bool bMoveWithBase{true};

	/// @brief  Whether to impart the linear velocity of teh current movement base when falling off it. Velocity is never imparted
	///			from a base that is simulating physics.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category= "Motor | Movement Base Settings", meta=(EditCondition="bMoveWithBase", EditConditionHides))
	bool bImpartBaseVelocity{true};

	/// @brief  Whether to impart the angular velocity of the current movement base when falling off it. Angular velocity is never imparted
	///			from a base that is simulating physics
	//UPROPERTY(EditAnywhere, BlueprintReadWrite, Category= "Motor | Moving Base Settings", meta=(EditCondition="bMoveWithBase", EditConditionHides))
	//bool bImpactAngularBaseVelocity{false};

	// TODO: Do we want to consider bConsiderMassOnImpartVelocity?
	
	UPROPERTY(BlueprintSetter="SetMovementBaseOverride")
	UPrimitiveComponent* MovementBaseOverride{nullptr};

	void MovementBaseUpdate(float DeltaTime);

	/// @brief Set a moving base to keep, useful for things like following an elevator where when you jump, you still
	///			want to follow it
	/// @param BaseOverride Override any moving bases with the one passed through here
	UFUNCTION(BlueprintSetter)
	void SetMovementBaseOverride(UPrimitiveComponent* BaseOverride) {MovementBaseOverride = BaseOverride;}

	/// @brief Given a primitive component, get its velocity to be used in moving base update
	/// @param MovementBase Current moving base whose velocity is desired
	/// @return Correct velocity of the moving base, considering physics
	FVector ComputeBaseVelocity(UPrimitiveComponent* MovementBase) const;

	/// @brief Given a primitive component, compute its effective rotation on movement component owner
	/// @param MovementBase Current moving base whose rotation is desired
	/// @return Quaternion rotation as a result of Base angular velocity
	FQuat ComputeBaseRotation(UPrimitiveComponent* MovementBase, float DeltaTime) const;
	
	/// @brief Check the validity of the ground as a moving base
	/// @param MovementBase Moving base to check the validity of, likely from GroundingStatus hit result
	/// @return True if component is not null and is movable
	bool IsValidMovementBase(UPrimitiveComponent* MovementBase) const;

	/// @brief Check whether should impart velocity from current moving base
	/// @param MovementBase Moving base to check whether to impart velocity from
	/// @return True if component is movable, and NOT simulating physics (Can cause large impulse)
	bool ShouldImpartVelocityFromBase(UPrimitiveComponent* MovementBase) const; 

#pragma endregion Moving Base 
	
/* Methods to evaluate the stability of a given hit or overlap */
#pragma region Stability Evaluations

	/// @brief  Checks whether we are currently able to move
	/// @return True if we are not stuck in geometry or have valid data
	bool CanMove();
	
	void EvaluateHitStability(FHitResult Hit, FVector MoveVelocity, FHitStabilityReport& OutStabilityReport);

	void ProbeGround(float ProbingDistance, FGroundingReport& OutGroundingReport);
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

	bool CheckStepValidity(FHitResult& StepHit, FVector InnerHitDirection);

#pragma endregion Stability Evaluations


/* Methods to adjust movement based on the stability of a hit or overlap */
#pragma region Collision Adjustments

	FHitResult SinglePeneterationResolution();
	FHitResult AutoResolvePenetration();

	virtual void HandleImpact(const FHitResult& Hit, float TimeSlice = 0.f, const FVector& MoveDelta = FVector::ZeroVector) override;

	void HandleVelocityProjection(FVector& MoveVelocity, FVector ObstructionNormal, bool bStableOnHit);

	bool StepUp(FHitResult StepHit, FVector InnerHitDirection, FHitResult* OutForwardHit = nullptr);

	virtual float SlideAlongSurface(const FVector& Delta, float Time, const FVector& Normal, FHitResult& Hit, bool bHandleImpact) override;
	virtual void TwoWallAdjust(FVector& Delta, const FHitResult& Hit, const FVector& OldHitNormal) const override;

#pragma endregion Collision Adjustments


/* Methods to sweep the environment and retrieve information */
#pragma region Collision Checks

	bool GroundSweep(FVector Position, FQuat Rotation, FVector Direction, float Distance, FHitResult& OutHit);
	bool CollisionLineCast(FVector StartPoint, FVector Direction, float Distance, FHitResult& OutHit);

#pragma endregion Collision Checks

#pragma region Utility

	FVector GetObstructionNormal(const FVector HitNormal, const bool bStableOnHit) const;
	FVector GetDirectionTangentToSurface(const FVector Direction, const FVector SurfaceNormal) const;

#pragma endregion Utility
};

FORCEINLINE FVector UCustomMovementComponent::GetObstructionNormal(const FVector HitNormal, const bool bStableOnHit) const
{
	FVector ObstructionNormal = HitNormal;

	if (GroundingStatus.bIsStableOnGround && !MustUnground() && !bStableOnHit)
	{
		const FVector ObstructionLeftAlongGround = (GroundingStatus.GroundNormal ^ ObstructionNormal).GetSafeNormal();
		ObstructionNormal = (ObstructionLeftAlongGround ^ UpdatedComponent->GetUpVector()).GetSafeNormal();
	}

	if (ObstructionNormal.SizeSquared() == 0.f)
	{
		ObstructionNormal = HitNormal;
	}

	return ObstructionNormal;
}


FORCEINLINE FVector UCustomMovementComponent::GetDirectionTangentToSurface(const FVector Direction, const FVector SurfaceNormal) const
{
	const FVector DirectionRight = Direction ^ UpdatedComponent->GetUpVector();
	return (SurfaceNormal ^ DirectionRight).GetSafeNormal();
}

FORCEINLINE float UCustomMovementComponent::GetAnimRootMotionTranslationScale() const
{
	return AnimRootMotionTranslationScale;
}

FORCEINLINE void UCustomMovementComponent::SetAnimRootMotionTranslationScale(float Scale)
{
	AnimRootMotionTranslationScale = Scale;
}


