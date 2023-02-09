// Copyright 2023 Abdulrahmen Almodaimegh. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PawnMovementComponent.h"
#include "CustomMovementComponent.generated.h"

#define CM_DEBUG_BREAK GEngine->DeferredCommands.Add(TEXT("pause"));

/* Profiling */
DECLARE_STATS_GROUP(TEXT("RadicalMovementComponent_Game"), STATGROUP_RadicalMovementComp, STATCAT_Advanced)
/* ~~~~~~~~ */

#pragma region Floor Data Structs

USTRUCT(BlueprintType)
struct COREFRAMEWORK_API FGroundingStatus
{
	GENERATED_USTRUCT_BODY()

	/// @brief  True if there was a blocking hit in the floor test that was NOT in initial penetration.
	///			The hit result can give more info about other circumstances
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = CharacterFloor)
	uint32 bBlockingHit		: 1;

	/// @brief  True if the hit found a valid stable, walkable floor
	uint32 bWalkableFloor	: 1;

	/// @brief  True if bBlockingHit && !bWalkableFloor. A floor was found but it is not stable
	uint32 bUnstableFloor	: 1;

	/// @brief  True if the hit found a floor using a line trace (rather than a sweep trace, which happens when the sweep test fails to yield a valid floor)
	uint32 bLineTrace		: 1;

	/// @brief  The distance to the floor, computed from the swept capsule trace
	float FloorDist;

	/// @brief  The distance to the floor, computed from the line trace. Only valid if bLineTrace is true
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

			// Restore some of the old values. We want the new normals and hit actor, however.
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

struct FStepDownResult
{
	uint32 bComputedFloor : 1;		// True if the floor was computed as a result of the step down
	FGroundingStatus FloorResult;

	FStepDownResult() : bComputedFloor(false) {}
};

#pragma endregion Floor Data Structs

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

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	virtual void PrePhysicsTickComponent(float DeltaTime);
	virtual void PostPhysicsTickComponent(float DeltaTime);

	/* Overriding methods from parent */
#pragma region Movement Component Overrides

	// BEGIN UMovementComponent Interface
	virtual void SetUpdatedComponent(USceneComponent* NewUpdatedComponent) override;
	FORCEINLINE virtual bool IsMovingOnGround() const override { return CurrentFloor.bWalkableFloor; }
	FORCEINLINE virtual bool IsFalling() const override { return !CurrentFloor.bBlockingHit; }
	virtual void AddRadialForce(const FVector& Origin, float Radius, float Strength, ERadialImpulseFalloff Falloff) override;
	virtual void AddRadialImpulse(const FVector& Origin, float Radius, float Strength, ERadialImpulseFalloff Falloff, bool bVelChange) override;
	virtual void StopActiveMovement() override;			// Check CMC
	virtual void StopMovementImmediately() override;	// Check CMC
	// END UMovementComponent Interface

#pragma endregion Movement Component Overrides

#pragma region Ground Stability Handling

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Motor | Ground Settings")
	uint8 bSolveGrounding				: 1;
	
	/// @brief  Extra probing distance for ground detection.
	UPROPERTY(EditDefaultsOnly, Category= "Motor | Ground Settings", meta=(EditCondition="bSolveGrounding", EditConditionHides))
	float GroundDetectionExtraDistance = 0.f;

	/// @brief  Maximum ground slope angle relative to the actor up direction.
	UPROPERTY(EditDefaultsOnly, Category= "Motor | Ground Settings", meta=(EditCondition="bSolveGrounding", EditConditionHides))
	float MaxStableSlopeAngle = 60.f;
	
	/// @brief	Performs floor checks as if the collision shape has a flat base. Avoids situations where the character slowly lowers off the
	///			side of a ledge (as their capsule 'balances' on the edge).
	UPROPERTY(Category="Motor | Ground Settings", EditAnywhere, BlueprintReadWrite, AdvancedDisplay)
	uint8 bUseFlatBaseForFloorChecks	: 1;

	/// @brief  Force the character (if on ground) to do a check for a valid floor even if it hasn't moved. Cleared after the next floor check.
	///			Normally if @see bAlwaysCheckFloor is false, floor checks are avoided unless certain conditions are met. This overrides that to force a floor check.
	UPROPERTY(Category="Motor | Ground Settings", VisibleInstanceOnly, BlueprintReadWrite, AdvancedDisplay)
	uint8 bForceNextFloorCheck			: 1;

	//~~~~~ Ground Extra Settings ~~~~~ //
	UPROPERTY(Category = "Motor | Ground Settings", EditDefaultsOnly, AdvancedDisplay)
	bool bEnableGlobalAngleRestriction;

	UPROPERTY(Category = "Motor | Ground Settings", EditDefaultsOnly, AdvancedDisplay, meta=(EditCondition="bEnableGlobalAngleRestriction", EditConditionHides))
	FVector RelativeDirection;

	UPROPERTY(Category = "Motor | Ground Settings", EditDefaultsOnly, AdvancedDisplay, meta=(EditCondition="bEnableGlobalAngleRestriction", EditConditionHides))
	float MaxStableSlongAngleInRelativeDirection;
	// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ //

	UPROPERTY(Category="Motor | Ground Status", VisibleInstanceOnly, BlueprintReadOnly)
	FGroundingStatus CurrentFloor = FGroundingStatus();
	
	UPROPERTY(Category="Motor | Ground Status", VisibleInstanceOnly, BlueprintReadOnly)
	FGroundingStatus LastGroundingStatus = FGroundingStatus();

	bool bMustUnground;

	FORCEINLINE bool MustUnground() const { return bMustUnground; }
	
	/// @brief  Determines if the pawn can be considered stable on a given slope normal.
	/// @param  Normal Given ground normal
	/// @return True if the pawn can be considered stable on a given slope normal
	bool IsFloorStable(FVector Normal) const;
	
	void UpdateFloorFromAdjustment();
	void AdjustFloorHeight();
	bool IsWithinEdgeTolerance(const FVector& CapsuleLocation, const FVector& TestImpactPoint, const float CapsuleRadius) const;
	void ComputeFloorDist(const FVector& CapsuleLocation, float LineDistance, float SweepDistance, FGroundingStatus& OutFloorResult, float SweepRadius, const FHitResult* DownwardSweepResult) const;
	void FindFloor(const FVector& CapsuleLocation, FGroundingStatus& OutFloorResult, bool bCanUseCachedLocation, const FHitResult* DownwardSweepResult = NULL) const;
	bool FloorSweepTest(FHitResult& OutHit, const FVector& Start, const FVector& End, ECollisionChannel TraceChannel, const struct FCollisionShape& CollisionShape, const struct FCollisionQueryParams& Params, const struct FCollisionResponseParams& ResponseParams) const;

	bool IsValidLandingSpot(const FVector& CapsuleLocation, const FHitResult& Hit) const;
	bool ShouldCheckForValidLandingSpot(const FHitResult& Hit) const;

#pragma endregion Ground Stability Handling

#pragma region Step Handling

	/// @brief  Handles properly detecting grounding status on step, but has a performance cost.
	UPROPERTY(Category= "Motor | Step Settings", EditDefaultsOnly)
	uint8 bSolveSteps : 1;

	/// @brief  Can the pawn step up obstacles even if it is not currently on stable ground (e.g from air)?
	UPROPERTY(Category= "Motor | Step Settings", EditDefaultsOnly, meta=(EditCondition = "bSolveSteps", EditConditionHides))
	uint8 bAllowSteppingWithoutStableGrounding : 1;
	
	/// @brief  Maximum height of a step which the pawn can climb.
	UPROPERTY(Category= "Motor | Step Settings", EditDefaultsOnly, meta=(EditCondition = "bSolveSteps", EditConditionHides))
	float MaxStepHeight = 50.f;

	bool CanStepUp(const FHitResult& StepHit) const;
	
	bool StepUp(const FHitResult& StepHit, const FVector& Delta, FStepDownResult* OutStepDownResult = nullptr);

#pragma endregion Step Handling

#pragma region Ledge Settings

	/// @brief  If false, owner won't be able to walk off a ledge - it'll be treated as if there was an invisible wall
	UPROPERTY(Category="Motor | Ledge Settings", EditDefaultsOnly, BlueprintReadWrite)
	uint8 bCanWalkOffLedges				: 1;

	/// @brief  Prevents owner from perching on the edge of a surface if the contact is 'PerchRadiusThreshold' close to the edge of the capsule.
	///			NOTE: Will not switch to Aerial if they're within the MaxStepHeight of the surface below it (Assuming stable surface)
	UPROPERTY(Category="Motor | Ledge Settings", EditDefaultsOnly, BlueprintReadWrite, meta=(ClampMin="0", UIMin="0", ForceUnits="cm"))
	float PerchRadiusThreshold;
	
	/// @brief  When perching on a ledge, add this additional distance to MaxStepHeight when determining how high above a walkable floor we can perch.
	///			NOTE: MaxStepHeight is still enforced for perching, this is just an additional distance on top of that
	UPROPERTY(Category="Motor | Ledge Settings", EditDefaultsOnly, BlueprintReadWrite, meta=(ClampMin="0", UIMin="0", ForceUnits="cm"))
	float PerchAdditionalHeight;
	
	/// @brief  Flag to enable handling properly detecting ledge information and grounding status. Has a performance cost.
	UPROPERTY(Category="Motor | Ledge Settings", EditDefaultsOnly, BlueprintReadWrite)
	uint8 bLedgeAndDenivelationHandling : 1;
	
	/// @brief Prevents snapping to ground on ledges beyond a certain velocity.
	UPROPERTY(Category= "Motor | Ledge Settings", EditDefaultsOnly, BlueprintReadWrite, meta=(EditCondition = "bLedgeAndDenivelationHandling", EditConditionHides, ClampMin="0", UIMin="0", ForceUnits="cm/s"))
	float MaxVelocityForLedgeSnap = 0.f;

	/// @brief  Maximum downward slope angle DELTA that the actor can be subjected to and still be snapping to the ground
	UPROPERTY(Category= "Motor | Ledge Settings", EditDefaultsOnly, BlueprintReadWrite, meta=(EditCondition = "bLedgeAndDenivelationHandling", EditConditionHides, ClampMin="0", ClampMax="180", UIMin="0", UIMax="180"))
	float MaxStableDenivelationAngle = 180.f;

	/// @brief	Returns the distance from the edge of the capsule within which we don't allow the owner to perch on the edge of a surface
	/// @return Max of PerchRadiusThreshold and zero
	UFUNCTION(Category= "Motor | Ledge Settings", BlueprintGetter)
	FORCEINLINE float GetPerchRadiusThreshold() const { return FMath::Max(0.f, PerchRadiusThreshold); }

	/// @brief  Returns the radius within which we can stand on the edge of a surface without falling (If it's a stable surface).
	/// @return Capsule Radius - GetPerchRadiusThreshold()
	UFUNCTION(Category= "Motor | Ledge Settings", BlueprintGetter)
	FORCEINLINE float GetValidPerchRadius() const
	{
		const float PawnRadius = UpdatedPrimitive->GetCollisionShape().GetCapsuleRadius();
		return FMath::Clamp(PawnRadius - GetPerchRadiusThreshold(), 0.11f, PawnRadius);
	};

	bool ShouldCatchAir(const FGroundingStatus& OldFloor, const FGroundingStatus& NewFloor);

	bool ShouldComputePerchResult(const FHitResult& InHit, bool bCheckRadius) const;
	
	bool ComputePerchResult(const float TestRadius, const FHitResult& InHit, const float InMaxFloorDist, FGroundingStatus& OutPerchFloorResult) const;

#pragma endregion Ledge Settings


#pragma region Simulation Parameters

	/// @brief  Used within movement code to determine if a change in position is based on normal movement or a teleport. If not a teleport,
	///			velocity can be recomputed based on position deltas
	UPROPERTY(Category="Motor | Simulation Settings", Transient, VisibleInstanceOnly, BlueprintReadWrite)
	uint8 bJustTeleported								: 1;

	/// @brief  Max delta time for each discrete simulation step in the movement simulation. Lowering the value can address issues with fast-moving
	///			objects or complex collision scenarios, at the cost of performance.
	///
	///			WARNING: If (MaxSimulationTimeStep * MaxSimulationIterations) is too low for the min framerate, the last simulation step may exceed
	///			MaxSimulationTimeStep to complete the simulation. @see MaxSimulationIterations
	UPROPERTY(Category= "Motor | Simulation Settings", EditDefaultsOnly, meta=(ClampMin="0.0166", ClampMax="0.50", UIMin="0.0166", UIMax="0.50"))
	float MaxSimulationTimeStep							= 0.05f;

	/// @brief  Max number of iterations used for each discrete simulation step in the movement simulation. Increasing the value can address issues with fast-moving
	///			objects or complex collision scenarios, at the cost of performance.
	///
	///			WARNING: If (MaxSimulationTimeStep * MaxSimulationIterations) is too low for the min framerate, the last simulation step may exceed
	///			MaxSimulationTimeStep to complete the simulation. @see MaxSimulationTimeStep
	UPROPERTY(Category= "Motor | Simulation Settings", EditDefaultsOnly, meta=(ClampMin="1", ClampMax="25", UIMin="1", UIMax="25"))
	float MaxSimulationIterations						= 8;

	/// @brief  Max distance we allow to depenetrate when moving out of anything but Pawns.
	///			This is generally more tolerant than with Pawns, because other geometry is static.
	///			@see MaxDepenetrationWithPawn
	UPROPERTY(Category= "Motor | Simulation Settings", EditDefaultsOnly, meta=(ClampMin="0", UIMin="0", ForceUnits=cm))
	float MaxDepenetrationWithGeometry					= 100.f;

	/// @brief  Max distance we allow to depenetrate when moving out of pawns.
	///			@see MaxDepenetrationWithGeometry
	UPROPERTY(Category= "Motor | Simulation Settings", EditDefaultsOnly, meta=(ClampMin="0", UIMin="0", ForceUnits=cm))
	float MaxDepenetrationWithPawn						= 100.f;
	
#pragma region Const Simulation Parameters

private:
	
	/* CONSTANTS FOR SWEEP AND PHYSICS CALCULATIONS */
	static constexpr float MIN_TICK_TIME				= 1e-6f;
	static constexpr float MIN_FLOOR_DIST				= 1.9f;
	static constexpr float MAX_FLOOR_DIST				= 2.4f;
	static constexpr float SWEEP_EDGE_REJECT_DISTANCE	= 0.15f;

#pragma endregion Const Simulation Parameters

#pragma endregion Simulation Parameters


/* The core update loop of the movement component */
#pragma region Core Update Loop 

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
	virtual void MoveAlongFloor(const FVector& InVelocity, float DeltaTime, FStepDownResult* OutStepDownResult = NULL);

	/// @brief  
	void StartLanding(float DeltaTime, uint32 Iterations);

	/// @brief  
	void StartFalling(float DeltaTime, uint32 Iterations);


#pragma endregion Core Update Loop


#pragma region Exposed Calls
protected:

	/* Physics State Parameters */
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

	/// @brief  Unground and prevent snapping to allow for leaving ground
	UFUNCTION(Category="Motor | Physics State", BlueprintCallable)
	FORCEINLINE void ForceUnground() { bMustUnground = true; }

	/// @brief  Get the velocity required to move [MoveDelta] amount in [DeltaTime] time.
	/// @param  MoveDelta Movement Delta To Compute Velocity
	/// @param  DeltaTime Delta Time In Which Move Would Be Performed
	/// @return Velocity corresponding to MoveDelta/DeltaTime if valid delta time was passed through
	UFUNCTION(Category="Motor | Physics State", BlueprintCallable)
	FORCEINLINE FVector GetVelocityFromMovement(FVector MoveDelta, float DeltaTime) const 
	{
		if (DeltaTime <= 0.f) return FVector::ZeroVector;
		return MoveDelta / DeltaTime;
	}
	
	UFUNCTION(Category="Motor | Physics State", BlueprintCallable)
	FORCEINLINE FVector GetVelocity() const { return Velocity; }

	UFUNCTION(Category="Motor | Physics State", BlueprintCallable)
	FORCEINLINE void SetVelocity(const FVector TargetVelocity) { Velocity = TargetVelocity; } 

	UFUNCTION(Category="Motor | Physics State", BlueprintCallable)
	FORCEINLINE bool IsStableOnGround() const { return IsMovingOnGround(); }

	UFUNCTION(Category="Motor | Physics State", BlueprintCallable)
	FORCEINLINE bool IsUnstableOnGround() const { return CurrentFloor.bUnstableFloor; }

	UFUNCTION(Category="Motor | Physics State", BlueprintCallable)
	FORCEINLINE bool IsInAir() const { return !CurrentFloor.bBlockingHit; }

#pragma region Events

	/// @brief Entry point for gameplay manipulation of rotation via blueprints or child class
	/// @param CurrentRotation Reference to current rotation to modify
	/// @param DeltaTime Current sub-step delta time
	UFUNCTION(Category="Motor | Movement Controller", BlueprintNativeEvent)
	void UpdateRotation(FQuat& CurrentRotation, float DeltaTime);
	virtual void UpdateRotation_Implementation(FQuat& CurrentRotation, float DeltaTime) {};

	/// @brief	Entry point for gameplay manipulation of velocity via blueprints or child class
	///			Velocity should only be modified through here as the order is important to what updates come after
	/// @param	CurrentVelocity Reference to current velocity to modify
	/// @param	DeltaTime Current sub-step delta time
	UFUNCTION(Category="Motor | Movement Controller", BlueprintNativeEvent)
	void UpdateVelocity(FVector& CurrentVelocity, float DeltaTime);
	virtual void UpdateVelocity_Implementation(FVector& CurrentVelocity, float DeltaTime) {};

	UFUNCTION(Category="Motor | Movement Controller", BlueprintNativeEvent)
	void SubsteppedTick(FVector& CurrentVelocity, float DeltaTime);
	virtual void SubsteppedTick_Implementation(FVector& CurrentVelocity, float DeltaTime) {};

#pragma endregion Events

#pragma endregion Exposed Calls

	
/* Methods to evaluate the stability of a given hit or overlap */
#pragma region Stability Evaluations

	/// @brief  Checks whether we are currently able to move
	/// @return True if we are not stuck in geometry or have valid data
	bool CanMove() const;



#pragma endregion Stability Evaluations


/* Methods to adjust movement based on the stability of a hit or overlap */
#pragma region Collision Adjustments

	virtual FVector GetPenetrationAdjustment(const FHitResult& Hit) const override;
	virtual bool ResolvePenetrationImpl(const FVector& Adjustment, const FHitResult& Hit, const FQuat& NewRotation) override;

	virtual float SlideAlongSurface(const FVector& Delta, float Time, const FVector& Normal, FHitResult& Hit, bool bHandleImpact) override;
	virtual void TwoWallAdjust(FVector& Delta, const FHitResult& Hit, const FVector& OldHitNormal) const override;
	virtual void HandleImpact(const FHitResult& Hit, float TimeSlice = 0.f, const FVector& MoveDelta = FVector::ZeroVector) override;

#pragma endregion Collision Adjustments

	
	
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

	/* ~~~~~ Based Movement Core Settings ~~~~~ */
	
	/// @brief  Whether to move with the platform/base relatively
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category= "Motor | Movement Base Settings")
	uint8 bMoveWithBase					: 1;

	/**
	* Whether the character ignores changes in rotation of the base it is standing on.
	* If true, the character maintains current world rotation.
	* If false, the character rotates with the moving base.
	*/
	UPROPERTY(Category="Motor | Movement Base Settings", EditAnywhere, BlueprintReadWrite)
	uint8 bIgnoreBaseRotation			: 1;
	
	/** If true, impart the base actor's X velocity when falling off it (which includes jumping) */
	UPROPERTY(Category= "Motor | Movement Base Settings", EditAnywhere, BlueprintReadWrite)
	uint8 bImpartBaseVelocityPlanar		: 1;

	/** If true, impart the base actor's Y velocity when falling off it (which includes jumping) */
	UPROPERTY(Category= "Motor | Movement Base Settings", EditAnywhere, BlueprintReadWrite)
	uint8 bImpartBaseVelocityVertical	: 1;

	/// @brief  If true, impart the base component's tangential components of angular velocity when jumping or falling off it.
	///			May be restricted by other base velocity impart settings @see bImpartBaseVelocityVertical, bImpartBaseVelocityPlanar
	UPROPERTY(Category="Motor | Movement Base Settings", EditAnywhere, BlueprintReadWrite)
	uint8 bImpartBaseAngularVelocity	: 1;

	/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
	
	/*~~~~~ Based Movement Bookkeeping ~~~~~*/
	FQuat OldBaseQuat;
	FVector OldBaseLocation;
	FVector DecayingFormerBaseVelocity = FVector::ZeroVector;

	// TODO: Do we want to consider bConsiderMassOnImpartVelocity?
	
	UPROPERTY(BlueprintSetter="SetMovementBaseOverride")
	UPrimitiveComponent* MovementBaseOverride{nullptr};

	void TryUpdateBasedMovement(float DeltaTime);

	void MovementBaseUpdate(float DeltaTime);

	/// @brief Set a moving base to keep, useful for things like following an elevator where when you jump, you still
	///			want to follow it
	/// @param BaseOverride Override any moving bases with the one passed through here
	UFUNCTION(BlueprintSetter)
	void SetMovementBaseOverride(UPrimitiveComponent* BaseOverride) {MovementBaseOverride = BaseOverride;}

#pragma endregion Moving Base


#pragma region Utility

	FVector GetObstructionNormal(const FVector HitNormal, const bool bStableOnHit) const;
	FVector GetDirectionTangentToSurface(const FVector Direction, const FVector SurfaceNormal) const;

#pragma endregion Utility

	
};

FORCEINLINE FVector UCustomMovementComponent::GetObstructionNormal(const FVector HitNormal, const bool bStableOnHit) const
{
	FVector ObstructionNormal = HitNormal;

	if (CurrentFloor.bWalkableFloor && !MustUnground() && !bStableOnHit)
	{
		const FVector ObstructionLeftAlongGround = (CurrentFloor.HitResult.Normal ^ ObstructionNormal).GetSafeNormal();
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


