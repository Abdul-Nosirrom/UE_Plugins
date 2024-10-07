// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "NativeGameplayTags.h"
#include "Components/BillboardComponent.h"
#include "InteractableComponent.generated.h"


/* Define Base Gameplay Tag Category */
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Interaction_Base)


/* Forward Declarations */
class UActionSystemComponent;
class URadicalMovementComponent;
class ARadicalCharacter;
class UInputAction;
class UInteractionBehavior;
class UInteractionCondition;
class UInteractionTrigger;
enum class EInteractionDomain : uint8;

/* Events */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FInteractionInitializationSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FInteractionStateChangedSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FInteractionUpdateSignature, float, DeltaTime);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FUpdatePlayerPhysicsSignature, URadicalMovementComponent*, MovementComponent, float, DeltaTime);

/* Definition Enums */

UENUM(BlueprintType)
enum class EInteractionState : uint8
{
	NotStarted,
	Viable,
	InProgress
};

UENUM(BlueprintType)
enum class EInteractionLockStatus : uint8
{
	Unlocked,
	Locked
};

UENUM(BlueprintType)
enum class EInteractionStopTrigger : uint8 
{
	/// @brief  Will call StopInteraction after UpdateBehaviors are complete, then properly stop when EndBehaviors are complete
	Automatic,
	/// @brief  Requires manually calling StopInteraction 
	Manual
};

UCLASS(ClassGroup=(Interactables), Blueprintable, PrioritizeCategories=(Status, Behavior), hidecategories=(Sprite, HLOD, Rendering, Navigation, Cooking, Object,LOD,Lighting,Transform,Sockets,TextureStreaming, Mobile, RayTracing, AssetUserData), meta=(BlueprintSpawnableComponent))
class INTERACTIONFRAMEWORK_API UInteractableComponent : public UBillboardComponent
{
	GENERATED_BODY()

	friend class UInteractionManager;
	friend class UInteractionScript;
	friend class UInteractionBehavior;
	friend class FInteractableComponentVisualizer;
	
public:
	// Sets default values for this component's properties
	UInteractableComponent();

public:
	virtual void BeginPlay() override;

	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
	                           FActorComponentTickFunction* ThisTickFunction) override;

protected:
	/*--------------------------------------------------------------------------------------------------------------
	* Setup
	*--------------------------------------------------------------------------------------------------------------*/
	
	UPROPERTY(Transient, BlueprintReadWrite, meta=(AllowPrivateAccess=true))
	TWeakObjectPtr<ARadicalCharacter> PlayerCharacter;
	UPROPERTY(Transient, BlueprintReadWrite, meta=(AllowPrivateAccess=true))
	TWeakObjectPtr<UActionSystemComponent> ActionSystemComponent;

	/*
	virtual void OnRegister() override;

	virtual void OnComponentDestroyed(bool bDestroyingHierarchy) override;
	*/
	
	/*--------------------------------------------------------------------------------------------------------------
	* Shared
	*--------------------------------------------------------------------------------------------------------------*/

#if WITH_EDITOR 
	/// @brief  Helper to just spawn transform markers for the behaviors
	UFUNCTION(Category=Behavior, CallInEditor)
	void SpawnTransformMarker() const;
#endif

	/// @brief	If set, will use the custom widget provided as the interaction prompt
	UPROPERTY(Category=Behavior, EditAnywhere, BlueprintReadOnly)
	TSubclassOf<UUserWidget> OverrideInteractionPrompt;
	/// @brief  If false and StopInteraction is called externally, result won't go through.
	UPROPERTY(Category=Behavior, EditAnywhere, BlueprintReadOnly)
	bool bAllowStopExternal;
	/// @brief  Determines how "StopInteraction" is called. If automatic, will wait until all behaviors complete. If manual,
	///			explicitly calling "StopInteraction" is required to go into the "OnInteractionEnd" domain.
	UPROPERTY(Category=Behavior, EditDefaultsOnly)
	EInteractionStopTrigger StopTrigger;
	/// @brief  How this interactable can be triggered. If nothing is specified, "InitializeInteraction" has to be called manually
	///			from somewhere to start this interactable
	UPROPERTY(Category=Behavior, EditAnywhere, Instanced)
	UInteractionTrigger* InteractionTrigger;
	/// @brief  All these conditions must pass for the interactable to start once its been called
	UPROPERTY(Category=Behavior, EditAnywhere, Instanced)
	TArray<UInteractionCondition*> InteractionConditions;
	/// @brief  Behaviors to run on StartInteraction. Can have behaviors that run for a duration here, but it means you have to wait
	///			for them to complete before we move onto running the UpdateBehaviors
	UPROPERTY(Category=Behavior, EditAnywhere, Instanced)
	TArray<UInteractionBehavior*> StartBehaviors;
	/// @brief  Behaviors to run on UpdateInteraction. If "StopTrigger" is set to automatic, once these behaviors complete we automatically
	///			call "StopInteraction". Otherwise, if Manual, we just stop running behaviors when these are complete until StopInteraction is called externally
	UPROPERTY(Category=Behavior, EditAnywhere, Instanced)
	TArray<UInteractionBehavior*> UpdateBehaviors;
	/// @brief  Behaviors to run on EndInteraction. Can have behaviors that run for a duration here, and in that case we have to wait until they finish running
	///			before the interactable actually stops and broadcasts "OnInteractionStopped".
	UPROPERTY(Category=Behavior, EditAnywhere, Instanced)
	TArray<UInteractionBehavior*> EndBehaviors;
	
	UPROPERTY()
	EInteractionState InteractionState;
	UPROPERTY()
	EInteractionLockStatus LockStatus = EInteractionLockStatus::Unlocked;
	UPROPERTY()
	EInteractionDomain InteractionDomain;

	
	/// @brief  If stop trigger is manual and it requested a stop while EndBehaviors haven't fully ran, we set this flag so we can call StopInteraction after EndBehaviors ran.
	bool bStopRequested;
	
	void UpdateInteractionBehavior(float DeltaTime);

	void SetInteractionState(EInteractionState NewState);

	void SetInteractionDomain(EInteractionDomain NewDomain);

public:
	
	UFUNCTION(Category=Interactions, BlueprintCallable)
	void InitializeInteraction();
	UFUNCTION(Category=Interactions, BlueprintCallable)
	void StopInteraction(bool bCalledExternally = false);

	UFUNCTION(Category=Interactions, BlueprintPure)
	EInteractionState GetInteractionState() const { return InteractionState; }

	UFUNCTION(Category=Interactions, BlueprintPure)
	EInteractionLockStatus GetInteractionLockStatus() const { return LockStatus; }

	void SetInteractionLockStatus(EInteractionLockStatus InStatus);

	bool IsInteractionValid() const;
	
	/*--------------------------------------------------------------------------------------------------------------
	* Virtual Behavior
	*--------------------------------------------------------------------------------------------------------------*/
public:
	UPROPERTY(Category=Interactions, BlueprintAssignable)
	FInteractionStateChangedSignature OnInteractableBecomeViable;
	UPROPERTY(Category=Interactions, BlueprintAssignable)
	FInteractionStateChangedSignature OnInteractableNoLongerViable;

	UPROPERTY(Category=Interactions, BlueprintAssignable)
	FInteractionStateChangedSignature OnInteractableUnlocked;
	UPROPERTY(Category=Interactions, BlueprintAssignable)
	FInteractionStateChangedSignature OnInteractableLocked;
	
	UPROPERTY(Category=Interactions, BlueprintAssignable)
	FInteractionInitializationSignature OnInteractionStartedEvent;
	UPROPERTY(Category=Interactions, BlueprintAssignable)
	FInteractionUpdateSignature OnInteractionUpdateEvent;
	UPROPERTY(Category=Interactions, BlueprintAssignable)
	FInteractionInitializationSignature OnInteractionEndedEvent;

	/* There is a template action that basically invokes this in its CalcVel & UpdateRot */
	UPROPERTY(Category=Interactions, BlueprintAssignable)
	FUpdatePlayerPhysicsSignature CalcPlayerVelocityEvent;
	UPROPERTY(Category=Interactions, BlueprintAssignable)
	FUpdatePlayerPhysicsSignature UpdatePlayerRotationEvent;

private:

	// NOTE: These are used to notify behaviors when we first start and when we fully end, before anything is executed. Since the regular OnStart only executes after Start behaviors complete
	FInteractionInitializationSignature Internal_StartEvent;

	UPROPERTY()
	TArray<TWeakObjectPtr<AActor>> CachedSpawnedMarkers;
	
protected:
	UFUNCTION(Category=Interactions, BlueprintImplementableEvent)
	void OnInteractionStarted();

	UFUNCTION(Category=Interactions, BlueprintImplementableEvent)
	void OnInteractionUpdate(float DeltaTime);

	UFUNCTION(Category=Interactions, BlueprintImplementableEvent)
	void OnInteractionEnded();


#pragma region Debug
#if WITH_EDITORONLY_DATA
	/// @brief	Allow us to show the Status of the class (valid configurations or invalid configurations) while configuring in the Editor 
	UPROPERTY(Category = Status, VisibleAnywhere, Transient)
	mutable FText EditorStatusText;
#endif

#if WITH_EDITOR

	bool bWarningsMsgsIssued;
	EDataValidationResult ValidationResult;

	virtual void PostLoad() override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) override;
	virtual EDataValidationResult IsDataValid(TArray<FText>& ValidationErrors) override;

	TArray<AActor*> GetReferences() const;
	bool IsActorReferenced(AActor* ActorQuery) const;
#endif 
#pragma endregion
 
};

