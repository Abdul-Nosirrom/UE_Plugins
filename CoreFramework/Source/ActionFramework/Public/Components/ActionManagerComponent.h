// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ActionSystem/GameplayActionTypes.h"
#include "ActionSystem/GameplayAction.h"
#include "Components/ActorComponent.h"
#include "StateMachineDefinitions/GameplayStateMachine_Base.h"
#include "GameplayTags/Classes/GameplayTagAssetInterface.h"
#include "ActionManagerComponent.generated.h"

/* ~~~~~ Forward Declarations ~~~~~ */
class ARadicalCharacter;
class ALevelPrimitiveActor;
//class UGameplayAction;

DECLARE_STATS_GROUP(TEXT("ActionManager_Game"), STATGROUP_ActionManagerComp, STATCAT_Advanced)


UCLASS(ClassGroup=(StateMachines), meta=(BlueprintSpawnableComponent, DisplayName="Action Manager Component"))
class ACTIONFRAMEWORK_API UActionManagerComponent : public UActorComponent, public IGameplayTagAssetInterface
{
	friend class ARadicalPlayerCharacter;
	friend class UGameplayAction;
	
	GENERATED_BODY()
	
protected:

	UPROPERTY(Category="State Machine", EditDefaultsOnly, BlueprintReadOnly)
	TSubclassOf<UGameplayStateMachine_Base> StateMachineClass;
	
	UPROPERTY(Transient)
	TObjectPtr<UGameplayStateMachine_Base> A_Instance;

	UPROPERTY(Transient, DuplicateTransient)
	TObjectPtr<ARadicalPlayerCharacter> CharacterOwner;
	
public:
	// Sets default values for this component's properties
	UActionManagerComponent();

	// UActorComponent
	virtual void InitializeComponent() override;
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	// ~ UActorComponent
	
	
	
protected:
	void DisplayDebug(class UCanvas* Canvas, const FDebugDisplayInfo& DebugDisplay, float& YL, float& YPos);

#pragma region Level Primitive Interface

protected:
	UPROPERTY(Transient)
	TMap<FGameplayTag, ALevelPrimitiveActor*> ActiveLevelPrimitives;
	
public:
	// Level primitive Interface
	UFUNCTION(BlueprintCallable)
	void RegisterLevelPrimitive(ALevelPrimitiveActor* LP);
	UFUNCTION(BlueprintCallable)
	bool UnRegisterLevelPrimitive(ALevelPrimitiveActor* LP);
	UFUNCTION(BlueprintCallable)
	ALevelPrimitiveActor* GetActiveLevelPrimitive(FGameplayTag LPTag) const;
	// ~ Level primitive Interface

#pragma endregion Level Primitive Interface

#pragma region Gameplay Actions

	/*--------------------------------------------------------------------------------------------------------------
	* Members
	*--------------------------------------------------------------------------------------------------------------*/
public:
	/// @brief  Cached off data about the owning actor that actions will need to frequently access
	UPROPERTY()
	FActionActorInfo ActionActorInfo;
	
protected:
	/// @brief  Actions that have been granted and can be executed
	UPROPERTY()
	TMap<TSubclassOf<UGameplayAction>, UGameplayAction*> ActivatableActions; // NOTE: Same comment as below...
	/// @brief  Currently active actions
	UPROPERTY(Transient)
	TArray<UGameplayAction*> RunningActions; // Maybe make these TMaps w/ TSubClass keys for faster lookup? Googling says size is not a concern, its just a ptr

	/// @brief  Currently active actions which want to listen to CalcVelocity
	UPROPERTY(Transient)
	TArray<UGameplayAction*> RunningCalcVelocityActions;

	/// @brief	Currently active actions which want to listen to UpdateRotation
	UPROPERTY(Transient)
	TArray<UGameplayAction*> RunningUpdateRotationActions;

	/*--------------------------------------------------------------------------------------------------------------
	* Accessors
	*--------------------------------------------------------------------------------------------------------------*/
public:
	/// @brief  Returns list of all activatable actions. Read-only.
	//UFUNCTION(Category="Actions | Accessors", BlueprintPure)
	//const TArray<UGameplayAction*>& GetActivatableActions() const
	//{
	//	return ActivatableActions.valu;
	//}

	/// @brief  Returns a list of all currently running actions. (Ex: When one action wants to know about another)
	UFUNCTION(Category="Actions | Accessors", BlueprintPure)
	const UGameplayAction* GetRunningAction(UPARAM(meta=(BlueprintBaseOnly))TSubclassOf<UGameplayAction> InActionClass) const
	{
		for (const auto Action : RunningActions)
		{
			if (Action->GetClass() == InActionClass) return Action;
		}
		return nullptr;
	}
	
	/// @brief	Given an action class, searches whether an instance has been instantiated in this component.
	/// @return Action instance if it has been previously granted. Null otherwise.
	UFUNCTION(Category="Actions | Accessors", BlueprintCallable)
	UGameplayAction* FindActionInstanceFromClass(UPARAM(meta=(BlueprintBaseOnly))TSubclassOf<UGameplayAction> InActionClass);
	
	/*--------------------------------------------------------------------------------------------------------------
	* Instantiation
	*--------------------------------------------------------------------------------------------------------------*/
public:
	/// @brief  Will grant an action to this component, allowing it to later be performed.
	/// @return Instance of the action created (Honestly liking the SpecHandle approach now that I think about it)
	UFUNCTION(Category=Actions, BlueprintCallable)
	UGameplayAction* GiveAction(UPARAM(meta=(BlueprintBaseOnly))TSubclassOf<UGameplayAction> InActionClass);

	/// @brief  Will grant an action to this component, activate it, then remove it once its completed
	/// @return Instance of the action created
	UFUNCTION(Category=Actions, BlueprintCallable)
	UGameplayAction* GiveActionAndActivateOnce(UPARAM(meta=(BlueprintBaseOnly))TSubclassOf<UGameplayAction> InActionClass);

	/// @brief  Attempts to remove a granted action from this component, preventing it from being performed.
	/// @return True if the action of the given class was ever granted in the first place.
	UFUNCTION(Category=Actions, BlueprintCallable)
	bool RemoveAction(UPARAM(meta=(BlueprintBaseOnly))TSubclassOf<UGameplayAction> InActionClass);
	
protected:
	/// @brief  Internal Use Only. Called by GiveAction, creating an instance of the action class and storing it in the component.
	///			A given action is instanced once per actor and stored for future use (or removal, which it then has to be regranted)
	UGameplayAction* CreateNewInstanceOfAbility(TSubclassOf<UGameplayAction> InActionClass) const; 

	/*--------------------------------------------------------------------------------------------------------------
	* Activation
	*--------------------------------------------------------------------------------------------------------------*/

public:
	
	/// @brief  Will check if the action can be activated, and if it can, it'll activate it
	UFUNCTION(Category=Actions, BlueprintCallable)
	bool TryActivateAbilityByClass(UPARAM(meta=(BlueprintBaseOnly))TSubclassOf<UGameplayAction> InActionToActivate);

protected:
	bool InternalTryActivateAction(UGameplayAction* Action);


	/*--------------------------------------------------------------------------------------------------------------
	* Cancelling and Interrupts
	*--------------------------------------------------------------------------------------------------------------*/


	
	/*--------------------------------------------------------------------------------------------------------------
	* Ability Event Response: 
	*--------------------------------------------------------------------------------------------------------------*/

	/// @brief  Called after an action has been granted NOTE: Multicast delegates perhaps
	//virtual void OnGiveAction(UGameplayAction* Action);
	
	/// @brief  Called from the action to let us know it successfully activated
	virtual void NotifyActionActivated(UGameplayAction* Action);
	/// @brief  Called from the action to let us know it ended
	virtual void NotifyActionEnded(UGameplayAction* Action);

	/*--------------------------------------------------------------------------------------------------------------
	* Movement Component Bindings: Propagate these to the active actions 
	*--------------------------------------------------------------------------------------------------------------*/
public:
	UFUNCTION()
	void CalculateVelocity(URadicalMovementComponent* MovementComponent, float DeltaTime);
	UFUNCTION()
	void UpdateRotation(URadicalMovementComponent* MovementComponent, float DeltaTime);
	UFUNCTION()
	void PostProcessRMVelocity(URadicalMovementComponent* MovementComponent, FVector& Velocity, float DeltaTime);
	UFUNCTION()
	void PostProcessRMRotation(URadicalMovementComponent* MovementComponent, FQuat& Rotation, float DeltaTime);

#pragma endregion Gameplay Actions

#pragma region Gameplay Tag Interface
	
protected:
	UPROPERTY(Category="Action Tags", EditDefaultsOnly, BlueprintReadWrite)
	FGameplayTagContainer GrantedTags;
	
public:
	/*--------------------------------------------------------------------------------------------------------------
	* IGameplayTagInterface
	*--------------------------------------------------------------------------------------------------------------*/

	// NOTE: Can add optional delegate to bind to in case we wanna listen to any added tags from anywhere
	UFUNCTION(BlueprintCallable)
	void AddTag(const FGameplayTag& TagToAdd) { GrantedTags.AddTag(TagToAdd); }
	UFUNCTION(BlueprintCallable)
	void AddTags(const FGameplayTagContainer& TagContainer) { GrantedTags.AppendTags(TagContainer); }
	UFUNCTION(BlueprintCallable)
	bool RemoveTag(const FGameplayTag& TagToRemove) { return GrantedTags.RemoveTag(TagToRemove); }
	UFUNCTION(BlueprintCallable)
	void RemoveTags(const FGameplayTagContainer& TagContainer) { GrantedTags.RemoveTags(TagContainer); }
	
	// IGameplayTagAssetInterface
	FORCEINLINE virtual void GetOwnedGameplayTags(FGameplayTagContainer& TagContainer) const override { TagContainer.AppendTags(GrantedTags); }
	FORCEINLINE virtual bool HasMatchingGameplayTag(FGameplayTag TagToCheck) const override { return GrantedTags.HasTag(TagToCheck); }
	FORCEINLINE virtual bool HasAllMatchingGameplayTags(const FGameplayTagContainer& TagContainer) const override { return GrantedTags.HasAll(TagContainer); }
	FORCEINLINE virtual bool HasAnyMatchingGameplayTags(const FGameplayTagContainer& TagContainer) const override { return GrantedTags.HasAny(TagContainer); }
	// ~ IGameplayTagAssetInterface

#pragma endregion Gameplay Tag Interface
};
