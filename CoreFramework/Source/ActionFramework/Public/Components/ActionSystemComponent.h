// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ActionSystem/GameplayActionTypes.h"
#include "ActionSystem/GameplayAction.h"
#include "Components/ActorComponent.h"
#include "GameplayTagAssetInterface.h"
#include "Data/GameplayTagData.h"
#include "RadicalMovementComponent.h"
#include "ActionSystemComponent.generated.h"


/* ~~~~~ Event Signatures ~~~~~ */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FGameplayTagsAddedSignature, FGameplayTagContainer, AddedTag);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FGameplayTagsRemovedSignature, FGameplayTagContainer, RemovedTag);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FActionGrantedSignature, UGameplayActionData*, GrantedAction);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FActionActivatedSignature, UGameplayActionData*, ActivatedAction);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FActionEndedSignature, UGameplayActionData*, EndedAction);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FLevelPrimitiveRegisteredSignature, FGameplayTag, LevelPrimitiveTag);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FLevelPrimitiveUnRegisteredSignature, FGameplayTag, LevelPrimitiveTag);

/* ~~~~~ Forward Declarations ~~~~~ */
class ARadicalCharacter;
class ULevelPrimitiveComponent;
class UInteractableComponent;
//class UGameplayAction;

UCLASS(ClassGroup=(StateMachines), meta=(BlueprintSpawnableComponent, DisplayName="Action System Component"), hidecategories=(Object,LOD,Lighting,Transform,Sockets,TextureStreaming))
class ACTIONFRAMEWORK_API UActionSystemComponent : public UActorComponent, public IGameplayTagAssetInterface
{
	friend class FGameplayWindow_Actions;
	friend class ARadicalCharacter;
	friend class UGameplayAction;
	
	GENERATED_BODY()
	
public:
	// Sets default values for this component's properties
	UActionSystemComponent();

	// UActorComponent
	virtual void InitializeComponent() override;
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void OnUnregister() override;
	// ~ UActorComponent
	
	// Getters & Setters
	FORCEINLINE ARadicalCharacter* GetCharacterOwner() const { return CharacterOwner; }
	FORCEINLINE FGameplayTagContainer GetPrevAction() const { return PrevAction; }
	FORCEINLINE void SetCharacterOwner(ARadicalCharacter* InOwner) { CharacterOwner = InOwner; }

protected:
	
	UPROPERTY(Transient, DuplicateTransient)
	TObjectPtr<ARadicalCharacter> CharacterOwner;

	UPROPERTY(Transient)
	FGameplayTagContainer PrevAction;
	
public:
	virtual void DisplayDebug(class UCanvas* Canvas, const FDebugDisplayInfo& DebugDisplay, float& YL, float& YPos);

#pragma region Event Declarations

	UPROPERTY(Category=ActionEvents, BlueprintAssignable)
	FGameplayTagsAddedSignature OnGameplayTagAddedDelegate;
	UPROPERTY(Category=ActionEvents, BlueprintAssignable)
	FGameplayTagsRemovedSignature OnGameplayTagRemovedDelegate;

	UPROPERTY(Category=ActionEvents, BlueprintAssignable)
	FActionGrantedSignature OnActionGrantedDelegate;
	UPROPERTY(Category=ActionEvents, BlueprintAssignable)
	FActionActivatedSignature OnActionActivatedDelegate;
	UPROPERTY(Category=ActionEvents, BlueprintAssignable)
	FActionEndedSignature OnActionEndedDelegate;

	UPROPERTY(Category=ActionEvents, BlueprintAssignable)
	FLevelPrimitiveRegisteredSignature OnLevelPrimitiveRegisteredDelegate;
	UPROPERTY(Category=ActionEvents, BlueprintAssignable)
	FLevelPrimitiveUnRegisteredSignature OnLevelPrimitiveUnRegisteredDelegate;

#pragma endregion Event Declarations
	
#pragma region Level Primitive Interface

protected:
	UPROPERTY(Transient)
	TMap<FGameplayTag, ULevelPrimitiveComponent*> ActiveLevelPrimitives;
	
public:
	// Level primitive Interface
	UFUNCTION(BlueprintCallable)
	void RegisterLevelPrimitive(ULevelPrimitiveComponent* LP, FGameplayTag PrimitiveTag);
	UFUNCTION(BlueprintCallable)
	bool UnRegisterLevelPrimitive(ULevelPrimitiveComponent* LP, FGameplayTag PrimitiveTag);
	UFUNCTION(BlueprintCallable)
	ULevelPrimitiveComponent* GetActiveLevelPrimitive(FGameplayTag LPTag) const;
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

	UPROPERTY(Category="Action Set", EditDefaultsOnly, BlueprintReadOnly)
	UCharacterActionSet* ActionSet;
	
	/// @brief  Actions that have been granted and can be executed
	UPROPERTY()
	TMap<UGameplayActionData*, UGameplayAction*> ActivatableActions; // NOTE: Same comment as below...
	
	/// @brief  Currently running primary action
	UPROPERTY(Transient)
	UPrimaryAction* PrimaryActionRunning;

	/// @brief	Previous running primary action
	UPROPERTY(Transient)
	UPrimaryAction* PreviousPrimaryAction;
	
	/// @brief  Currently running additive actions
	UPROPERTY(Transient)
	TArray<UAdditiveAction*> RunningAdditiveActions; // Whats the distinction between additive & Passive?	
	
	/// @brief  Actions that have registered so that we try to activate them on tick. That is, calling TryActivate on them on tick.
	UPROPERTY(Transient)
	TArray<UGameplayAction*> ActionsAwaitingActivation;

	/// @brief	Specific actions that have been blocked. TSet for fast lookup and queries
	UPROPERTY()
	TSet<const UGameplayActionData*> BlockedActions;

	float TimeLastPrimaryActivated;
	
	/*--------------------------------------------------------------------------------------------------------------
	* Accessors
	*--------------------------------------------------------------------------------------------------------------*/
public:
	/// @brief  Returns list of all activatable actions. Read-only.
	UFUNCTION(Category="Actions | Accessors", BlueprintPure)
	const TMap<UGameplayActionData*, UGameplayAction*>& GetActivatableActions() const
	{
		return  ActivatableActions;
	}

	/// @brief	Returns the action data of the currently running primary action, or null
	UFUNCTION(Category="Actions | Accessors", BlueprintPure)
	const UGameplayActionData* GetPrimaryActionData() const
	{
		if (PrimaryActionRunning) return PrimaryActionRunning->GetActionData();
		return nullptr;
	}

	/// @brief	Returns the action instance of the currently running primary action, or null
	UFUNCTION(Category="Actions | Accessors", BlueprintPure)
	UGameplayAction* GetPrimaryActionInstance() const
	{
		return PrimaryActionRunning;
	}

	/// @brief	Returns the action instance of the previous running primary action, or null
	UFUNCTION(Category="Actions | Accessors", BlueprintPure)
	UGameplayAction* GetPreviousPrimaryAction() const
	{
		return PreviousPrimaryAction;
	}

	UFUNCTION(Category="Actions | Accessors", BlueprintPure)
	float GetTimeSinceLastPrimaryActionActivated()
	{
		return GetWorld()->GetTimeSeconds() - TimeLastPrimaryActivated;
	}
	
	/// @brief  Returns a list of all currently running actions. (Ex: When one action wants to know about another)
	UFUNCTION(Category="Actions | Accessors", BlueprintPure)
	virtual const UGameplayAction* GetRunningAction(const UGameplayActionData* InActionData) const
	{
		if (PrimaryActionRunning && PrimaryActionRunning->GetActionData() == InActionData) return PrimaryActionRunning;
		
		for (int i = 0; i < RunningAdditiveActions.Num(); i++)
		{
			if (InActionData == RunningAdditiveActions[i]->GetActionData()) return RunningAdditiveActions[i];
		}
		
		return nullptr;
	}

	/// @brief  Returns a running action with the matching tag definition. If multiple are running with the same tag it'll return he first one found
	UFUNCTION(Category="Actions | Accessors", BlueprintPure, meta=(GameplayTagFilter="Action.Def"))
	virtual const UGameplayAction* GetRunningActionByTag(const FGameplayTag& ActionTag)
	{
		if (PrimaryActionRunning && PrimaryActionRunning->GetActionData()->HasTag(ActionTag)) return PrimaryActionRunning;
		
		for (int i = 0; i < RunningAdditiveActions.Num(); i++)
		{
			if (RunningAdditiveActions[i]->GetActionData()->HasTag(ActionTag)) return RunningAdditiveActions[i];
		}
		
		return nullptr;
	}
	
	/// @brief	Given an action class, searches whether an instance has been instantiated in this component.
	/// @return Action instance if it has been previously granted. Null otherwise.
	UFUNCTION(Category="Actions | Accessors", BlueprintCallable)
	UGameplayAction* FindActionInstanceFromClass(UGameplayActionData* InActionData);
	
	/*--------------------------------------------------------------------------------------------------------------
	* Instantiation
	*--------------------------------------------------------------------------------------------------------------*/
public:
	/// @brief  Will grant an action to this component, allowing it to later be performed.
	/// @return Instance of the action created (Honestly liking the SpecHandle approach now that I think about it)(nvm)
	UFUNCTION(Category=Actions, BlueprintCallable)
	UGameplayAction* GiveAction(UGameplayActionData* InActionData);

	/// @brief  Will grant an action to this component, activate it, then remove it once its completed
	/// @return Instance of the action created
	UFUNCTION(Category=Actions, BlueprintCallable)
	UGameplayAction* GiveActionAndActivateOnce(UGameplayActionData* InActionData);

	/// @brief  Attempts to remove a granted action from this component, preventing it from being performed.
	/// @return True if the action of the given class was ever granted in the first place.
	UFUNCTION(Category=Actions, BlueprintCallable)
	bool RemoveAction(UGameplayActionData* InActionData);
	
protected:
	/// @brief  Internal Use Only. Called by GiveAction, creating an instance of the action class and storing it in the component.
	///			A given action is instanced once per actor and stored for future use (or removal, which it then has to be regranted)
	UGameplayAction* CreateNewInstanceOfAbility(UGameplayActionData* InActionData) const; 

	/*--------------------------------------------------------------------------------------------------------------
	* Activation
	*--------------------------------------------------------------------------------------------------------------*/

public:
	
	/// @brief  Will check if the action can be activated, and if it can, it'll activate it
	UFUNCTION(Category=Actions, BlueprintCallable, meta=(HidePin=bQueryOnly))
	bool TryActivateAbilityByClass(UGameplayActionData* InActionData, bool bQueryOnly = false);

protected:
	virtual bool InternalTryActivateAction(UGameplayAction* Action, bool bQueryOnly = false);

	/*--------------------------------------------------------------------------------------------------------------
	* Action Selector
	*--------------------------------------------------------------------------------------------------------------*/
protected:
	
	/// @brief  Called On Begin Play to initialize our starting actions from the action-set
	void InitializeCharacterActionSet();
	
	/// @brief  Called On Tick to select any new actions that are not input-bound
	virtual void UpdateActionSelection();

public:
	
	/// @brief  Registers an action to be considered for selection on tick. Calling on TryActivate until its unregistered
	void RegisterActionExecution(UGameplayAction* InAction);
	
	/// @brief  Removes an action from being selected during tick evaluation
	void UnRegisterActionExecution(UGameplayAction* InAction);

protected:
	
	void SwapDefaultAction(FGameplayTag DefaultTag, int Count);

	UGameplayActionData* GetCurrentDefaultAction() const;

	/*--------------------------------------------------------------------------------------------------------------
	* Cancelling and Interrupts
	*--------------------------------------------------------------------------------------------------------------*/
public:
	/// @brief  Given an action, will check if it's currently active. If so, it will force cancel it.
	UFUNCTION(Category=Actions, BlueprintCallable)
	void CancelAction(UGameplayActionData* Action);

	/// @brief	Will cancel all currently running actions with any matching CancelTags in their ActionTags container
	UFUNCTION(Category=Actions, BlueprintCallable)
	void CancelActionsWithTags(const FGameplayTagContainer& CancelTags, const UGameplayActionData* Ignore=nullptr);

	/// @brief	Will cancel all actions running that have any of CancelTags in OwnerRequiredTags
	UFUNCTION(Category=Actions, BlueprintCallable)
	void CancelActionsWithTagRequirement(const FGameplayTagContainer& CancelTags);
	
	/// @brief	Will cancel all running actions except the ignored one
	UFUNCTION(Category=Actions, BlueprintCallable)
	void CancelAllActions(const UGameplayActionData* Ignore=nullptr);
	
	/// @brief  Given an actions ActionTags, will check if it's currently blocked from activation, parent tags suffice
	UFUNCTION(Category=Actions, BlueprintCallable)
	FORCEINLINE bool AreActionTagsBlocked(const FGameplayTagContainer& Tags) const
	{
		// NOTE: Change is just to allow us to block tags using parents, so blocking Action.Def blocks all actions for example, other way around wont allow parents in BlockedTags to affect the query
		return Tags.HasAny(BlockedTags.GetExplicitGameplayTags());
		//return BlockedTags.HasAnyMatchingGameplayTags(Tags);
	}

	/// @brief	Adds tags to the BlockActionsWithTags local container
	UFUNCTION(Category=Actions, BlueprintCallable)
	FORCEINLINE void BlockActionsWithTags(const FGameplayTagContainer& Tags) { BlockedTags.UpdateTagCount(Tags, 1); }

	/// @brief	Blocks action of specific types
	UFUNCTION(Category=Actions, BlueprintCallable)
	FORCEINLINE void BlockActionByClass(const UGameplayActionData* ActionToBlock) { BlockedActions.Add(ActionToBlock); }
	
	/// @brief	Remove tags from the BlockActionsWithTags local container
	UFUNCTION(Category=Actions, BlueprintCallable)
	FORCEINLINE void UnBlockActionsWithTags(const FGameplayTagContainer& Tags) { BlockedTags.UpdateTagCount(Tags, -1); };

	/// @brief	UnBlocks action of specific types
	UFUNCTION(Category=Actions, BlueprintCallable)
	FORCEINLINE void UnBlockActionByClass(const UGameplayActionData* ActionToUnBlock) { BlockedActions.Remove(ActionToUnBlock); }

protected:
	/// @brief  Internal Use. Will setup an actions Block & Cancel tags. Adding block tags to an internal container to prevent activation
	///			and cancel running actions with the incoming ones CancelTags.
	void ApplyActionBlockAndCancelTags(const UGameplayActionData* InAction, bool bEnabledBlockTags, bool bExecuteCancelTags);

	/*--------------------------------------------------------------------------------------------------------------
	* Ability Event Response: 
	*--------------------------------------------------------------------------------------------------------------*/
	
	/// @brief  Called from the action to let us know it successfully activated
	virtual void NotifyActionActivated(UGameplayAction* Action);
	/// @brief  Called from the action to let us know it ended
	virtual void NotifyActionEnded(UGameplayAction* Action);

	/*--------------------------------------------------------------------------------------------------------------
	* Movement Component Bindings: Propagate these to the active actions 
	*--------------------------------------------------------------------------------------------------------------*/
public:

	virtual void CalculateVelocity(URadicalMovementComponent* MovementComponent, float DeltaTime);
	virtual void UpdateRotation(URadicalMovementComponent* MovementComponent, float DeltaTime);

#pragma endregion Gameplay Actions

#pragma region Gameplay Tag Interface
	
protected:
	//UPROPERTY(Category="Action Tags", EditDefaultsOnly, BlueprintReadWrite)
	FGameplayTagCountContainer GrantedTags;
	FGameplayTagCountContainer BlockedTags;

public:
	/*--------------------------------------------------------------------------------------------------------------
	* IGameplayTagInterface
	*--------------------------------------------------------------------------------------------------------------*/

	// NOTE: Can add optional delegate to bind to in case we wanna listen to any added tags from anywhere
	UFUNCTION(Category = GameplayTags, BlueprintCallable)
	void AddTag(const FGameplayTag& TagToAdd)
	{
		OnGameplayTagAddedDelegate.Broadcast(FGameplayTagContainer(TagToAdd));
		GrantedTags.UpdateTagCount(TagToAdd,1);
	}
	UFUNCTION(Category = GameplayTags, BlueprintCallable)
	void AddTags(const FGameplayTagContainer& TagContainer)
	{
		OnGameplayTagAddedDelegate.Broadcast(TagContainer);
		GrantedTags.UpdateTagCount(TagContainer, 1);
	}
	UFUNCTION(Category = GameplayTags, BlueprintCallable)
	bool RemoveTag(const FGameplayTag& TagToRemove)
	{
		OnGameplayTagRemovedDelegate.Broadcast(FGameplayTagContainer(TagToRemove));
		return GrantedTags.UpdateTagCount(TagToRemove, -1);
	}
	UFUNCTION(Category = GameplayTags, BlueprintCallable)
	void RemoveTags(const FGameplayTagContainer& TagContainer)
	{
		OnGameplayTagRemovedDelegate.Broadcast(TagContainer);
		GrantedTags.UpdateTagCount(TagContainer, -1);
	}

	UFUNCTION(Category = GameplayTags, BlueprintCallable)
	void AddCountOfTag(const FGameplayTag& TagToAdd, const int Count)
	{
		GrantedTags.UpdateTagCount(TagToAdd, Count);
	}

	UFUNCTION(Category = GameplayTags, BlueprintCallable)
	void RemoveCountOfTag(const FGameplayTag& TagToRemove, const int Count)
	{
		GrantedTags.UpdateTagCount(TagToRemove, -Count);
	}
	
	// IGameplayTagAssetInterface
	FORCEINLINE virtual void GetOwnedGameplayTags(FGameplayTagContainer& TagContainer) const override { TagContainer.AppendTags(GrantedTags.GetExplicitGameplayTags()); }
	FORCEINLINE virtual bool HasMatchingGameplayTag(FGameplayTag TagToCheck) const override { return GrantedTags.HasMatchingGameplayTag(TagToCheck); }
	FORCEINLINE virtual bool HasAllMatchingGameplayTags(const FGameplayTagContainer& TagContainer) const override { return GrantedTags.HasAllMatchingGameplayTags(TagContainer); }
	FORCEINLINE virtual bool HasAnyMatchingGameplayTags(const FGameplayTagContainer& TagContainer) const override { return GrantedTags.HasAnyMatchingGameplayTags(TagContainer); }
	// ~ IGameplayTagAssetInterface

	// For Tag Count Queries
	FORCEINLINE bool HasMatchingGameplayTagCount(FGameplayTag TagToCheck, int CountToCheck) const
	{
		return GrantedTags.GetTagCount(TagToCheck) >= CountToCheck;
	}

	/// @brief	Allow events to be registered for specific gameplay tags being added or removed 
	FOnGameplayTagCountChanged& RegisterGameplayTagEvent(FGameplayTag Tag, EGameplayTagEventType::Type EventType=EGameplayTagEventType::NewOrRemoved)
	{
		return GrantedTags.RegisterGameplayTagEvent(Tag, EventType);
	}

	/// @brief	Unregister previously added events 
	void UnregisterGameplayTagEvent(FDelegateHandle DelegateHandle, FGameplayTag Tag, EGameplayTagEventType::Type EventType=EGameplayTagEventType::NewOrRemoved)
	{
		GrantedTags.RegisterGameplayTagEvent(Tag, EventType).Remove(DelegateHandle);
	}


#pragma endregion Gameplay Tag Interface
};
