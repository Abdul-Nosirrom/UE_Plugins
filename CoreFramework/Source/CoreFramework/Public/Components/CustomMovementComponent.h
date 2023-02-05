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
	FGroundingStatus CurrentFloor = FGroundingStatus();
	UPROPERTY(BlueprintReadOnly, Category="Motor | Ground Status")
	FGroundingStatus LastGroundingStatus = FGroundingStatus();

	UPROPERTY(BlueprintReadOnly, Category="Motor | Ground Status")
	bool bLastMovementIterationFoundAnyGround;

	UPROPERTY(BlueprintReadOnly, Category="Motor | Ground Status")
	bool bStuckInGeometry;

	bool bMustUnground;

#pragma endregion Ground Stability Settings

#pragma region Step Settings

	/// @brief  Handles properly detecting grounding status on step, but has a performance cost.
	UPROPERTY(EditDefaultsOnly, Category= "Motor | Step Settings")
	uint8 bSolveSteps : 1;

	/// @brief  Can the pawn step up obstacles even if it is not currently on stable ground (e.g from air)?
	UPROPERTY(EditDefaultsOnly, Category= "Motor | Step Settings", meta=(EditCondition = "bSolveSteps", EditConditionHides))
	uint8 bAllowSteppingWithoutStableGrounding : 1;
	
	/// @brief  Maximum height of a step which the pawn can climb.
	UPROPERTY(EditDefaultsOnly, Category= "Motor | Step Settings", meta=(EditCondition = "bSolveSteps", EditConditionHides))
	float MaxStepHeight = 50.f;

#pragma endregion Step Settings

#pragma region Ledge Settings
	
	/// @brief  Flag to enable handling properly detecting ledge information and grounding status. Has a performance cost.
	UPROPERTY(EditDefaultsOnly, Category= "Motor | Ledge Settings")
	uint8 bLedgeAndDenivelationHandling : 1;

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

#pragma endregion Core Movement Parameters

#pragma region Simulation Parameters
	
	UPROPERTY(EditDefaultsOnly, Category= "Motor | Simulation Settings")
	float MaxSimulationTimeStep							= 0.05f;
	UPROPERTY(EditDefaultsOnly, Category= "Motor | Simulation Settings")
	float MaxSimulationIterations						= 8;
	UPROPERTY(EditDefaultsOnly, Category= "Motor | Simulation Settings")
	float MaxDepenetrationWithGeometry					= 100.f;
	UPROPERTY(EditDefaultsOnly, Category= "Motor | Simulation Settings")
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


	/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
	
public:

	// BEGIN UMovementComponent Interface
	FORCEINLINE virtual bool IsMovingOnGround() const override { return CurrentFloor.bWalkableFloor; }
	FORCEINLINE virtual bool IsFalling() const override { return !CurrentFloor.bBlockingHit; }
	virtual void AddRadialForce(const FVector& Origin, float Radius, float Strength, ERadialImpulseFalloff Falloff) override;
	virtual void AddRadialImpulse(const FVector& Origin, float Radius, float Strength, ERadialImpulseFalloff Falloff, bool bVelChange) override;
	virtual void StopActiveMovement() override;			// Check CMC
	virtual void StopMovementImmediately() override;	// Check CMC
	// END UMovementComponent Interface
	
	void DisableMovement();
	void EnableMovement();

	/// @brief  Unground and prevent snapping to allow for leaving ground
	UFUNCTION(BlueprintCallable, Category="Physics State")
	FORCEINLINE void ForceUnground() { bMustUnground = true; }

	/// @brief  Get the velocity required to move [MoveDelta] amount in [DeltaTime] time.
	/// @param  MoveDelta Movement Delta To Compute Velocity
	/// @param  DeltaTime Delta Time In Which Move Would Be Performed
	/// @return Velocity corresponding to MoveDelta/DeltaTime if valid delta time was passed through
	UFUNCTION(BlueprintCallable, Category="Physics State")
	FORCEINLINE FVector GetVelocityFromMovement(FVector MoveDelta, float DeltaTime) const 
	{
		if (DeltaTime <= 0.f) return FVector::ZeroVector;
		return MoveDelta / DeltaTime;
	}
	
	UFUNCTION(BlueprintCallable, Category="Physics State")
	FORCEINLINE FVector GetVelocity() const { return Velocity; }

	UFUNCTION(BlueprintCallable, Category="Physics State")
	FORCEINLINE void SetVelocity(const FVector TargetVelocity) { Velocity = TargetVelocity; } 

	UFUNCTION(BlueprintCallable, Category="Physics State")
	FORCEINLINE bool IsStableOnGround() const { return IsMovingOnGround(); }

	UFUNCTION(BlueprintCallable, Category="Physics State")
	FORCEINLINE bool IsUnstableOnGround() const { return CurrentFloor.bUnstableFloor; }

	UFUNCTION(BlueprintCallable, Category="Physics State")
	FORCEINLINE bool IsInAir() const { return !CurrentFloor.bBlockingHit; }

#pragma region Events

	/// @brief Entry point for gameplay manipulation of rotation via blueprints or child class
	/// @param CurrentRotation Reference to current rotation to modify
	/// @param DeltaTime Current sub-step delta time
	UFUNCTION(BlueprintNativeEvent, Category="Movement Controller")
	void UpdateRotation(FQuat& CurrentRotation, float DeltaTime);
	virtual void UpdateRotation_Implementation(FQuat& CurrentRotation, float DeltaTime) {};

	/// @brief	Entry point for gameplay manipulation of velocity via blueprints or child class
	///			Velocity should only be modified through here as the order is important to what updates come after
	/// @param	CurrentVelocity Reference to current velocity to modify
	/// @param	DeltaTime Current sub-step delta time
	UFUNCTION(BlueprintNativeEvent, Category="Movement Controller")
	void UpdateVelocity(FVector& CurrentVelocity, float DeltaTime);
	virtual void UpdateVelocity_Implementation(FVector& CurrentVelocity, float DeltaTime) {};

	UFUNCTION(BlueprintNativeEvent, Category="Movement Controller")
	void SubsteppedTick(FVector& CurrentVelocity, float DeltaTime);
	virtual void SubsteppedTick_Implementation(FVector& CurrentVelocity, float DeltaTime) {};

#pragma endregion Events

#pragma endregion Exposed Calls
	
	
/* The core update loop of the movement component */
#pragma region Core Update Loop 

	void PerformMovement(float DeltaTime);
	
	virtual bool PreMovementUpdate(float DeltaTime);
	
	void StartMovementTick(float DeltaTime, uint32 Iterations);
	void GroundMovementTick(float DeltaTime, uint32 Iterations);
	void AirMovementTick(float DeltaTime, uint32 Iterations);
	
	virtual void PostMovementUpdate(float DeltaTime);

	virtual void MoveAlongFloor(const FVector& InVelocity, float DeltaTime, FStepDownResult* OutStepDownResult = NULL);

	void StartLanding(float DeltaTime, uint32 Iterations);
	void StartFalling(float DeltaTime, uint32 Iterations);


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

	void TryUpdateBasedMovement(float DeltaTime);

	void MovementBaseUpdate(float DeltaTime);

	/// @brief Set a moving base to keep, useful for things like following an elevator where when you jump, you still
	///			want to follow it
	/// @param BaseOverride Override any moving bases with the one passed through here
	UFUNCTION(BlueprintSetter)
	void SetMovementBaseOverride(UPrimitiveComponent* BaseOverride) {MovementBaseOverride = BaseOverride;}

#pragma endregion Moving Base 
	
/* Methods to evaluate the stability of a given hit or overlap */
#pragma region Stability Evaluations

	/// @brief  Checks whether we are currently able to move
	/// @return True if we are not stuck in geometry or have valid data
	bool CanMove() const;
	
	bool MustUnground() const;
	
	/// @brief  Determines if the pawn can be considered stable on a given slope normal.
	/// @param  Normal Given ground normal
	/// @return True if the pawn can be considered stable on a given slope normal
	bool IsFloorStable(FVector Normal) const;

	bool CanStepUp(const FHitResult& StepHit) const;

#pragma endregion Stability Evaluations


/* Methods to adjust movement based on the stability of a hit or overlap */
#pragma region Collision Adjustments

	virtual FVector GetPenetrationAdjustment(const FHitResult& Hit) const override;
	virtual bool ResolvePenetrationImpl(const FVector& Adjustment, const FHitResult& Hit, const FQuat& NewRotation) override;

	virtual float SlideAlongSurface(const FVector& Delta, float Time, const FVector& Normal, FHitResult& Hit, bool bHandleImpact) override;
	virtual void TwoWallAdjust(FVector& Delta, const FHitResult& Hit, const FVector& OldHitNormal) const override;
	virtual void HandleImpact(const FHitResult& Hit, float TimeSlice = 0.f, const FVector& MoveDelta = FVector::ZeroVector) override;
	
	bool StepUp(const FHitResult& StepHit, const FVector& Delta, FStepDownResult* OutStepDownResult = nullptr);

#pragma endregion Collision Adjustments


/* Methods to sweep the environment and retrieve information */
#pragma region Collision Checks

	bool ShouldCatchAir(const FGroundingStatus& OldFloor, const FGroundingStatus& NewFloor);

	void UpdateFloorFromAdjustment();
	void AdjustFloorHeight();
	bool IsWithinEdgeTolerance(const FVector& CapsuleLocation, const FVector& TestImpactPoint, const float CapsuleRadius) const;
	void ComputeFloorDist(const FVector& CapsuleLocation, float LineDistance, float SweepDistance, FGroundingStatus& OutFloorResult, float SweepRadius, const FHitResult* DownwardSweepResult) const;
	void FindFloor(const FVector& CapsuleLocation, FGroundingStatus& OutFloorResult, bool bCanUseCachedLocation, const FHitResult* DownwardSweepResult = NULL) const;
	bool FloorSweepTest(FHitResult& OutHit, const FVector& Start, const FVector& End, ECollisionChannel TraceChannel, const struct FCollisionShape& CollisionShape, const struct FCollisionQueryParams& Params, const struct FCollisionResponseParams& ResponseParams) const;

	bool IsValidLandingSpot(const FVector& CapsuleLocation, const FHitResult& Hit) const;
	bool ShouldCheckForValidLandingSpot(const FHitResult& Hit) const;

	float GetPerchRadiusThreshold() const;
	float GetValidPerchRadius() const;
	bool ShouldComputePerchResult(const FHitResult& InHit, bool bCheckRadius) const;
	bool ComputePerchResult(const float TestRadius, const FHitResult& InHit, const float InMaxFloorDist, FGroundingStatus& OutPerchFloorResult) const;

#pragma endregion Collision Checks

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


