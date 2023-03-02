// Copyright 2023 Abdulrahmen Almodaimegh. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MovementData.h"
#include "GameFramework/PawnMovementComponent.h"
#include "CustomMovementComponent.generated.h"

/* Profiling */
DECLARE_STATS_GROUP(TEXT("RadicalMovementComponent_Game"), STATGROUP_RadicalMovementComp, STATCAT_Advanced)
DECLARE_LOG_CATEGORY_EXTERN(LogRMCMovement, Log, All);
/* ~~~~~~~~ */

/* Events */
DECLARE_DYNAMIC_DELEGATE_TwoParams(FCalculateVelocitySignature, UCustomMovementComponent*, MovementComponent, float, DeltaTime);
DECLARE_DYNAMIC_DELEGATE_TwoParams(FUpdateRotationSignature, UCustomMovementComponent*, MovementComponent, float, DeltaTime);
/* ~~~~~~ */

/* Forward Declarations */
class AOPCharacter;
class UMovementData;
//struct FBasedMovementInfo;

#pragma region Enums

UENUM(BlueprintType)
enum EMovementState
{
	/* None (Movement disabled) */
	STATE_None		UMETA(DisplayName="None"),

	/* Grounded on a stable surface*/
	STATE_Grounded	UMETA(DisplayName="Grounded"),

	/* No floor or floor is unstable */
	STATE_Falling	UMETA(DisplayName="Falling"),
};

UENUM(BlueprintType)
enum EOrientationMode
{
	MODE_PawnUp			UMETA(DisplayName="Pawn Up"),

	MODE_Gravity		UMETA(DisplayName="Gravity Direction"),
};

#pragma endregion Enums 

#pragma region Structs

/** 
 * Tick function that calls UCustomMovementComponent::PostPhysicsTickComponent
 **/
USTRUCT()
struct COREFRAMEWORK_API FCustomMovementComponentPostPhysicsTickFunction : public FTickFunction
{
	GENERATED_USTRUCT_BODY()

	/** CustomMovementComponent that is the target of this tick **/
	class UCustomMovementComponent* Target;

	/** 
	 * Abstract function actually execute the tick. 
	 * @param DeltaTime - frame time to advance, in seconds
	 * @param TickType - kind of tick for this frame
	 * @param CurrentThread - thread we are executing on, useful to pass along as new tasks are created
	 * @param MyCompletionGraphEvent - completion event for this task. Useful for holding the completion of this task until certain child tasks are complete.
	 **/
	virtual void ExecuteTick(float DeltaTime, enum ELevelTick TickType, ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent) override;
};

template<>
struct TStructOpsTypeTraits<FCustomMovementComponentPostPhysicsTickFunction> : public TStructOpsTypeTraitsBase2<FCustomMovementComponentPostPhysicsTickFunction>
{
	enum
	{
		WithCopy = false
	};
};

USTRUCT(BlueprintType)
struct COREFRAMEWORK_API FGroundingStatus
{
	GENERATED_USTRUCT_BODY()

	/// @brief  True if there was a blocking hit in the floor test that was NOT in initial penetration.
	///			The hit result can give more info about other circumstances
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = CharacterFloor)
	uint32 bBlockingHit		: 1;
	
	/// @brief  True if the hit found a valid stable, walkable floor
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = CharacterFloor)
	uint32 bWalkableFloor	: 1;

	/// @brief  True if bBlockingHit && !bWalkableFloor. A floor was found but it is not stable
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = CharacterFloor)
	uint32 bUnstableFloor	: 1;

	/// @brief  True if the hit found a floor using a line trace (rather than a sweep trace, which happens when the sweep test fails to yield a valid floor)
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = CharacterFloor)
	uint32 bLineTrace		: 1;

	/// @brief  The distance to the floor, computed from the swept capsule trace
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = CharacterFloor)
	float FloorDist;

	/// @brief  The distance to the floor, computed from the line trace. Only valid if bLineTrace is true
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = CharacterFloor)
	float LineDist;

	/// @brief  Hit result of the test that found a floor. Includes more specific data about the point of impact and surface normal at that point
	FHitResult HitResult;

public:

	FGroundingStatus()
		: bBlockingHit(false)
		, bWalkableFloor(false)
		, bUnstableFloor(false)
		, bLineTrace(false)
		, FloorDist(0.f)
		, LineDist(0.f)
		, HitResult(1.f)
	{
	}

	/// @brief  Check whether assigned floor is walkable
	/// @return True if the assigned floor is walkable
	bool IsWalkableFloor() const
	{
		return bBlockingHit && bWalkableFloor;
	}

	void Clear()
	{
		bBlockingHit = false;
		bWalkableFloor = false;
		bUnstableFloor = false;
		bLineTrace = false;
		FloorDist = 0.f;
		LineDist = 0.f;
		HitResult.Reset(1.f, false);
	}

	/// @brief  Getter for floor distance, either from LineDist or FloorDist depending on which trace result is used
	/// @return Returns the distance to the floor
	float GetDistanceToFloor() const
	{
		// When the floor distance is set using SetFromSweep, the LineDist value will be reset.
		// However, when SetLineFromTrace is used, there's no guarantee that FloorDist is set.
		return bLineTrace ? LineDist : FloorDist;
	}

	void SetFromSweep(const FHitResult& InHit, const float InSweepFloorDist, const bool bIsWalkableFloor)
	{
		bBlockingHit = InHit.IsValidBlockingHit();
		bWalkableFloor = bIsWalkableFloor;
		bUnstableFloor = bBlockingHit && !bWalkableFloor; 
		bLineTrace = false;
		FloorDist = InSweepFloorDist;
		LineDist = 0.f;
		HitResult = InHit;
	}
	
	void SetFromLineTrace(const FHitResult& InHit, const float InSweepFloorDist, const float InLineDist, const bool bIsWalkableFloor)
	{
		// We require a sweep that hit if we are going to use a line result.
		check(HitResult.bBlockingHit);
		if (HitResult.bBlockingHit && InHit.bBlockingHit)
		{
			// Override most of the sweep result with the line result, but save some values
			FHitResult OldHit(HitResult);
			HitResult = InHit;

			// Restore some of the old values. However, we want the new normals and hit actor.
			HitResult.Time = OldHit.Time;
			HitResult.ImpactPoint = OldHit.ImpactPoint;
			HitResult.Location = OldHit.Location;
			HitResult.TraceStart = OldHit.TraceStart;
			HitResult.TraceEnd = OldHit.TraceEnd;

			bLineTrace = true;
			FloorDist = InSweepFloorDist;
			LineDist = InLineDist;
			bWalkableFloor = bIsWalkableFloor;
			bUnstableFloor = bBlockingHit && !bWalkableFloor;
		}
	}

};

struct COREFRAMEWORK_API FStepDownFloorResult
{
	uint32 bComputedFloor : 1;		// True if the floor was computed as a result of the step down
	FGroundingStatus FloorResult;

	FStepDownFloorResult() : bComputedFloor(false) {}
};

#pragma endregion Structs

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

public:
	// Sets default values for this component's properties
	UCustomMovementComponent();

	UPROPERTY()
	TObjectPtr<AOPCharacter> CharacterOwner;

public:
	// BEGIN UActorComponent Interface
	virtual void BeginPlay() override;
	virtual void PostLoad() override;
	virtual void Deactivate() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	// END UActorComponent Interface

	UPROPERTY()
	FCustomMovementComponentPostPhysicsTickFunction PostPhysicsTickFunction;
	
	virtual void PostPhysicsTickComponent(float DeltaTime, FCustomMovementComponentPostPhysicsTickFunction& ThisTickFunction);
	

	
/* Overriding methods from parent */
#pragma region Movement Component Overrides

	// BEGIN UMovementComponent Interface
	virtual void SetUpdatedComponent(USceneComponent* NewUpdatedComponent) override;
	FORCEINLINE virtual bool IsMovingOnGround() const override { return PhysicsState == STATE_Grounded; }
	FORCEINLINE virtual bool IsFalling() const override { return PhysicsState == STATE_Falling; }
	virtual void AddRadialForce(const FVector& Origin, float Radius, float Strength, ERadialImpulseFalloff Falloff) override;
	virtual void AddRadialImpulse(const FVector& Origin, float Radius, float Strength, ERadialImpulseFalloff Falloff, bool bVelChange) override;
	virtual void StopActiveMovement() override;			// Check CMC
	// END UMovementComponent Interface

	// BEGIN UPawnMovementComponent Interface
	virtual void NotifyBumpedPawn(APawn* BumpedPawn) override;
	virtual void OnTeleported() override;
	// END UPawnMovementComponent Interface

#pragma endregion Movement Component Overrides

/* Fields & Methods containing the movement state and API stuff*/
#pragma region Movement State & Interface
protected:

	/* Physics State Parameters */
	UPROPERTY(Category="Motor | Physics State", BlueprintReadOnly)
	TEnumAsByte<EMovementState> PhysicsState;

	UPROPERTY(Category="Motor | Physics State", EditAnywhere, BlueprintReadWrite)
	TObjectPtr<UMovementData> MovementData;

	UPROPERTY(Category="Motor | Physics State", EditDefaultsOnly)
	bool bAlwaysOrientToGravity; // TODO: Temp for debug
	
	UPROPERTY()
	FVector Acceleration;
	UPROPERTY()
	FQuat LastUpdateRotation;
	UPROPERTY()
	FVector LastUpdateLocation;
	UPROPERTY()
	FVector LastUpdateVelocity;

	/** Returns the location at the end of the last tick. */
	UFUNCTION(BlueprintCallable)
	FVector GetLastUpdateLocation() const { return LastUpdateLocation; }

	/** Returns the rotation at the end of the last tick. */
	UFUNCTION(BlueprintCallable)
	FRotator GetLastUpdateRotation() const { return LastUpdateRotation.Rotator(); }

	/** Returns the rotation Quat at the end of the last tick. */
	FQuat GetLastUpdateQuat() const { return LastUpdateRotation; }

	/** Returns the velocity at the end of the last tick. */
	UFUNCTION(BlueprintCallable)
	FVector GetLastUpdateVelocity() const { return LastUpdateVelocity; }
	
	//UPROPERTY()
	//float Mass; Exists under Physics interaction, should I move it here?

	/* ~~~~~~~~~~~~~~~~~~~~~~~ */
	
	/* Interface Handling Parameters */
	UPROPERTY()
	FVector PendingLaunchVelocity;
	UPROPERTY()
	FVector PendingImpulseToApply;
	UPROPERTY()
	FVector PendingForceToApply;
	UPROPERTY()
	FVector InputVector;
	/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
	
public:

	/*~~~~~ Orientation, scale all that jazz ~~~~~*/

	/// @brief There are 3 possible values here to return, Either: 
	///	(1) Player Up Vector (Stuff where its in terms of their orientation 
	///	(2) Negative Gravity (So a constant value regardless of surface normal or player orientation, likely used in air) 
	///	(3) Ground normal
	FORCEINLINE FVector GetUpOrientation(EOrientationMode OrientationMode) const /* Perhaps having a fallback be a parameter? */
	{
		if (bAlwaysOrientToGravity) return -MovementData->GetGravityDir();
		switch (OrientationMode)
		{
			case MODE_PawnUp:
				return UpdatedComponent->GetUpVector();
			case MODE_Gravity:
				return MovementData->GetGravityDir().IsZero() ? UpdatedComponent->GetUpVector() : -MovementData->GetGravityDir();
			default:
				return UpdatedComponent->GetUpVector();
		}
	}

	// TODO: Might wanna also (not here but in utilities) add some math methods for computing stuff in w.r.t a given orientation mode (e.g steps). Would be super nice to localize that math in one place.

	/*~~~~~ Movement State Interface ~~~~~*/

	UFUNCTION(Category="(Radical Movement): Physics State", BlueprintCallable)
	virtual void SetMovementState(EMovementState NewMovementState);

	UFUNCTION(Category="(Radical Movement): Physics State", BlueprintCallable)
	EMovementState GetMovementState() const { return PhysicsState; }
		
	virtual void OnMovementStateChanged(EMovementState PreviousMovementState);

	void DisableMovement();
	
	/// @brief  Get the velocity required to move [MoveDelta] amount in [DeltaTime] time.
	/// @param  MoveDelta Movement Delta To Compute Velocity
	/// @param  DeltaTime Delta Time In Which Move Would Be Performed
	/// @return Velocity corresponding to MoveDelta/DeltaTime if valid delta time was passed through
	UFUNCTION(Category="(Radical Movement): Physics State", BlueprintCallable)
	FORCEINLINE FVector GetVelocityFromMovement(FVector MoveDelta, float DeltaTime) const 
	{
		if (DeltaTime <= 0.f) return FVector::ZeroVector;
		return MoveDelta / DeltaTime;
	}
	
	UFUNCTION(Category="(Radical Movement): Physics State", BlueprintCallable)
	FORCEINLINE FVector GetVelocity() const { return Velocity; }

	UFUNCTION(Category="(Radical Movement): Physics State", BlueprintCallable)
	FORCEINLINE void SetVelocity(const FVector TargetVelocity) { Velocity = TargetVelocity; }

	UFUNCTION(Category="(Radical Movement): Physics State", BlueprintCallable)
	FORCEINLINE FVector GetAcceleration() const { return Acceleration; }

	UFUNCTION(Category="(Radical Movement): Physics State", BlueprintCallable)
	FORCEINLINE void SetAcceleration(const FVector TargetAcceleration) { Acceleration = TargetAcceleration; }

	UFUNCTION(Category="(Radical Movement): Physics State", BlueprintCallable)
	FORCEINLINE bool IsStableOnGround() const { return IsMovingOnGround(); }

	UFUNCTION(Category="(Radical Movement): Physics State", BlueprintCallable)
	FORCEINLINE bool IsUnstableOnGround() const { return CurrentFloor.bUnstableFloor; }

	UFUNCTION(Category="(Radical Movement): Physics State", BlueprintCallable)
	FORCEINLINE bool IsInAir() const { return IsFalling(); }

	UFUNCTION(Category="(Radical Movement): Physics State", BlueprintCallable)
	virtual void AddImpulse(FVector Impulse, bool bVelocityChange = false);

	UFUNCTION(Category="(Radical Movement): Physics State", BlueprintCallable)
	virtual void AddForce(FVector Force);

	UFUNCTION(Category="(Radical Movement): Physics State", BlueprintCallable)
	virtual void Launch(const FVector& LaunchVel);
	
	/// @brief	Clears forces accumulated through AddImpulse(), AddForce(), and also PendingLaunchVelocity
	UFUNCTION(Category="(Radical Movement): Physics State", BlueprintCallable)
	virtual void ClearAccumulatedForces();

	/// @brief  Applies momentum accumulated through AddImpulse() and AddForce(), then clears those forces.
	///			NOTE: Does not use ClearAccumulatedForces() since that would clear pending launch velocity as well
	virtual void ApplyAccumulatedForces(float DeltaTime);

	/// @brief  Handles a pending launch during an update (primarily from the character class, LaunchCharacter)
	/// @return True if the launch was triggered
	virtual bool HandlePendingLaunch();

#pragma region Events

	// TODO: Review these...
	UPROPERTY()
	FCalculateVelocitySignature CalculateVelocityDelegate;
	
	UPROPERTY()
	FUpdateRotationSignature UpdateRotationDelegate;


#pragma endregion Events

#pragma endregion Movement State & Interface
	
/* Core simulation loop methods & parameters */
#pragma region Core Simulation Handling

protected:
	/* CONSTANTS FOR SWEEP AND PHYSICS CALCULATIONS */
	static constexpr float MIN_TICK_TIME				= 1e-6f; 
	static constexpr float MIN_FLOOR_DIST				= 1.9f; 
	static constexpr float MAX_FLOOR_DIST				= 2.4f; 
	static constexpr float SWEEP_EDGE_REJECT_DISTANCE	= 0.15f;

protected:
	/// @brief  Used within movement code to determine if a change in position is based on normal movement or a teleport. If not a teleport,
	///			velocity can be recomputed based on position deltas
	UPROPERTY(Category="(Radical Movement): Simulation Settings", Transient, VisibleInstanceOnly, BlueprintReadWrite)
	uint8 bJustTeleported								: 1;

	/// @brief  If true, high level movement updates will be wrapped in a movement scope that accumulates and defers a bulk of
	///			the work until the end. When enabled, touch & hit events will not be triggered until the end of multiple moves
	///			within an update, which can improve performance.
	///
	///			@see FScopedMovementUpdate
	UPROPERTY(Category="(Radical Movement): Simulation Settings", EditDefaultsOnly)
	uint8 bEnableScopedMovementUpdates					: 1;

	/// @brief  Flag set when SlideAlongSurface makes a non-zero movement. Used in determining how to recalculate velocity to reflect a move within a tick
	UPROPERTY(Transient)
	uint8 bSuccessfulSlideAlongSurface					: 1;
	
	/// @brief  Max delta time for each discrete simulation step in the movement simulation. Lowering the value can address issues with fast-moving
	///			objects or complex collision scenarios, at the cost of performance.
	///
	///			WARNING: If (MaxSimulationTimeStep * MaxSimulationIterations) is too low for the min framerate, the last simulation step may exceed
	///			MaxSimulationTimeStep to complete the simulation. @see MaxSimulationIterations
	UPROPERTY(Category= "(Radical Movement): Simulation Settings", EditDefaultsOnly, meta=(ClampMin="0.0166", ClampMax="0.50", UIMin="0.0166", UIMax="0.50"))
	float MaxSimulationTimeStep							= 0.05f;

	/// @brief  Max number of iterations used for each discrete simulation step in the movement simulation. Increasing the value can address issues with fast-moving
	///			objects or complex collision scenarios, at the cost of performance.
	///
	///			WARNING: If (MaxSimulationTimeStep * MaxSimulationIterations) is too low for the min framerate, the last simulation step may exceed
	///			MaxSimulationTimeStep to complete the simulation. @see MaxSimulationTimeStep
	UPROPERTY(Category= "(Radical Movement): Simulation Settings", EditDefaultsOnly, meta=(ClampMin="1", ClampMax="25", UIMin="1", UIMax="25"))
	float MaxSimulationIterations						= 8;

	/// @brief  Max distance we allow to depenetrate when moving out of anything but Pawns.
	///			This is generally more tolerant than with Pawns, because other geometry is static.
	///			@see MaxDepenetrationWithPawn
	UPROPERTY(Category= "(Radical Movement): Simulation Settings", EditDefaultsOnly, meta=(ClampMin="0", UIMin="0", ForceUnits=cm))
	float MaxDepenetrationWithGeometry					= 100.f;

	/// @brief  Max distance we allow to depenetrate when moving out of pawns.
	///			@see MaxDepenetrationWithGeometry
	UPROPERTY(Category= "(Radical Movement): Simulation Settings", EditDefaultsOnly, meta=(ClampMin="0", UIMin="0", ForceUnits=cm))
	float MaxDepenetrationWithPawn						= 100.f;

protected:
	
	/// @brief  
	void PerformMovement(float DeltaTime);

	/// @brief  
	/// @return True if we should continue with the movement updates
	virtual bool PreMovementUpdate(float DeltaTime);

	/// @brief  
	void StartMovementTick(float DeltaTime, uint32 Iterations);


	/// @brief  
	void GroundMovementTick(float DeltaTime, uint32 Iterations);

	/// @brief  
	void AirMovementTick(float DeltaTime, uint32 Iterations);

	/// @brief  
	virtual void PostMovementUpdate(float DeltaTime);

	/// @brief  
	/// @param  OutStepDownResult Result of a possible StepDown, can be used to compute the floor if valid
	virtual void MoveAlongFloor(const float DeltaTime, FStepDownFloorResult* OutStepDownResult = NULL);

	/// @brief  
	void ProcessLanded(FHitResult& Hit, float DeltaTime, uint32 Iterations);

	/// @brief  
	void StartFalling(uint32 Iterations, float RemainingTime, float IterTick, const FVector& Delta, const FVector SubLoc);

	float GetSimulationTimeStep(float DeltaTime, uint32 Iterations) const;

	void RevertMove(const FVector& OldLocation, UPrimitiveComponent* OldBase, const FVector& InOldBaseLocation, const FGroundingStatus& OldFloor, bool bFailMove);
	
	void OnStuckInGeometry(const FHitResult* Hit);

#pragma endregion Core Simulation Handling

/* Fields for ground stability & Methods for sweeping and evaluating floors */
#pragma region Ground Stability Handling
protected:

	/// @brief  Mode to define against what direction to test stability settings
	UPROPERTY(Category="(Radical Movement): Ground Settings", EditDefaultsOnly, BlueprintReadWrite)
	TEnumAsByte<EOrientationMode> StabilityOrientationMode; 

	/// @brief  Maximum ground slope angle relative to the actor up direction.
	UPROPERTY(Category= "(Radical Movement): Ground Settings", EditDefaultsOnly)
	float MaxStableSlopeAngle = 60.f;

	/// @brief	Additional ground probing distance, probe distance will be chosen as the maximum between this and @see MaxStepHeight
	UPROPERTY(Category="(Radical Movement): Ground Settings", EditDefaultsOnly, BlueprintReadWrite, AdvancedDisplay)
	float ExtraFloorProbingDistance = 20.f;

	/// @brief	Will always perform floor sweeps regardless of whether we are moving or not
	UPROPERTY(Category="(Radical Movement): Ground Settings", EditAnywhere, BlueprintReadWrite, AdvancedDisplay)
	uint8 bAlwaysCheckFloor					: 1;
	
	/// @brief	Performs floor checks as if the collision shape has a flat base. Avoids situations where the character slowly lowers off the
	///			side of a ledge (as their capsule 'balances' on the edge).
	UPROPERTY(Category="(Radical Movement): Ground Settings", EditAnywhere, BlueprintReadWrite, AdvancedDisplay)
	uint8 bUseFlatBaseForFloorChecks		: 1;

	/// @brief  Force the character (if on ground) to do a check for a valid floor even if it hasn't moved. Cleared after the next floor check.
	///			Normally if @see bAlwaysCheckFloor is false, floor checks are avoided unless certain conditions are met. This overrides that to force a floor check.
	UPROPERTY(Category="(Radical Movement): Ground Settings", VisibleInstanceOnly, BlueprintReadWrite, AdvancedDisplay)
	uint8 bForceNextFloorCheck				: 1;
	// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ //
public:
	UPROPERTY(Category="(Radical Movement): Ground Status", VisibleInstanceOnly, BlueprintReadOnly)
	FGroundingStatus CurrentFloor = FGroundingStatus();
	
	UPROPERTY(Category="(Radical Movement): Ground Status", VisibleInstanceOnly, BlueprintReadOnly)
	FGroundingStatus LastGroundingStatus = FGroundingStatus();

protected:
	/// @brief  Determines if the pawn can be considered stable on a given slope normal.
	/// @param  Hit Given collision hit result
	/// @return True if the pawn can be considered stable on a given slope normal
	bool IsFloorStable(const FHitResult& Hit) const;

	bool CheckFall(const FGroundingStatus& OldFloor, const FHitResult& Hit, const FVector& Delta, const FVector& OldLocation, float RemainingTime, float IterTick, uint32 Iterations, bool bMustUnground);
	
	void UpdateFloorFromAdjustment();
	void AdjustFloorHeight();
	bool IsWithinEdgeTolerance(const FVector& CapsuleLocation, const FVector& TestImpactPoint, const float CapsuleRadius) const;
	void ComputeFloorDist(const FVector& CapsuleLocation, float LineDistance, float SweepDistance, FGroundingStatus& OutFloorResult, float SweepRadius, const FHitResult* DownwardSweepResult = nullptr) const;
	void FindFloor(const FVector& CapsuleLocation, FGroundingStatus& OutFloorResult, bool bCanUseCachedLocation, const FHitResult* DownwardSweepResult = nullptr) const;
	bool FloorSweepTest(FHitResult& OutHit, const FVector& Start, const FVector& End, ECollisionChannel TraceChannel, const struct FCollisionShape& CollisionShape, const struct FCollisionQueryParams& Params, const struct FCollisionResponseParams& ResponseParams) const;

	bool IsValidLandingSpot(const FVector& CapsuleLocation, const FHitResult& Hit) const;
	bool ShouldCheckForValidLandingSpot(const FHitResult& Hit) const;

	void MaintainHorizontalGroundVelocity();

	void RecalculateVelocityToReflectMove(const FVector& OldLocation, const float DeltaTime);

#pragma endregion Ground Stability Handling

/* Main stair related shit */
#pragma region Step Handling
protected:
	/// @brief  Maximum height of a step which the pawn can climb.
	UPROPERTY(Category= "(Radical Movement): Step Settings", EditDefaultsOnly, BlueprintReadWrite)
	float MaxStepHeight = 50.f;

	bool CanStepUp(const FHitResult& StepHit) const;
	
	bool StepUp(const FVector& Orientation, const FHitResult& StepHit, const FVector& Delta, FStepDownFloorResult* OutStepDownResult = nullptr);

#pragma endregion Step Handling

/* Ledge & Perching shit, contains methods for evaluating denivelation handling and perch stability */
#pragma region Ledge Handling
protected:
	/// @brief  If false, owner won't be able to walk off a ledge - it'll be treated as if there was an invisible wall
	UPROPERTY(Category="(Radical Movement): Ledge Settings", EditDefaultsOnly, BlueprintReadWrite)
	uint8 bCanWalkOffLedges				: 1;

	/// @brief Used in determining if pawn is going off ledge.  If the ledge is "shorter" than this value then the pawn will be able to walk off it. 
	UPROPERTY(Category="(Radical Movement): Ledge Settings", EditAnywhere, BlueprintReadWrite, meta=(EditCondition="bCanWalkOffLedges", EditConditionHides, ForceUnits=cm))
	float LedgeCheckThreshold = 4.f;
	
	/// @brief  Prevents owner from perching on the edge of a surface if the contact is 'PerchRadiusThreshold' close to the edge of the capsule.
	///			NOTE: Will not switch to Aerial if they're within the MaxStepHeight of the surface below it (Assuming stable surface)
	UPROPERTY(Category="(Radical Movement): Ledge Settings", EditDefaultsOnly, BlueprintReadWrite, meta=(ClampMin="0", UIMin="0", ForceUnits="cm"))
	float PerchRadiusThreshold;
	
	/// @brief  When perching on a ledge, add this additional distance to MaxStepHeight when determining how high above a walkable floor we can perch.
	///			NOTE: MaxStepHeight is still enforced for perching, this is just an additional distance on top of that
	UPROPERTY(Category="(Radical Movement): Ledge Settings", EditDefaultsOnly, BlueprintReadWrite, meta=(ClampMin="0", UIMin="0", ForceUnits="cm"))
	float PerchAdditionalHeight;
	
	/// @brief  Flag to enable handling properly detecting ledge information and grounding status. Has a performance cost.
	UPROPERTY(Category="(Radical Movement): Ledge Settings", EditDefaultsOnly, BlueprintReadWrite)
	uint8 bLedgeAndDenivelationHandling : 1;
	
	/// @brief Prevents snapping to ground on ledges beyond a certain velocity.
	UPROPERTY(Category= "(Radical Movement): Ledge Settings", EditDefaultsOnly, BlueprintReadWrite, meta=(EditCondition = "bLedgeAndDenivelationHandling", EditConditionHides, ClampMin="0", UIMin="0", ForceUnits="cm/s"))
	float MaxVelocityForLedgeSnap = 0.f;

	/// @brief  Maximum downward slope angle DELTA that the actor can be subjected to and still be snapping to the ground
	UPROPERTY(Category= "(Radical Movement): Ledge Settings", EditDefaultsOnly, BlueprintReadWrite, meta=(EditCondition = "bLedgeAndDenivelationHandling", EditConditionHides, ClampMin="0", ClampMax="180", UIMin="0", UIMax="180"))
	float MaxStableDenivelationAngle = 180.f;

	/// @brief	Returns the distance from the edge of the capsule within which we don't allow the owner to perch on the edge of a surface
	/// @return Max of PerchRadiusThreshold and zero
	UFUNCTION(Category= "(Radical Movement): Ledge Settings", BlueprintGetter)
	FORCEINLINE float GetPerchRadiusThreshold() const { return FMath::Max(0.f, PerchRadiusThreshold); }

	/// @brief  Returns the radius within which we can stand on the edge of a surface without falling (If it's a stable surface).
	/// @return Capsule Radius - GetPerchRadiusThreshold()
	UFUNCTION(Category= "(Radical Movement): Ledge Settings", BlueprintGetter)
	float GetValidPerchRadius() const;

	bool ShouldCatchAir(const FGroundingStatus& OldFloor, const FGroundingStatus& NewFloor) const;
	
	bool ShouldComputePerchResult(const FHitResult& InHit, bool bCheckRadius = true) const;
	
	bool ComputePerchResult(const float TestRadius, const FHitResult& InHit, const float InMaxFloorDist, FGroundingStatus& OutPerchFloorResult) const;
	
	FORCEINLINE virtual bool CanWalkOffLedge() const { return bCanWalkOffLedges; }

	virtual bool CheckLedgeDirection(const FVector& OldLocation, const FVector& SideStep, const FVector& GravDir);

	virtual FVector GetLedgeMove(const FVector& OldLocation, const FVector& Delta, const FVector& GravityDir);

	void HandleWalkingOffLedge(const FVector& PreviousFloorImpactNormal, const FVector& PreviousFloorContactNormal, const FVector& PreviousLocation, float TimeDelta);

#pragma endregion Ledge Handling
	
/* Methods to evaluate the stability of a given hit or overlap */
#pragma region Stability Evaluations
public:
	/// @brief  Checks whether we are currently able to move
	/// @return True if we are not stuck in geometry or have valid data
	UFUNCTION(Category="(Radical Movement): Physics State", BlueprintCallable)
	bool CanMove() const;



#pragma endregion Stability Evaluations
	
/* Methods to adjust movement based on the stability of a hit or overlap */
#pragma region Collision Adjustments

protected:
	virtual FVector GetPenetrationAdjustment(const FHitResult& Hit) const override;
	virtual bool ResolvePenetrationImpl(const FVector& Adjustment, const FHitResult& Hit, const FQuat& NewRotation) override;

	virtual FVector ComputeSlideVector(const FVector& Delta, const float Time, const FVector& Normal, const FHitResult& Hit) const override;
	virtual FVector HandleSlopeBoosting(const FVector& SlideResult, const FVector& Delta, const float Time, const FVector& Normal, const FHitResult& Hit) const;
	
	virtual float SlideAlongSurface(const FVector& Delta, float Time, const FVector& InNormal, FHitResult& Hit, bool bHandleImpact) override;
	virtual void TwoWallAdjust(FVector& Delta, const FHitResult& Hit, const FVector& OldHitNormal) const override;
	virtual void HandleImpact(const FHitResult& Hit, float TimeSlice = 0.f, const FVector& MoveDelta = FVector::ZeroVector) override;

#pragma endregion Collision Adjustments
	
/* Methods and fields to handle root motion */
#pragma region Root Motion
protected:
	UPROPERTY(Category="(Radical Movement): Animation", EditDefaultsOnly)
	uint8 bApplyRootMotionDuringBlendIn		: 1;
	
	UPROPERTY(Category="(Radical Movement): Animation", EditDefaultsOnly)
	uint8 bApplyRootMotionDuringBlendOut	: 1;

	UPROPERTY(Transient)
	FRootMotionMovementParams RootMotionParams;

protected:
	/// @brief  Ticks the mesh pose and accumulates root motion
	void TickPose(float DeltaTime);

	virtual FVector CalcRootMotionVelocity(FVector RootMotionDeltaMove, float DeltaTime, const FVector& CurrentVelocity) const;

	bool ShouldDiscardRootMotion(const UAnimMontage* RootMotionMontage, float RootMotionMontagePosition) const;

public:

	/// @brief  Check to see if we have root motion from an Anim. Not valid outside of the scope of PerformMovement() since its extracted and used in it
	/// @return True if we have Root Motion from animation to use in PerformMovement() physics
	bool HasAnimRootMotion() const
	{
		return RootMotionParams.bHasRootMotion;
	}
	
/*
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

	UFUNCTION(BlueprintCallable)
	bool HasAnimRootMotion() const { return bHasAnimRootMotion; }

protected:

	
	void BlockSkeletalMeshPoseTick() const;

	void TickPose(float DeltaTime);

	virtual void ApplyAnimRootMotionRotation(float DeltaTime);

	virtual void CalculateAnimRootMotionVelocity(float DeltaTime);

	bool ShouldDiscardRootMotion(UAnimMontage* RootMotionMontage, float RootMotionMontagePosition) const;
	
	UFUNCTION(BlueprintNativeEvent, Category = "General Movement Component")
	FVector PostProcessAnimRootMotionVelocity(const FVector& RootMotionVelocity, float DeltaSeconds);
	virtual FVector PostProcessAnimRootMotionVelocity_Implementation(const FVector& RootMotionVelocity, float DeltaSeconds) {return RootMotionVelocity; }
*/
#pragma endregion Root Motion
	
/* Methods to impart forces and evaluate physics interactions*/
#pragma region Physics Interactions

protected:
	/* Wanna keep this setting exposed and read/writable */
	UPROPERTY(Category= "(Radical Movement): Physics Interactions", EditAnywhere, BlueprintReadWrite, BlueprintGetter="GetMass", meta = (ClampMin = "0", UIMin = "0"))
	float Mass{100.f};
	
	/// @brief  Whether the pawn should interact with physics objects in the world
	UPROPERTY(Category= "(Radical Movement): Physics Interactions", EditAnywhere, BlueprintReadWrite)
	uint8 bEnablePhysicsInteraction				: 1;

	/// @brief  If true, "TouchForceScale" is applied per kilogram of mass of the affected object.
	UPROPERTY(Category= "(Radical Movement): Physics Interactions", EditAnywhere, BlueprintReadWrite, meta=(EditCondition="bEnablePhysicsInteraction"))
	uint8 bTouchForceScaledToMass				: 1;

	UPROPERTY(Category= "(Radical Movement): Physics Interactions", EditAnywhere, BlueprintReadWrite, meta=(EditCondition="bEnablePhysicsInteraction"))
	uint8 bPushForceScaledToMass				: 1;

	UPROPERTY(Category= "(Radical Movement): Physics Interactions", EditAnywhere, BlueprintReadWrite, meta=(EditCondition="bEnablePhysicsInteraction"))
	uint8 bPushForceUsingVerticalOffset			: 1;

	UPROPERTY(Category= "(Radical Movement): Physics Interactions", EditAnywhere, BlueprintReadWrite, meta=(EditCondition="bEnablePhysicsInteraction"))
	uint8 bScalePushForceToVelocity				: 1;

	UPROPERTY(Category= "(Radical Movement): Physics Interactions", EditAnywhere, BlueprintReadWrite, meta=(EditCondition="bEnablePhysicsInteraction"))
	float RepulsionForce{2.5f};
	
	/// @brief  Multiplier for the force that is applied to physics objects that are touched by the pawn.
	UPROPERTY(Category= "(Radical Movement): Physics Interactions", EditAnywhere, BlueprintReadWrite, meta=(EditCondition="bEnablePhysicsInteraction"))
	float TouchForceFactor{1.f};

	/// @brief  The minimum force applied to physics objects touched by the pawn.
	UPROPERTY(Category= "(Radical Movement): Physics Interactions", EditAnywhere, BlueprintReadWrite, meta=(EditCondition="bEnablePhysicsInteraction"))
	float MinTouchForce{-1.f};

	/// @brief  The maximum force applied to physics objects touched by the pawn.
	UPROPERTY(Category= "(Radical Movement): Physics Interactions", EditAnywhere, BlueprintReadWrite, meta=(EditCondition="bEnablePhysicsInteraction"))
	float MaxTouchForce{250.f};

	/// @brief  
	UPROPERTY(Category= "(Radical Movement): Physics Interactions", EditAnywhere, BlueprintReadWrite, meta=(EditCondition="bEnablePhysicsInteraction"))
	float StandingDownwardForceScale{1.f};

	/// @brief  
	UPROPERTY(Category= "(Radical Movement): Physics Interactions", EditAnywhere, BlueprintReadWrite, meta=(EditCondition="bEnablePhysicsInteraction"))
	float InitialPushForceFactor{500.f};

	/// @brief  
	UPROPERTY(Category= "(Radical Movement): Physics Interactions", EditAnywhere, BlueprintReadWrite, meta=(EditCondition="bEnablePhysicsInteraction"))
	float PushForceFactor{750000.f};

	/// @brief  
	UPROPERTY(Category= "(Radical Movement): Physics Interactions", EditAnywhere, BlueprintReadWrite, meta=(EditCondition="bEnablePhysicsInteraction", UIMin="-1.0", UIMax="1.0"))
	float PushForcePointVerticalOffsetFactor{-0.75f};

	
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

	virtual void ApplyImpactPhysicsForces(const FHitResult& Impact, const FVector& ImpactAcceleration, const FVector& ImpactVelocity);
	virtual void ApplyRepulsionForce(float DeltaTime);
	virtual void ApplyDownwardForce(float DeltaTime);

public:
	UFUNCTION(Category="(Radical Movement): Physics Interactions", BlueprintCallable)
	float GetMass() const { return Mass; };

#pragma endregion Physics Interactions

/* Methods to handle moving bases */
#pragma region Moving Base

protected:

	/* ~~~~~ Based Movement Core Settings ~~~~~ */
	
	/// @brief  Whether to move with the platform/base relatively
	UPROPERTY(Category= "(Radical Movement): Movement Base Settings", EditAnywhere, BlueprintReadWrite)
	uint8 bMoveWithBase					: 1;

	UPROPERTY(Category= "(Radical Movement): Movement Base Settings", EditAnywhere, BlueprintReadWrite, meta=(EditCondition="bMoveWithBase", EditConditionHides, ClampMin="0", UIMin="0"))
	float FormerBaseVelocityDecayHalfLife = 0.f;
	
	/**
	* Whether the character ignores changes in rotation of the base it is standing on.
	* If true, the character maintains current world rotation.
	* If false, the character rotates with the moving base.
	*/
	UPROPERTY(Category="(Radical Movement): Movement Base Settings", EditAnywhere, BlueprintReadWrite, meta=(EditCondition="bMoveWithBase", EditConditionHides))
	uint8 bIgnoreBaseRotation			: 1;
	
	/** If true, impart the base actor's X velocity when falling off it (which includes jumping) */
	UPROPERTY(Category= "(Radical Movement): Movement Base Settings", EditAnywhere, BlueprintReadWrite, meta=(EditCondition="bMoveWithBase", EditConditionHides))
	uint8 bImpartBaseVelocityPlanar		: 1;

	/** If true, impart the base actor's Y velocity when falling off it (which includes jumping) */
	UPROPERTY(Category= "(Radical Movement): Movement Base Settings", EditAnywhere, BlueprintReadWrite, meta=(EditCondition="bMoveWithBase", EditConditionHides))
	uint8 bImpartBaseVelocityVertical	: 1;

	/// @brief  If true, impart the base component's tangential components of angular velocity when jumping or falling off it.
	///			May be restricted by other base velocity impart settings @see bImpartBaseVelocityVertical, bImpartBaseVelocityPlanar
	UPROPERTY(Category="(Radical Movement): Movement Base Settings", EditAnywhere, BlueprintReadWrite, meta=(EditCondition="bMoveWithBase", EditConditionHides))
	uint8 bImpartBaseAngularVelocity	: 1;

	/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
	
	/*~~~~~ Based Movement Bookkeeping ~~~~~*/

	/// @brief  Flag set in pre-physics update to indicate that based movement should be updated in post-physics
	uint8 bDeferUpdateBasedMovement		: 1;
	
	/// @brief  Saved location of object we are standing on for UpdatedBasedMovement() to determine if based moved in the last frame and therefore
	///			pawn needs an update
	FVector OldBaseLocation;

	/// @brief  Saved rotation of object we are standing on for UpdatedBasedMovement() to determine if based moved in the last frame and therefore
	///			pawn needs an update
	FQuat OldBaseQuat;

	FVector DecayingFormerBaseVelocity = FVector::ZeroVector;

	/*~~~~~ Based Movement Methods ~~~~~*/

public:
	/// @brief  Sets the component the pawn is walking on/following
	void SetBase(UPrimitiveComponent* NewBaseComponent, const FName InBoneName = NAME_None, bool bNotifyPawn = true);

	/// @brief	Update the base of the character using the given floor result if it is stable, or null if not. Calls @see SetBase()  
	void SetBaseFromFloor(const FGroundingStatus& FloorResult);
	
	/// @brief  Returns the current movement base, could be just the static floor or the override movement base
	/// @return	The primitive component of the movement base
	UFUNCTION(Category="Motor | Movement Base", BlueprintCallable)
	UPrimitiveComponent* GetMovementBase() const; // TODO: Let Owner call this, mainly to account for sneaking in the Override if it exists

	/// @brief  If we have a movement base, get the velocity that should be imparted by that base (usually when jumping off of it)
	///			that respect the impart velocity settings (@see bImpartBaseVelocityVertical, bImpartBaseVelocityPlanar)
	/// @return The total velocity imparted by the movement base
	UFUNCTION(Category="Motor | Movement Base", BlueprintCallable)
	virtual FVector GetImpartedMovementBaseVelocity() const;

	virtual void SaveBaseLocation();
	
protected:
	void DecayFormerBaseVelocity(float DeltaTime);
	
	/// @brief  Update position based on Based movement
	virtual void TryUpdateBasedMovement(float DeltaTime);

	virtual void UpdateBasedMovement(float DeltaTime);
	
	virtual void UpdateBasedRotation(FRotator& FinalRotation, const FRotator& ReducedRotation);
	
	/// @brief  Event triggered when we are moving on a base but are unable to move the full DeltaPosition because something blocked us
	/// @param  DeltaPosition	How far we tried to move WITH the base
	/// @param  OldPosition		Location before we tried to move with the base
	/// @param  MoveOnBaseHit	Hit result for the object we hit when trying to move with the base
	virtual void OnUnableToFollowBaseMove(const FVector& DeltaPosition, const FVector& OldPosition, const FHitResult& MoveOnBaseHit);
	
#pragma endregion Moving Base


#pragma region AI Path Following & RVO

	UPROPERTY(Transient)
	FVector RequestedVelocity;

	UPROPERTY(Transient)
	FVector LastUpdateRequestedVelocity;

	FVector GetLastUpdateRequestedVelocity() const { return LastUpdateRequestedVelocity; }
	
// BEGIN UNavMovementComponent Interface
	virtual void RequestDirectMove(const FVector& MoveVelocity, bool bForceMaxSpeed) override { Super::RequestDirectMove(MoveVelocity, bForceMaxSpeed); };
	virtual void RequestPathMove(const FVector& MoveInput) override {Super::RequestPathMove(MoveInput); };
	virtual bool CanStartPathFollowing() const override {return Super::CanStartPathFollowing(); };
	virtual bool CanStopPathFollowing() const override { return Super::CanStopPathFollowing(); };
	virtual float GetPathFollowingBrakingDistance(float MaxSpeed) const override {return Super::GetPathFollowingBrakingDistance(MaxSpeed);};
// END UNavMovementComponent Interface

	//virtual bool ApplyRequestedMove(float DeltaTime, float MaxAccel, float MaxSpeed, float Friction, float BrakingDeceleration, FVector& OutAcceleration);
	//virtual bool ShouldComputeAccelerationToReachRequestedVelocity(const float RequestedSpeed) const;
	//virtual void PerformAirControlForPathFollowing(FVector Direction, float ZDiff);
	//virtual void ShouldPerformAirControlForPathFollowing() const;
#pragma endregion AI Path Following & RVO
	
/* Math shit or whatever helpers */
#pragma region Utility

	void VisualizeMovement() const;

public:
	/// @brief  Draw important variables on canvas. Character will call DisplayDebug() on the current view target when the ShowDebug exec is used
	/// @param  Canvas Canvas to draw on
	/// @param  DebugDisplay Contains information about what debug data to display
	/// @param  YL Height of the current font
	/// @param  YPos Y position on Canvas. YPos += YL, gives position to draw text for next debug line
	virtual void DisplayDebug(class UCanvas* Canvas, const FDebugDisplayInfo& DebugDisplay, float& YL, float& YPos);


	FORCEINLINE FVector GetDirectionTangentToSurface(const FVector& Direction, const FVector& SurfaceNormal) const
	{
		const FVector DirectionRight = Direction ^ UpdatedComponent->GetUpVector();
		return (SurfaceNormal ^ DirectionRight).GetSafeNormal();
	}

	FORCEINLINE float DistanceAlongAxis(const FVector& From, const FVector& To, const FVector& Axis) const 
	{
		return (To - From) | Axis;
	}

#pragma endregion Utility
	
};

