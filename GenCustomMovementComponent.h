// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GenMovementComponent.h"
#include "AI/RVOAvoidanceInterface.h"
#include "AI/Navigation/NavigationAvoidanceTypes.h"
#include "Framework/Components/GenMovementComponent.h"
#include "GenCustomMovementComponent.generated.h"

/// TODO: Stuff Left To Implement:
///	1.) Handle the actual calls to PerformMovement, which is done via the ReplicatedTick
///
/// 1.) Calculate Velocity, make it so on ground we project the velocity on slopes rather than omit the Z velocity
///		and we can ignore everything else except the DirectMove, ApplyRotation (we wanna just do it here and expose the enum
///		that switches modes of rotation), and CalculateAvoidance
///
///	 2.) DirectMove and other "AI" related movement calls might not need to exist, we wanna handle it seperately and just ensure
///		 they're getting the correct InputVector to steer them (Received from RequestedVelocity I think).
///
///	 3.) Can safely ignore the LimitSpeed methods (Other than airborne terminal velocity) as our physics would naturally handle that
///
///	 4.) The ShouldUnground (named ReceivedUpwardForce) should be handled. Not doing anything with it right now, study the OG first.
///
///
///  10.) AI stuff (NotifyBumpedPawn is actually needed for AI and RVO)
///
///  12.) Collision Penetration Handling And Adjustment (GetPenetrationAdjustment, AutoResolvePenetration)
///
///  13.) Lastly, get rid of GenMovementNetworkComponent, and let GenMovementComponent be the parent class. This way I don't need to rely on
///       GenPawn or presumably GenPlayerController. There's no movement handled there and it adds a alot of network stuff we don't need, and we
///       wanna make our own Controller & Pawn for input buffering interface shit. Need to carry over the veloity getter/setter from it as well
///       as the delta time macros. Another thing to carry over is the ticks that are required locally. Start messing around with it after you get
///       this setup. GenMovementComponent handles a lot of physics for us so we wanna keep that since its generic

UENUM(BlueprintType)
enum class EGroundingStatus : uint8
{
	None UMETA(DisplayName = "None", ToolTip = "No Movement"),
	Grounded UMETA(DisplayName = "Grounded", ToolTip = "On Ground"),
	Aerial UMETA(DisplayName = "Aerial", ToolTip = "In Air")
};

UENUM(BlueprintType)
enum class ERotationHandling : uint8
{
	None UMETA(DisplayName = "None", ToolTip = "Rotation Not Handled Automatically"),
	OrientToVelocity UMETA(DisplayName = "Orient To Velocity", ToolTip = "Orient Forward To Velocity Direction"),
	OrientToInput UMETA(DisplayName = "Orient To Input", ToolTip = "Orient Forward To Input Direction")
};

/**
 * 
 */
UCLASS(ClassGroup="Movement", BlueprintType, Blueprintable, meta = (BlueprintSpawnableComponent))
class PROTOTYPING_API UGenCustomMovementComponent : public UGenMovementComponent
{
	GENERATED_BODY()

#pragma region GENMOVEMENT_OVERRIDES

public:
	UGenCustomMovementComponent();\

	void DebugMessage(const FColor color, const FString message) const;

	// Basic
	void BeginPlay() override;;
	void SetUpdatedComponent(USceneComponent* NewUpdatedComponent) override;

	void GenReplicatedTick_Implementation(float DeltaTime) override;

	// TODO: Override this later as the override is primarily for adding RVO support
	//void NotifyBumpedPawn(APawn* BumpedPawn) override;;

	// Collision Adjustments
	void HandleImpact(const FHitResult& Impact, float TimeSlice = 0.f, const FVector& MoveDelta = FVector::ZeroVector) override;
	FVector GetPenetrationAdjustment(const FHitResult& Hit) const override;
	FVector ComputeSlideVector(const FVector& Delta, float Time, const FVector& Normal, const FHitResult& Hit) const override;
	float SlideAlongSurface(const FVector& Delta, float Time, const FVector& Normal, FHitResult& Hit, bool bHandleImpact = false) override;
	void TwoWallAdjust(FVector& Delta, const FHitResult& Hit, const FVector& OldHitNormal) const override;

	// Generic Movement Calls
	void DisableMovement(); // Eh shouldnt be placed here lmao
	void HaltMovement() override;
	bool CanMove() const override;

	// Collision Data
	USceneComponent* SetRootCollisionShape(EGenCollisionShape NewCollisionShape, const FVector& Extent, FName Name/*not used*/) override;
	void SetRootCollisionExtent(const FVector& NewExtent, bool bUpdateOverlaps = true) override;
	EGenCollisionShape GetRootCollisionShape() const override;
	FVector GetRootCollisionExtent() const override;
	FHitResult AutoResolvePenetration() override;

#if WITH_EDITOR
	// Validating editor changes
	void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

#pragma endregion GENMOVEMENT_OVERRIDES

#pragma region MOVEMENT_PARAMETERS

public:
	// ======== Movement Parameters ======= //
	// Movement Data (Could also bundle in rotation with this , gonna define it here now tho and move it out later once i get things set up) //
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Movement|Rotation")
	// The method in which we want to handle rotation
	ERotationHandling RotationHandling;
	
	// ===================================== //
	// ======== Rotation Parameters ======== //
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Rotation")
	/// When true, the pawn will always align their up with the current ground normal
	bool bOrientToGroundNormal{false};
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Rotation", meta = (ClampMin = "0", UIMin = "0"))
	/// When orienting the pawn's rotation to the input or control rotation direction, this is the rate of rotation.
	float RotationRate{650.f};

	// ====================================== //
	// ======== Movement Constraints ======== //
	// ******** Can move these to data objects to allow for hot swapping of them ******** //
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement|Constraints", meta = (ClampMin = "0", ClampMax = "90", UIMin = "0", UIMax = "90"))
	/// Max angle in degrees of a surface the pawn can still walk on. Should only be set through @see SetWalkableFloorAngle.
	float WalkableFloorAngle{45.f};

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Constraints", meta = (ClampMin = "0", UIMin = "0", UIMax = "100"))
	/// The maximum height the pawn can step up to while grounded.
	float MaxStepUpHeight{50.f};

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Constraints", meta = (ClampMin = "0", UIMin = "0", UIMax = "100"))
	/// When walking down a slope or off a ledge, the pawn will remain grounded if the floor underneath is closer than this threshold.
	float MaxStepDownHeight{50.f};

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Constraints")
	/// Whether the pawn is able to walk off ledges which exceed the max step down height when grounded.
	bool bCanWalkOffLedges{true};

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Constraints", meta =
	  (ClampMin = "0", ClampMax = "1", UIMin = "0", UIMax = "1", EditCondition = "bCanWalkOffLedges"))
	/// When standing on a ledge the pawn will fall off if its collision shape perches further beyond the end of the ledge than the set
	/// threshold allows. This is percentage based with the center of the collision being 0 (i.e. the pawn will fall off as early as possible)
	/// and the outer boundary of the collision being 1 (i.e. the pawn will fall off as late as possible). For box collisions the threshold
	/// is internally treated as either 1 (if >= 0.5) or 0 (if < 0.5).
	float LedgeFallOffThreshold{0.5f};

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Constraints", AdvancedDisplay)
	/// If true, the contact normal of a hit will also be considered to check whether a floor is walkable when landing. This will often allow
	/// the pawn to land on hard edges that usually produce opposing impact normals.
	bool bLandOnEdges{true};

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Constraints", AdvancedDisplay, meta = (ClampMin = "0.0001", UIMin = "1"))
	/// How far downwards the trace should go when updating the floor.
	float FloorTraceLength{500.f};

	// ===================================== //
	// ======== Collision Resolution ======= //
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Penetration Resolution", AdvancedDisplay, meta = (ClampMin = "0", UIMin = "0"))
	/// Max distance allowed for depenetration when moving out of anything but pawns.
	float MaxDepenetrationWithGeometry{100.f};

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Penetration Resolution", AdvancedDisplay, meta = (ClampMin = "0", UIMin = "0"))
	/// Max distance allowed for depenetration when moving out of other pawns.
	float MaxDepenetrationWithPawn{100.f};
	
	// ===================================== //
	// ======== Animation Montage ========== //
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Movement|Animation")
	/// Only relevant when playing a montage with animation root motion: if false, no root motion will be applied while the montage is
	/// blending in.
	bool bApplyRootMotionDuringBlendIn{true};

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Movement|Animation")
	/// Only relevant when playing a montage with animation root motion: if false, no root motion will be applied while the montage is
	/// blending out.
	bool bApplyRootMotionDuringBlendOut{true};

	// ===================================== //
	// ======== Imparting Velocity ========= //
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Platforms")
	/// Whether the pawn should assume the velocity of a moveable base while grounded.
	bool bMoveWithBase{true};
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Platforms", meta=(EditCondition = "bMoveWithBase"))
	/// Whether to impart the linear velocity of the current movement base when falling off it. Velocity is never imparted from a base that
	/// is simulating physics. Only applies if "MoveWithBase" is true.
	bool bImpartLinearBaseVelocity{true};

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Platforms", meta=(EditCondition = "bMoveWithBase"))
	/// Whether to impart the angular velocity of the current movement base when falling off it. Velocity is never imparted from a base that
	/// is simulating physics. Only applies if "MoveWithBase" is true.
	bool bImpartAngularBaseVelocity{true};

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Movement|Platforms", AdvancedDisplay, meta=(EditCondition = "bMoveWithBase"))
	/// If true, the mass of the pawn will be taken into account when imparting the velocity of a base the pawn has just left.
	bool bConsiderMassOnImpartVelocity{false};

	// =================================== //
	// ======== Physics Interactions ===== //
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics Interaction")
	/// Whether the pawn should interact with physics objects in the world.
	bool bEnablePhysicsInteraction{true};

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics Interaction", meta =
    (UIMin = "0", EditCondition = "bEnablePhysicsInteraction"))
	/// Multiplier for the force that is applied to physics objects that are touched by the pawn.
	float TouchForceScale{1.f};

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics Interaction", meta =
    (ClampMin = "0", UIMin = "0", EditCondition = "bEnablePhysicsInteraction"))
	/// The minimum force applied to physics objects touched by the pawn.
	float MinTouchForce{0.f};

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics Interaction", meta =
    (ClampMin = "0", UIMin = "0", EditCondition = "bEnablePhysicsInteraction"))
	/// The maximum force applied to physics objects touched by the pawn.
	float MaxTouchForce{250.f};

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics Interaction", meta =
    (EditCondition = "bEnablePhysicsInteraction"))
	/// If true, "TouchForceScale" is applied per kilogram of mass of the affected object.
	bool bScaleTouchForceToMass{true};

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics Interaction", meta =
    (UIMin = "0", EditCondition = "bEnablePhysicsInteraction"))
	/// Multiplier for the force that is applied when the player collides with a blocking physics object.
	float PushForceScale{750000.f};

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics Interaction", meta =
    (UIMin = "0", EditCondition = "bEnablePhysicsInteraction"))
	/// Multiplier for the initial impulse force applied when the pawn bounces into a blocking physics object.
	float InitialPushForceScale{500.f};

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics Interaction", meta =
    (EditCondition = "bEnablePhysicsInteraction"))
	/// If true, "PushForceScale" is applied per kilogram of mass of the affected object.
	bool bScalePushForceToMass{false};

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics Interaction", meta =
    (EditCondition = "bEnablePhysicsInteraction"))
	/// If true, the applied push force will try to get the touched physics object to the same velocity as the pawn, not faster. This will
	/// only ever scale the force down and will never apply more force than calculated with regard to "PushForceScale".
	bool bScalePushForceToVelocity{true};

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics Interaction", meta =
    (EditCondition = "bEnablePhysicsInteraction"))
	/// If true, the push force location is adjusted using "PushForceZOffsetFactor". If false, the impact point is used.
	bool bUsePushForceZOffset{false};

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics Interaction", meta =
    (UIMin = "-1", UIMax = "1", EditCondition = "bEnablePhysicsInteraction && bUsePushForceZOffset"))
	/// Z-offset for the location the force is applied to the touched physics object (0 = center, 1 = top, -1 = bottom).
	float PushForceZOffsetFactor{-0.75f};

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics Interaction", meta =
    (UIMin = "0", EditCondition = "bEnablePhysicsInteraction"))
	/// Multiplier for the gravity force applied to physics objects the pawn is walking on.
	float DownwardForceScale{1.f};

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics Interaction", meta =
    (ClampMin = "0", UIMin = "0", EditCondition = "bEnablePhysicsInteraction"))
	/// The force applied constantly per kilogram of mass of the pawn to all overlapping components.
	float RepulsionForce{2.5f};

	// ============================= //
	// ======== RVO Avoidance ====== //
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RVO Avoidance")
	/// Whether avoidance should be used for bots.
	bool bUseAvoidance{false};

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RVO Avoidance")
	/// Indicates RVO behavior.
	float AvoidanceWeight{0.f};

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RVO Avoidance")
	/// The radius around the pawn for which to consider avoidance of other agents.
	float AvoidanceConsiderationRadius{500.f};

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RVO Avoidance")
	/// This pawn's avoidance group.
	FNavAvoidanceMask AvoidanceGroup;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RVO Avoidance")
	/// This pawn will avoid other agents that belong to one of the groups specified in the mask.
	FNavAvoidanceMask GroupsToAvoid;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RVO Avoidance")
	/// This pawn will ignore other agents that belong to one of the groups specified in the mask.
	FNavAvoidanceMask GroupsToIgnore;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NavMovement", meta = (DisplayAfter = "bUseFixedBrakingDistanceForPaths"))
	/// If true, the magnitude of the requested velocity will be used as max speed if it does not exceed the max desired speed.
	bool bUseRequestedVelocityMaxSpeed{false};

#pragma endregion MOVEMENT_PARAMETERS

#pragma region CONST_VALUES
	/// Names given to a dynamically spawned root components (@see SetRootCollisionShape). Must never be equal to the name of the root
	/// component when first spawned (so this should be a string that is unlikely to be picked by the user when creating an actor).
	inline static const FName ROOT_NAME_DYNAMIC_CAPSULE = "RCSDynamicCapsule";
	inline static const FName ROOT_NAME_DYNAMIC_FLAT_CAPSULE = "RCSDynamicFlatCapsule";
	inline static const FName ROOT_NAME_DYNAMIC_BOX = "RCSDynamicBox";
	inline static const FName ROOT_NAME_DYNAMIC_SPHERE = "RCSDynamicSphere";
	/// If we reach a velocity magnitude lower or equal to this value when braking, velocity is zeroed.
	static constexpr float BRAKE_TO_STOP_VELOCITY = KINDA_SMALL_NUMBER;
	/// The minimum velocity magnitude the pawn should have when falling off a ledge.
	static constexpr float MIN_LEDGE_FALL_OFF_VELOCITY = 500.f;
	/// The minimum size of the deceleration vector when braking. Prevents the velocity from becoming very small when it approaches 0.
	static constexpr float MIN_DECELERATION = 5.f;
	/// The minimum distance the pawn should maintain to the floor when grounded.
	static constexpr float MIN_DISTANCE_TO_FLOOR = 1.9f;
	/// The maximum distance the pawn should maintain to the floor when grounded.
	static constexpr float MAX_DISTANCE_TO_FLOOR = 2.4f;
	/// Reject impacts that are this close to the edge of the vertical portion of the collision shape when performing vertical sweeps.
	static constexpr float EDGE_TOLERANCE = 0.15f;
	/// The min size of the step up location delta to be applied, so we don't fail the step up due to an unwalkable surface when starting from
	/// a resting position.
	static constexpr float MIN_STEP_UP_DELTA = 5.f;
#pragma endregion CONST_VALUES

#pragma region MOVEMENT_INFORMATION_HANDLING

public:
	/// @brief		Returns whether we are on valid ground
	///	@returns	bool	True if on ground, false otherwise
	UFUNCTION(BlueprintCallable, Category="Movement|Information")
	bool IsGrounded() const;

	/// @brief		Returns whether we are not on valid ground
	///	@returns	bool	True if in air, false otherwise
	UFUNCTION(BlueprintCallable, Category="Movement|Information")
	bool IsAirborne() const;

	
	/// @brief		Sets the grounding status, usually done automatically but you can force it to ensure specific behavior flags
	/// @param		NewGroundingStatus  
	/// @return		void
	UFUNCTION(BlueprintCallable, Category="Movement|Information")
	void SetGroundingStatus(EGroundingStatus NewGroundingStatus);

#pragma endregion GET_MOVEMENT_INFORMATION_HANDLING

#pragma region MOVEMENT_PHYSICS

protected:

#pragma region PHYSICS_VALUES
	// ======== General Information ======= //
	
	UPROPERTY(BlueprintReadWrite, Category="Movement Component")
	/// The current movement mode of the pawn. Default values: 0 = None, 1 = Grounded, 2 = Airborne
	/// @attention Use @see SetMovementMode to prompt a call to @see OnMovementModeChanged. If you don't want the event to be triggered the
	/// movement mode can be set directly. We use an enum because we don't care about network replication
	EGroundingStatus GroundingStatus;
	
	UPROPERTY(BlueprintReadWrite, Category="Movement Component")
	/// Holds information about the floor currently located underneath the pawn. Note that the pawn does not necessarily have to be grounded
	/// for this to be valid (@see FloorTraceLength).
	FFloorParams CurrentFloor;

	UPROPERTY(BlueprintReadWrite, Category="Movement Component")
	/// Whether the pawn received some form of upward force that should be considered when the movement mode is updated during the next tick.
	/// This flag gets reset automatically after it was processed (@see UpdateMovementModeDynamic).
	bool bShouldUnground{false};

	UPROPERTY(BlueprintReadWrite, Category="Movement Component")
	// Holds whatever the value of @see MaxDesiredSpeed when the pawn initially spawned
	float DefaultMaxSpeed{0.0f};
	
	UPROPERTY(BlueprintReadOnly, Category = "Movement Component")
	bool bStuckInGeometry{false};

	UPROPERTY(BlueprintReadOnly, Category = "Movement Component")
	/// The 2D direction in which the pawn should fall off a ledge when exceeding @see LedgeFallOffThreshold. Will be a zero vector if the
	/// pawn is currently not in the process of falling off.
	FVector LedgeFallOffDirection{0};

	// ====================================== //
	// ======== My Collision Data =========== //
	UPROPERTY(BlueprintReadOnly, Category = "Movement Component")
	/// The current collision shape of the pawn (@see EGenCollisionShape). Mainly used for replication (@see REPLICATE_COLLISION).
	uint8 CurrentRootCollisionShape{0};

	UPROPERTY(BlueprintReadOnly, Category = "Movement Component")
	/// The current extent of the pawn's root collision. Mainly used for replication (@see REPLICATE_COLLISION).
	FVector CurrentRootCollisionExtent{0};


#pragma endregion PHYSICS_VALUES

#pragma region PHYSICS_METHODS
	
	/// @brief		Clamps selected data members to their respective valid range when the actor is spawned and before movement is executed
	/// 
	/// @return void
	//virtual void ClampToValidValues();

	///	@brief		Returns the current grounding status of the pawn
	///
	/// @return		EGroundingStatus	The current grounding status.
	UFUNCTION(BlueprintCallable, Category = "Movement Component")
	EGroundingStatus GetGroundingStatus() const;
	
	/// @brief		Main function for moving the pawn and updating all associated data 
	/// @param		DeltaSeconds	The current move delta time
	/// @return		void
	virtual void PerformMovement(float DeltaSeconds);

	/// @brief		Executes the movement physics, solving for slopes and steps and so on. 
	/// @param		DeltaSeconds	The delta time to use
	/// @return		void
	virtual void RunPhysics(float DeltaSeconds);

	/// @brief		Move the updated component along the floor while grounded. Movement velocity will always remain horizontal.
	///
	/// @param      LocationDelta    The location delta to apply.
	/// @param      Floor            The current floor.
	/// @param      DeltaSeconds     The delta time to use.
	/// @returns    FVector          The applied location delta.
	virtual FVector GroundedPhysics(const FVector& LocationDelta, FFloorParams& Floor, float DeltaSeconds);

	/// @brief		Move the updated component by the given location delta while in the air.
	///
	/// @param      LocationDelta    The location delta to apply.
	/// @param      DeltaSeconds     The delta time to use.
	/// @returns    bool             True if the pawn landed on a walkable surface, false otherwise.
	virtual bool AirbornePhysics(const FVector& LocationDelta, float DeltaSeconds);

	
	/// @brief		Wrapper around @see AdjustVelocityFromHit that prevents the pawn from gaining upward velocity from the hit adjustment.
	///
	/// @param      Hit             The hit result of the collision.
	/// @param      DeltaSeconds    The delta time to use.
	/// @returns    void
	UFUNCTION(BlueprintNativeEvent, Category = "General Movement Component")
	void AdjustVelocityFromHitAirborne(const FHitResult& Hit, float DeltaSeconds);
	virtual void AdjustVelocityFromHitAirborne_Implementation(const FHitResult& Hit, float DeltaSeconds);

	
	/// @brief		Executes the movement physics, solving for slopes and steps and so on. (Taking from physicsgrounded)
	/// @param		DeltaSeconds	The delta time to use
	/// @return		void
	/// UFUNCTION(BlueprintCallable, Category = "Movement Component")
	virtual void SolvePhysics(float DeltaSeconds);

#pragma endregion PHYSICS_METHODS

#pragma region PHYSICS_INTERACTIONS

#pragma region MOVING_BASE

	/// @brief		Makes the pawn follow its current movement base.
	///
	/// @param      BaseVelocity    The velocity of the component the pawn is currently based on.
	/// @param      Floor           The current floor.
	/// @param      DeltaSeconds    The delta time to use.
	/// @returns    void
	virtual void MoveWithBase(const FVector& BaseVelocity, FFloorParams& Floor, float DeltaSeconds);

	/// @brief		Get the velocity of the object the pawn is currently based on.
	///
	/// @param      MovementBase    The component the pawn is currently based on.
	/// @returns    FVector         The (linear and angular) velocity of the movement base.
	virtual FVector ComputeBaseVelocity(UPrimitiveComponent* MovementBase);

	/// @brief		Returns the base velocity when the current movement base is a pawn. In a networked context we must take care not to use a values that
	///				are not synchronised between server and client.
	///
	/// @param      PawnMovementBase    The current movement base of type pawn.
	/// @returns    FVector             The net-safe base velocity.
	virtual FVector GetPawnBaseVelocity(APawn* PawnMovementBase) const;
	
	/// @brief		Determines whether the velocity of the movement base should be imparted.
	///
	/// @param      MovementBase    The current movement base of the pawn.
	/// @returns    bool            True if the base velocity should be imparted, false otherwise.
	virtual bool ShouldImpartVelocityFromBase(UPrimitiveComponent* MovementBase) const;

#pragma endregion MOVING_BASE

	/// @brief		Delegate called when the root collision touches another primitive component.
	/// @see		UPrimitiveComponent::OnComponentBeginOverlap
	UFUNCTION()
	virtual void RootCollisionTouched(
	  UPrimitiveComponent* OverlappedComponent,
	  AActor* OtherActor,
	  UPrimitiveComponent* OtherComponent,
	  int32 OtherBodyIndex,
	  bool bFromSweep,
	  const FHitResult& SweepResult
	);
	
	/// @brief		Applies impact forces to the hit component when using physics interaction (@see bEnablePhysicsInteraction).
	///
	/// @param      Impact                The hit result of the impact.
	/// @param      ImpactAcceleration    The acceleration of the pawn at the time of impact.
	/// @param      ImpactVelocity        The velocity of the pawn at the time of impact.
	/// @returns    void
	virtual void ApplyImpactPhysicsForces(const FHitResult& Impact, const FVector& ImpactAcceleration, const FVector& ImpactVelocity);

	/// @brief		Applies a downward force when walking on top of physics objects.
	///
	/// @param      DeltaSeconds    The delta time to use.
	/// @returns    void
	virtual void ApplyDownwardForce(float DeltaSeconds);

	/// @brief		Applies a repulsion force to touched physics objects.
	///
	/// @param      DeltaSeconds    The delta time to use.
	/// @returns    void
	virtual void ApplyRepulsionForce(float DeltaSeconds);

#pragma endregion PHYSICS_INTERACTIONS

#pragma region UPDATES

	/// @brief		Called before any kind of movement related update has happened. This is the only movement event that is called even if the pawn cannot
	///				move (@see CanMove).
	///
	/// @param		DeltaSeconds    The current move delta time.
	/// @returns	void
	UFUNCTION(BlueprintNativeEvent, Category = "General Movement Component")
	void PreMovementUpdate(float DeltaSeconds);
	virtual void PreMovementUpdate_Implementation(float DeltaSeconds);

	/// @brief		Called after movement was performed. If we are playing a montage the pose has been ticked and root motion was consumed at the time
	///				this function is called.
	///
	/// @param		DeltaSeconds    The current move delta time.
	/// @returns	void
	UFUNCTION(BlueprintNativeEvent, Category = "General Movement Component")
	void PostMovementUpdate(float DeltaSeconds);
	virtual void PostMovementUpdate_Implementation(float DeltaSeconds) {}

	/// @breif		Called immediately before switching on the current movement mode and executing the appropriate physics. At this point the movement
	///				mode and floor have been updated, and the input vector has been pre-processed (@see GetProcessedInputVector).
	///
	/// @param		DeltaSeconds    The current move delta time.
	/// @returns	void
	UFUNCTION(BlueprintNativeEvent, Category = "General Movement Component")
	void PrePhysicsUpdate(float DeltaSeconds);
	virtual void PrePhysicsUpdate_Implementation(float DeltaSeconds) {}

	/// @brief		Called immediately after the movement physics have run.
	///
	/// @param		DeltaSeconds    The current move delta time.
	/// @returns	void
	UFUNCTION(BlueprintNativeEvent, Category = "General Movement Component")
	void PostPhysicsUpdate(float DeltaSeconds);
	virtual void PostPhysicsUpdate_Implementation(float DeltaSeconds);

	/// @brief		Updates the movement mode dynamically (i.e. with regard to the current movement mode). Returning false indicates that the movement
	///				mode should still be updated statically (@see UpdateMovementModeStatic) afterwards, returning true will skip the static update.
	///
	/// @param      Floor           The current floor parameters.
	/// @param      DeltaSeconds    The delta time to use.
	/// @returns    bool            If false, the movement mode will still be updated statically afterwards.
	UFUNCTION(BlueprintNativeEvent, Category = "General Movement Component")
	bool UpdateMovementModeDynamic(const FFloorParams& Floor, float DeltaSeconds);
	virtual bool UpdateMovementModeDynamic_Implementation(const FFloorParams& Floor, float DeltaSeconds);

	/// @brief		Updates the movement mode statically (i.e. independent of the current movement mode).
	///
	/// @param      Floor           The current floor parameters.
	/// @param      DeltaSeconds    The delta time to use.
	/// @returns    void
	UFUNCTION(BlueprintNativeEvent, Category = "General Movement Component")
	void UpdateMovementModeStatic(const FFloorParams& Floor, float DeltaSeconds);
	virtual void UpdateMovementModeStatic_Implementation(const FFloorParams& Floor, float DeltaSeconds);

	/// @brief		Called after the movement mode was updated dynamically and/or statically.
	/// @attention	Do not confuse this function with @see OnMovementModeChanged.
	/// @see		UpdateMovementModeDynamic
	/// @see		UpdateMovementModeStatic
	///
	/// @param      PreviousMovementMode    The movement mode we changed from (i.e. the value of @see MovementMode before it was updated).
	/// @returns    void
	UFUNCTION(BlueprintNativeEvent, Category = "General Movement Component")
	void OnMovementModeUpdated(EGroundingStatus PreviousMovementMode);
	virtual void OnMovementModeUpdated_Implementation(EGroundingStatus PreviousMovementMode);

	/// @brief		Called when the updated component is stuck in geometry. If this happens no movement events after @see PreMovementUpdate will be called
	///				anymore.
	///
	/// @returns    void
	UFUNCTION(BlueprintNativeEvent, Category = "General Movement Component")
	void OnStuckInGeometry();
	virtual void OnStuckInGeometry_Implementation() {}

	
	/// @brief		Called at the end of the current movement update. This is the preferred entry point for subclasses to implement custom logic if
	///				automatic handling of the default movement modes is desired. Where to add and handle velocities.
	///
	/// @param		DeltaSeconds    The current move delta time (may not be equal to the frame delta time).
	/// @returns	void
	UFUNCTION(BlueprintNativeEvent, Category = "General Movement Component")
	void MovementUpdate(float DeltaSeconds);
	virtual void MovementUpdate_Implementation(float DeltaSeconds) {}

#pragma endregion UPDATES

#pragma region VEL_CALCULATIONS

	/// @brief		Calculates the new movement velocity for based on the current pawn state.
	/// @attention	Velocity from root motion animations is calculated separately (@see CalculateAnimRootMotionVelocity).
	///
	/// @param      DeltaSeconds    The delta time to use.
	/// @returns    void
	UFUNCTION(BlueprintCallable, Category = "General Movement Component")
	virtual void CalculateVelocity(float DeltaSeconds);

	/// @brief		Change the pawn's orientation.
	///
	/// @param      bIsDirectBotMove    Whether the pawn is a bot executing direct movement (i.e. the pawn is not using acceleration values
	///                                   to calculate its velocity).
	/// @param      DeltaSeconds        The delta time to use.
	/// @returns    void
	UFUNCTION(BlueprintCallable, Category = "General Movement Component")
	virtual void ApplyRotation(bool bIsDirectBotMove, float DeltaSeconds);

#pragma endregion VEL_CALCULATIONS

#pragma region COLLISION_QUERIES

	/// @brief		Prevents the pawn from boosting up slopes when airborne by limiting the computed slide vector.
	///
	/// @param      SlideResult    The computed slide vector.
	/// @param		Delta          The attempted movement location delta.
	/// @param      Time           The amount of the delta to apply.
	/// @param      Normal         The hit normal.
	/// @param      Hit            The original hit result that was used to compute the slide vector.
	/// @returns    FVector        The adjusted slide vector.
	virtual FVector HandleSlopeBoosting(
	  const FVector& SlideResult,
	  const FVector& Delta,
	  float Time,
	  const FVector& Normal,
	  const FHitResult& Hit
	) const;

	/// @brief		Computes a vector that moves parallel to the hit ramp.
	///
	/// @param      LocationDelta    The attempted location delta.
	/// @param      RampHit          The ramp that was hit.
	/// @returns    FVector          The new movement vector that moves parallel to the hit surface.
	virtual FVector ComputeRampVector(const FVector& LocationDelta, const FHitResult& RampHit) const;
	
	/// @brief		Checks if the hit surface is a valid landing spot for the pawn (when trying to land after being airborne).
	///
	/// @param      Hit         The hit surface.
	/// @returns    bool        True if the passed location is a valid landing spot, false otherwise.
	virtual bool IsValidLandingSpot(const FHitResult& Hit);

	/// @brief		Handles the grounding status change from airborne to grounded.
	///
	/// @param      Hit             The hit result after landing.
	/// @param      DeltaSeconds    The delta time to use.
	/// @param      bUpdateFloor    Whether the current floor should be updated.
	/// @returns    void
	virtual void ProcessLanded(const FHitResult& Hit, float DeltaSeconds, bool bUpdateFloor = true);

	/// @brief		Called when the movement mode changes from airborne to grounded.
	///
	/// @returns    void
	UFUNCTION(BlueprintNativeEvent, Category = "General Movement Component")
	void OnLanded();
	virtual void OnLanded_Implementation() {}
	
	/// @brief		Checks if the hit object is a walkable surface.
	///
	/// @param		Hit     The hit result to be checked.
	/// @returns    bool    True if the surface is walkable, false if not.
	UFUNCTION(BlueprintCallable, Category = "General Movement Component")
	virtual bool HitWalkableFloor(const FHitResult& Hit) const;

	/// Hanldes stepping up barriers that do not exceed @see MaxStepUpHeight.
	///
	/// @param        LocationDelta    The attempted location delta.
	/// @param        BarrierHit       The barrier that was hit.
	/// @param        Floor            The current floor.
	/// @param        OutForwardHit    The hit result of the step-up forward movement.
	/// @returns      bool             True if the pawn has scaled the barrier, false otherwise.
	virtual bool StepUp(
	  const FVector& LocationDelta,
	  const FHitResult& BarrierHit,
	  FFloorParams& Floor,
	  FHitResult* OutForwardHit = nullptr
	);
	
	/// Maintains the pawn's distance to the floor.
	///
	/// @param        Floor    The current floor (will be updated).
	/// @returns      bool     Whether the adjustment was successful and the pawn is within the set limits.
	UFUNCTION(BlueprintCallable, Category = "General Movement Component")
	virtual bool MaintainDistanceToFloor(UPARAM(ref) FFloorParams& Floor);

	/// @brief		Check whether the pawn is perching further beyond a ledge than allowed by @see LedgeFallOffThreshold. For box collision the threshold
	///				is assumed to be either 1 (if LedgeFallOffThreshold >= 0.5) or 0 (if LedgeFallOffThreshold < 0.5).
	///
	/// @param      ImpactPoint       The impact point to test for (usually from the hit result of the floor sweep).
	/// @param      PawnLowerBound    The lower bound of the pawn's collision.
	/// @param      PawnCenter        The center of the pawn's collision.
	/// @returns    bool              True if the pawn is exceeding the threshold, false otherwise.
	virtual bool IsExceedingFallOffThreshold(const FVector& ImpactPoint, const FVector& PawnLowerBound, const FVector& PawnCenter) const;


#pragma endregion COLLISION_QUERIES

#pragma endregion MOVEMENT_PHYSICS

#pragma region ROOT_MOTION
public:
	/// @brief		Sets a reference (@see SkeletalMesh) to the skeletal mesh component of the owning pawn. This is automatically done once when beginning
	///				play (taking the first skeletal mesh in the pawn's scene component hierarchy), but if the mesh is changed at any point after that the
	///				reference needs to be updated manually.
	/// @attention	Setting a skeletal mesh reference will add the movement component as a tick prerequisite component for the mesh.
	///
	/// @param      Mesh    The skeletal mesh to use. Passing nullptr clears the reference.
	/// @returns    void
	UFUNCTION(BlueprintCallable, Category = "General Movement Component")
	virtual void SetSkeletalMeshReference(USkeletalMeshComponent* Mesh);

	/// @brief		Returns the scaling factor applied to any animation root motion translation on this pawn.
	///
	/// @returns    float    The scaling factor for animation root motion.
	UFUNCTION(BlueprintCallable, Category = "General Movement Component")
	float GetAnimRootMotionTranslationScale() const;

	/// @brief		Sets the scaling factor applied to any animation root motion translation on this pawn.
	///
	/// @param      Scale    The scaling factor to use.
	/// @returns    void
	UFUNCTION(BlueprintCallable, Category = "General Movement Component")
	void SetAnimRootMotionTranslationScale(float Scale = 1.f);

	/// @brief		Returns whether we currently have animation root motion to consider. Valid throughout the whole movement tick even when montages
	///				instances are cleared regularly for networked play (@see StepMontagePolicy). Should be preferred over using
	///				@see UGenMovementComponent::IsPlayingRootMotion which can be misleading in a networked context.
	///
	/// @returns    bool    True if there currently is animation root motion to consider, false otherwise.
	UFUNCTION(BlueprintCallable, Category = "General Movement Component")
	bool HasAnimRootMotion() const;

protected:
	/// @brief		Blocks the automatic pose tick by ensuring that @see USkeletalMeshComponent::ShouldTickPose will return false on the currently set
	///				skeletal mesh.
	///
	/// @returns    void
	virtual void BlockSkeletalMeshPoseTick() const;

	/// @brief		Whether the pawn's pose should be ticked manually this iteration because we are playing a montage.
	///
	/// @param      bOutSimulate    Only relevant if the function return value is true. Whether the pose tick should only be simulated
	///                               because this is a client replay, a remote move execution or a sub-stepped move iteration.
	/// @returns    bool            True if the pawn's pose should be ticked manually, false otherwise.
	virtual bool ShouldTickPose(bool* bOutSimulate = nullptr) const;

	/// @brief		Ticks the pawn's pose and consumes root motion if applicable.
	/// @attention	Root motion from animations can only be handled correctly in networked games if the root motion mode (@see ERootMotionMode)
	///				is set to "RootMotionFromMontagesOnly".
	///
	/// @param      DeltaSeconds    The delta time to use.
	/// @param      bSimulate       Whether the pose tick should only be simulated (e.g. for client replay or remote move execution).
	/// @returns    void
	virtual void TickPose(float DeltaSeconds, bool bSimulate);
	
	/// @brief		Checks whether the currently available root motion movement parameters should not be applied due to the configured values of
	///				@see bApplyRootMotionDuringBlendIn and @see bApplyRootMotionDuringBlendOut.
	///
	/// @param      RootMotionMontage            The montage of the currently playing root motion montage instance.
	/// @param      RootMotionMontagePosition    The position in the currently playing root motion montage instance.
	/// @returns    bool                         True if the current root motion parameters should be discarded, false otherwise.
	virtual bool ShouldDiscardRootMotion(UAnimMontage* RootMotionMontage, float RootMotionMontagePosition) const;

	/// @brief		Applies rotation from animation root motion to the updated component.
	/// @attention	The pawn postition will only be adjusted to account for collisions when rotating around the yaw axis. Roll- and pitch-
	///				rotations will be set if they won't cause any blocking collisions, but no adjustment to the pawn position will be made.
	///
	/// @param      DeltaSeconds    The delta time to use.
	/// @returns    void
	virtual void ApplyAnimRootMotionRotation(float DeltaSeconds);

	/// @brief		Calculates the velocity from animation root motion.
	///
	/// @param      DeltaSeconds    The delta time to use.
	/// @returns    void
	virtual void CalculateAnimRootMotionVelocity(float DeltaSeconds);

	/// @brief		Allows for post-processing of the calculated root motion velocity.
	///
	/// @param      RootMotionVelocity    The velocity calculated purely from the root motion animation.
	/// @param      DeltaSeconds          The delta time to use.
	/// @returns    FVector               The final velocity the pawn should assume.
	UFUNCTION(BlueprintNativeEvent, Category = "General Movement Component")
	FVector PostProcessAnimRootMotionVelocity(const FVector& RootMotionVelocity, float DeltaSeconds);
	virtual FVector PostProcessAnimRootMotionVelocity_Implementation(const FVector& RootMotionVelocity, float DeltaSeconds);

	UPROPERTY(Transient)
	/// The current root motion parameters.
	FRootMotionMovementParams RootMotionParams;

	UPROPERTY(BlueprintReadWrite, Category = "General Movement Component")
	/// Whether we currently have any root motion from animations to consider.
	bool bHasAnimRootMotion{false};

	UPROPERTY(BlueprintReadWrite, Category = "General Movement Component")
	/// Scaling factor applied to animation root motion translation on this pawn.
	float AnimRootMotionTranslationScale{1.f};

	UPROPERTY(BlueprintReadWrite, Category = "General Movement Component")
	/// Reference to the skeletal mesh of the owning pawn.
	USkeletalMeshComponent* SkeletalMesh{nullptr};
#pragma endregion ROOT_MOTION

#pragma region STEERING_INPUT

protected:
	UPROPERTY(BlueprintReadOnly, Category = "Movement Component|Input")
	/// The input vector that is used for all physics calculations. May be different from the raw input vector received from the replication
	/// component (@see PreProcessInputVector).
	FVector ProcessedInputVector{0};

	/// @brief		Allows for pre-processing of the raw input vector. Called after the movement mode was updated.
	///
	/// @param      RawInputVector    The original input vector reflecting the raw input data.
	/// @returns    FVector           The actual input vector to be used for all physics calculations.
	UFUNCTION(BlueprintNativeEvent, Category = "General Movement Component")
	FVector PreProcessInputVector(FVector RawInputVector);
	virtual FVector PreProcessInputVector_Implementation(FVector RawInputVector) { return RawInputVector; }

public:
	/// @brief		Returns the pre-processed input vector (@see PreProcessInputVector).
	///	@attention	Not available until after the movement mode was updated meaning @see PrePhysicsUpdate is the earliest event for which the
	///				return value can be non-zero.
	///
	/// @returns      FVector    The processed input vector used for all physics calculations.
	UFUNCTION(BlueprintCallable, Category = "General Movement Component")
	virtual FVector GetProcessedInputVector() const;

#pragma endregion STEERING_INPUT
};

#pragma region INLINES

FORCEINLINE bool UGenCustomMovementComponent::IsGrounded() const
{
	return GroundingStatus == EGroundingStatus::Grounded || GroundingStatus == EGroundingStatus::None;
}

FORCEINLINE bool UGenCustomMovementComponent::IsAirborne() const
{
	return GroundingStatus == EGroundingStatus::Aerial || GroundingStatus == EGroundingStatus::None;
}

FORCEINLINE EGroundingStatus UGenCustomMovementComponent::GetGroundingStatus() const
{
	return (GroundingStatus);
}

FORCEINLINE FVector UGenCustomMovementComponent::GetProcessedInputVector() const
{
	return ProcessedInputVector;
}

FORCEINLINE bool UGenCustomMovementComponent::HasAnimRootMotion() const
{
	return bHasAnimRootMotion;
}

FORCEINLINE float UGenCustomMovementComponent::GetAnimRootMotionTranslationScale() const
{
	return AnimRootMotionTranslationScale;
}

FORCEINLINE void UGenCustomMovementComponent::SetAnimRootMotionTranslationScale(float Scale)
{
	AnimRootMotionTranslationScale = Scale;
}

#pragma endregion INLINES
