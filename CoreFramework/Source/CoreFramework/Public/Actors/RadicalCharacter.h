// Copyright 2023 Abdulrahmen Almodaimegh. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Character.h"
#include "RadicalCharacter.generated.h"

/* Forward declarations */
class URadicalMovementComponent;
class UCapsuleComponent;
class URootMotionTasksComponent;
class UArrowComponent;
class URootMotionTask_Base;
class UInputBufferSubsystem;
enum EMovementState : int;

/* Delegate Declarations */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMovementStateChangedSignature, class ARadicalCharacter*, Character, EMovementState, PrevMovementState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLandedSignature, const FHitResult&, Hit);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FLostFloorStabilitySignature, const FHitResult&, Hit);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FWalkedOffLedge, const FVector&, PreviousFloorImpactNormal, const FVector&, PreviousFloorContactNormal, const FVector&, PreviousLocation, float, DeltaTime);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMoveBlockedBySignature, const FHitResult&, Hit);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FStuckInGeometrySignature, const FHitResult&, Hit);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FCrouchIndicatorSignature, float, HalfHeightAdjustment, float, ScaledHalfHeightAdjustment);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FReachedJumpApex);

UCLASS()
class COREFRAMEWORK_API ARadicalCharacter : public APawn
{
	GENERATED_BODY()
	
public:
	// Sets default values for this pawn's properties
	ARadicalCharacter(const FObjectInitializer& ObjectInitializer);

	virtual void TickActor(float DeltaTime, ELevelTick TickType, FActorTickFunction& ThisTickFunction) override;

#pragma region Primary Components

protected:
	/// @brief Main skeletal mesh associated with this Character
	UPROPERTY(Category=Character, VisibleAnywhere, BlueprintReadOnly, meta=(AllowPrivateAccess="true"))
	TObjectPtr<USkeletalMeshComponent> Mesh;
	
	/// @brief Name of the mesh component used when creating the subobject 
	static FName MeshComponentName;
	
	/// @brief Movement component used for movement logic, containing all movement handling logic
	UPROPERTY(Category=Character, VisibleAnywhere, BlueprintReadOnly, meta=(AllowPrivateAccess="true"))
	TObjectPtr<URadicalMovementComponent> MovementComponent; 

	/// @brief Name of the movement component used when creating the subobject 
	static FName MovementComponentName;
	
	/// @brief The CapsuleComponent being used for movement collision (by MovementComponent). Alignment arbitrary.
	UPROPERTY(Category=Character, VisibleAnywhere, BlueprintReadOnly, meta=(AllowPrivateAccess="true"))
	TObjectPtr<UCapsuleComponent> CapsuleComponent;

	/// @brief Name of the capsule component used when creating the subobject 
	static FName CapsuleComponentName;

#if WITH_EDITORONLY_DATA
	/// @brief Component shown in the editor only to indicate character facing
	UPROPERTY()
	TObjectPtr<UArrowComponent> ArrowComponent;
#endif

#pragma endregion Primary Components
	
#pragma region Primary Component Getters
public:
	/// @brief Getter for characters skeletal mesh component
	FORCEINLINE USkeletalMeshComponent* GetMesh() const { return Mesh; }

	/// @brief Getter for characters movement component
	FORCEINLINE URadicalMovementComponent* GetCharacterMovement() const { return MovementComponent; } 
	
	/// @brief Getter for characters capsule component
	FORCEINLINE UCapsuleComponent* GetCapsuleComponent() const { return CapsuleComponent; }

	/// @brief  Getter for local players InputBuffer, only valid if controlled by a player controller
	UFUNCTION(Category="Input", BlueprintPure)
	UInputBufferSubsystem* GetInputBuffer() const;
#pragma endregion Primary Component Getters

#pragma region AActor & UObject Interface

	//~ Begin UObject Interface.
	virtual void PostLoad() override;
	//~ End UObject Interface
	
	//~ Begin AActor Interface.
	virtual void BeginPlay() override;
	virtual void ClearCrossLevelReferences() override;
	virtual void GetSimpleCollisionCylinder(float& CollisionRadius, float& CollisionHalfHeight) const override;
	virtual UActorComponent* FindComponentByClass(const TSubclassOf<UActorComponent> ComponentClass) const override;
	virtual void NotifyActorBeginOverlap(AActor* OtherActor);
	virtual void NotifyActorEndOverlap(AActor* OtherActor);
	//~ End AActor Interface

	template<class T>
	T* FindComponentByClass() const
	{
		return AActor::FindComponentByClass<T>();
	}

	//~ Begin INavAgentInterface Interface
	//virtual FVector GetNavAgentLocation() const override;
	//~ End INavAgentInterface Interface

	
#pragma endregion AActor & UObject Interface

#pragma region APawn Interface

	//~ Begin APawn Interface.
	virtual void PostInitializeComponents() override;
	virtual UPawnMovementComponent* GetMovementComponent() const override;
	virtual UPrimitiveComponent* GetMovementBase() const override final;
	virtual FVector GetGravityDirection() const override;
	virtual float GetDefaultHalfHeight() const override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	virtual void DisplayDebug(class UCanvas* Canvas, const FDebugDisplayInfo& DebugDisplay, float& YL, float& YPos) override;
	virtual void RecalculateBaseEyeHeight() override;
	virtual void UpdateNavigationRelevance() override;
	//~ End APawn Interface

#pragma endregion APawn Interface

#pragma region Gameplay Interface

	UFUNCTION(Category="Character", BlueprintCallable)
	virtual void LaunchCharacter(FVector LaunchVelocity, bool bPlanarOverride, bool bVerticalOverride);

	UFUNCTION(Category="Character", BlueprintCallable)
	virtual void SetHitStop(float Duration = 0.1f);
	
#pragma endregion Gameplay Interface

#pragma region Events
public:
	UPROPERTY(Category="Character", BlueprintAssignable)
	FMovementStateChangedSignature MovementStateChangedDelegate;
	
	UPROPERTY(Category="Character", BlueprintAssignable)
	FOnLandedSignature LandedDelegate;
	
	UPROPERTY(Category="Character", BlueprintAssignable)
	FLostFloorStabilitySignature LostStabilityDelegate;
	
	UPROPERTY(Category="Character", BlueprintAssignable)
	FWalkedOffLedge WalkedOffLedgeDelegate;

	UPROPERTY(Category="Character", BlueprintAssignable)
	FMoveBlockedBySignature MoveBlockedByDelegate;

	UPROPERTY(Category="Character", BlueprintAssignable)
	FStuckInGeometrySignature StuckInGeometryDelegate;

	UPROPERTY(Category="Character", BlueprintAssignable)
	FCrouchIndicatorSignature CrouchStartedDelegate;

	UPROPERTY(Category="Character", BlueprintAssignable)
	FCrouchIndicatorSignature CrouchEndedDelegate;
	
	UPROPERTY(Category="Character", BlueprintAssignable)
	FReachedJumpApex ReachedJumpApexDelegate;

public:
	UFUNCTION(Category="Character", BlueprintImplementableEvent)
	void OnLanded(const FHitResult& Hit);
	virtual void Landed(const FHitResult& Hit);

	UFUNCTION(Category="Character", BlueprintImplementableEvent)
	void OnMovementStateChanged(enum EMovementState PrevMovementState);
	virtual void MovementStateChanged(enum EMovementState PrevMovementState);

	UFUNCTION(Category="Character", BlueprintImplementableEvent)
	void OnWalkingOffLedge(const FVector& PreviousFloorImpactNormal, const FVector& PreviousFloorContactNormal, const FVector& PreviousLocation, float DeltaTime);
	virtual void WalkingOffLedge(const FVector& PreviousFloorImpactNormal, const FVector& PreviousFloorContactNormal, const FVector& PreviousLocation, float DeltaTime);

	UFUNCTION(Category="Character", BlueprintImplementableEvent)
	void OnReachedJumpApex();
	virtual void ReachedJumpApex();
	
	UFUNCTION(Category="Character", BlueprintImplementableEvent)
	void OnMoveBlocked(const FHitResult& Hit);
	virtual void MoveBlockedBy(const FHitResult& Hit);

	void OnStuckInGeometry(const FHitResult& Hit);

	UFUNCTION(Category="Character", BlueprintImplementableEvent, meta=(DisplayName="OnStartCrouch"))
	void K2_OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust);
	virtual void OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust);

	UFUNCTION(Category="Character", BlueprintImplementableEvent, meta=(DisplayName="OnEndCrouch"))
	void K2_OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust);
	virtual void OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust);

	UFUNCTION(BlueprintImplementableEvent)
	void BaseChange();

	UFUNCTION(BlueprintImplementableEvent)
	void OnLaunched(FVector LaunchVelocity, bool bPlanarOverride, bool bVerticalOverride);

#pragma endregion Events

#pragma region Crouching

public:
	
	/// @brief	Set by character movement to specify that this character is currently crouched.
	UPROPERTY(Category="Character", BlueprintReadOnly)
	uint8 bIsCrouched	: 1;

	/// @brief	Requests the movement component to start crouching. Sets bWantsToCrouch to true, notified when OnStartCrouch is called.
	UFUNCTION(Category="Character", BlueprintCallable)
	virtual void Crouch();

	/// @brief	Requests the movement component to stop crouching. Sets bWantsToCrouch to false, notified when OnEndCrouch is called.
	UFUNCTION(Category="Character", BlueprintCallable)
	virtual void UnCrouch();

	/// @brief	True if the character is currently not crouched, but is able to if crouch is requested.
	UFUNCTION(Category="Character", BlueprintPure)
	virtual bool CanCrouch();

	/// @brief	True if the character is currently crouching
	UFUNCTION(Category="Character", BlueprintPure)
	virtual bool IsCrouching();
	
#pragma endregion Crouching
	
#pragma region Based Movement

protected:
	
	UPROPERTY()
	FBasedMovementInfo BasedMovement;

	UPROPERTY()
	FBasedMovementInfo BasedMovementOverride;

	uint8 bHasBasedMovementOverride			: 1;

	void CreateBasedMovementInfo(FBasedMovementInfo& BasedMovementInfoToFill, UPrimitiveComponent* BaseComponent, const FName BoneName);

public:
	
	virtual void SetBase(UPrimitiveComponent* NewBase, const FName InBoneName = NAME_None, bool bNotifyActor=true);

	UFUNCTION(Category="Based Movement", BlueprintCallable)
	virtual void SetBaseOverride(UPrimitiveComponent* NewBase, const FName InBoneName = NAME_None); // Do not notify in the case of setting an override

	UFUNCTION(Category="Based Movement", BlueprintCallable)
	virtual void RemoveBaseOverride();

	const FBasedMovementInfo& GetBasedMovement() const;

	/** Save a new relative location in BasedMovement and a new rotation with is either relative or absolute. */
	void SaveRelativeBasedMovement(const FVector& NewRelativeLocation, const FRotator& NewRotation, bool bRelativeRotation);

	FORCEINLINE bool HasBasedMovementOverride() const { return bHasBasedMovementOverride; }

#pragma endregion Based Movement

#pragma region Animation Interface
	
	/// @brief	Scale to apply to root motion translation on this Character 
	UPROPERTY()
	float AnimRootMotionTranslationScale;

	/// @brief	Saved translation offset of mesh. (Primarily used for crouch)
	UPROPERTY()
	FVector BaseMeshTranslationOffset;

	/// @brief	Saved rotation offset of mesh. 
	UPROPERTY()
	FQuat BaseMeshRotationOffset;
	
	/// @brief	Get the saved translation offset of mesh. This is how much extra offset is applied from the center of the capsule. 
	UFUNCTION(Category=Animation, BlueprintPure)
	FVector GetBaseMeshTranslationOffset() const { return BaseMeshTranslationOffset; }

	/// @brief	Get the saved rotation offset of mesh. This is how much extra rotation is applied from the capsule rotation. */
	virtual FQuat GetBaseMeshRotationOffset() const { return BaseMeshRotationOffset; }

	/// @brief	Get the saved rotation offset of mesh. This is how much extra rotation is applied from the capsule rotation. */
	UFUNCTION(Category=Animation, BlueprintPure, meta=(DisplayName="Get Base Mesh Rotation Offset", ScriptName="GetBaseMeshRotationOffset"))
	FRotator GetBaseMeshRotationOffsetRotator() const { return GetBaseMeshRotationOffset().Rotator(); }

	UFUNCTION(Category=Animation, BlueprintCallable)
	void SetAnimRootMotionTranslationScale(float InAnimRootMotionTranslationScale = 1.f);

	UFUNCTION(Category=Animation, BlueprintCallable)
	float GetAnimRootMotionTranslationScale() const;

	/** Play Animation Montage on the character mesh. Returns the length of the animation montage in seconds, or 0.f if failed to play. **/
	UFUNCTION(Category=Animation, BlueprintCallable)
	virtual float PlayAnimMontage(class UAnimMontage* AnimMontage, float InPlayRate = 1.f, FName StartSectionName = NAME_None);

	/** Stop Animation Montage. If nullptr, it will stop what's currently active. The Blend Out Time is taken from the montage asset that is being stopped. **/
	UFUNCTION(Category=Animation, BlueprintCallable)
	virtual void StopAnimMontage(class UAnimMontage* AnimMontage = nullptr);

	/** Return current playing Montage **/
	UFUNCTION(Category=Animation, BlueprintCallable)
	class UAnimMontage* GetCurrentMontage() const;

	/** Get FAnimMontageInstance playing RootMotion */
	FAnimMontageInstance * GetRootMotionAnimMontageInstance() const;

	/** True if we are playing Anim root motion right now */
	UFUNCTION(Category=Animation, BlueprintCallable, meta=(DisplayName="Is Playing Anim Root Motion"))
	bool IsPlayingRootMotion() const;

	/** True if we are playing root motion from any source right now (anim root motion, root motion source) */
	UFUNCTION(Category=Animation, BlueprintCallable)
	bool HasAnyRootMotion() const;

#pragma endregion Animation Interface

#pragma region Root Motion Tasks Interface
public:
	
	/* Helper function for initializing new root motion tasks */
	template<class T>
	static T* NewRootMotionTask(ARadicalCharacter* OwningActor, FName InstanceName = FName())
	{
		check(OwningActor);

		T* MyObj = NewObject<T>();
		MyObj->InitTask(OwningActor);

		MyObj->ForceName = InstanceName;
		return MyObj;
	}

#pragma endregion Root Motion Tasks Interface
	
};
