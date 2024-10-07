// Copyright 2023 CoC All rights reserved

#pragma once

#include "CoreMinimal.h"
#include "AttributeSystem/EntityEffect.h"
#include "AttributeSystem/EntityEffectTypes.h"
#include "ActionSystem/GameplayActionTypes.h"
#include "Components/ActorComponent.h"
#include "Data/GameplayTagData.h"
#include "GameplayTagAssetInterface.h"
#include "AttributeSystemComponent.generated.h"


DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FImmunityBlockEE, const FEntityEffectSpec&, BlockedSpec);
                                             //const FActiveEntityEffect&, ImmunityAttributeEffect); TODO: Pass active effect handle to events
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEntityEffectAppliedDelegate, UAttributeSystemComponent*, Src, const FEntityEffectSpec&, ActiveEffect);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGivenActiveEntityEffectRemoved, const FActiveEntityEffect&, EffectRemoved);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEntityAttributeValueChange, const FOnEntityAttributeChangeData&, ChangeInfo);

// Will call ImmunityBlock if any fail
DECLARE_DELEGATE_RetVal_TwoParams(bool, FEntityEffectApplicationQuery, const UAttributeSystemComponent* /*AttributeSystemComponent*/, const FEntityEffectSpec& /*GESpecToConsider*/)//, const FActiveEntityEffectHandle /* Active Effect Handle On Effect Binding To This*/);

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent), hidecategories=(Object,LOD,Lighting,Transform,Sockets,TextureStreaming))
class ACTIONFRAMEWORK_API UAttributeSystemComponent : public UActorComponent, public IGameplayTagAssetInterface
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UAttributeSystemComponent();

	// BEGIN UActorComponent Interface
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	// ~ END
	
	
	/*--------------------------------------------------------------------------------------------------------------
	* Attributes
	*--------------------------------------------------------------------------------------------------------------*/
protected:
	
	/// @brief  Initial stats for this attribute system. Will auto create the necessary attribute sets and populate the initial values accordingly
	UPROPERTY(Category="Entity Attributes", EditDefaultsOnly)
	UAttributeInitDataTable* InitStats;

public:

	/// API TODO:
	/// - Cleanup Events [BaseValue Change, CurrentValue Change, Applied/Executed/Removed/Failed To Apply]
	/// - Functions We Want:
	///  - Getting info about End Time & Duration (By ActiveHandle, or Query)
	///  - [No need?] Getters for active effects (via a query or Tags)
	///  - [KINDA DONE, STILL UNSURE] More robust FGameplayTag interface (and same in ActionSystem to pipe our calls their if owner has it)
	
	/// @brief	Getter for an attribute set of a given class, C++ Helper
	template<class T>
	const T* GetSet() const
	{
		return (T*)GetAttributeSet(T::StaticClass());
	}
	
	/// @brief  Checks whether this components attribute set contains a given attribute
	UFUNCTION(Category="Entity Attributes", BlueprintPure)
	bool HasAttributeSetForAttribute(FEntityAttribute Attribute) const;
	
	/// @brief  Returns all attributes that this attribute system has from its attribute sets
	UFUNCTION(Category="Entity Attributes", BlueprintPure)
	void GetAllAttributes(TArray<FEntityAttribute>& OutAttributes) const;
	
	/// @brief  Attempts to get a given attribute set on this component. Will return null if not found.
	UFUNCTION(Category = "Entity Attributes", BlueprintPure)
	const UEntityAttributeSet* GetAttributeSet(TSubclassOf<UEntityAttributeSet> AttributeSetClass) const;

	/// @brief  Gets the current value of a given attribute on this component if it's found, 0 otherwise.
	UFUNCTION(Category = "Entity Attributes", BlueprintPure, DisplayName="GetAttributeValue")
	float BP_GetAttributeValue(FEntityAttribute Attribute, bool& bFound) const { return GetAttributeValue(Attribute, bFound); }
	
	/// @brief  Gets the current value of a given attribute on this component if it's found, 0 otherwise.
	float GetAttributeValue(const FEntityAttribute& Attribute, bool& bFound) const;

	UFUNCTION(Category = "Entity Attributes", BlueprintPure, DisplayName="GetAttributeBaseValue")
	float BP_GetAttributeBaseValue(FEntityAttribute Attribute) const { return GetAttributeBaseValue(Attribute); }

	/// @brief  Gets the BaseValue of a given attribute on this component if it's found, 0 otherwise
	float GetAttributeBaseValue(const FEntityAttribute& Attribute) const;

	float GetAttributeValue(const FEntityAttribute& Attribute) const { bool bDummy; return GetAttributeValue(Attribute, bDummy); }
	
	/// @brief  Attempts to get an attribute set on this component, or create it if its not found
	UFUNCTION(Category="Entity Attributes", BlueprintCallable)
	const UEntityAttributeSet* GetOrCreateAttributeSet(TSubclassOf<UEntityAttributeSet> AttributeClass)
	{
		AActor* OwningActor = GetOwner();
		const UEntityAttributeSet* MyAttributes = nullptr;
		if (OwningActor && AttributeClass)
		{
			MyAttributes = GetAttributeSet(AttributeClass);
			if (!MyAttributes)
			{
				UEntityAttributeSet* Attributes = NewObject<UEntityAttributeSet>(OwningActor, AttributeClass);
				AddSpawnedAttributeSet(Attributes);
				MyAttributes = Attributes;
			}
		}

		return MyAttributes;
	}

	void SetAttributeBaseValue(const FEntityAttribute& Attribute, float NewBaseValue);

protected:
	
	/// @brief	Internal function to add an attribute set after creating it
	void AddSpawnedAttributeSet(UEntityAttributeSet* AttributeSet)
	{
		if (IsValid(AttributeSet) && SpawnedAttributes.Find(AttributeSet) == INDEX_NONE)
			SpawnedAttributes.Add(AttributeSet);
	}

	/// @brief	Internal function to add an remove set
	void RemoveSpawnedAttributeSet(UEntityAttributeSet* AttributeSet) { SpawnedAttributes.RemoveSingle(AttributeSet); }
	
	/// @brief  Internal function to get all attributes
	const TArray<UEntityAttributeSet*>& GetSpawnedAttributeSets() const { return SpawnedAttributes; }
	
	/*--------------------------------------------------------------------------------------------------------------
	* Entity Effects
	*--------------------------------------------------------------------------------------------------------------*/

public:
	
	/// @brief  Applies a given entity effect to a target based on the class of the effect
	UFUNCTION(Category="Entity Effects", BlueprintCallable)
	FActiveEntityEffectHandle ApplyEntityEffectToTarget(TSubclassOf<UEntityEffect> EntityEffect, UAttributeSystemComponent* Target, FEntityEffectContext Context);

	/// @brief  Applies a given entity effect to self based on the class of the effect
	UFUNCTION(Category="Entity Effects", BlueprintCallable)
	FActiveEntityEffectHandle ApplyEntityEffectToSelf(TSubclassOf<UEntityEffect> EntityEffect, FEntityEffectContext Context);

	/// @brief  Applies a given entity effect to a target based on a previously created effect spec
	UFUNCTION(Category="Entity Effects", BlueprintCallable)
	FActiveEntityEffectHandle ApplyEntityEffectSpecToTarget(const FEntityEffectSpec& EntityEffect, UAttributeSystemComponent* Target);

	/// @brief  Applies a given entity effect to self based on a previously created effect spec
	UFUNCTION(Category="Entity Effects", BlueprintCallable)
	FActiveEntityEffectHandle ApplyEntityEffectSpecToSelf(const FEntityEffectSpec& EntityEffect);

	/// @brief  Removes the active effect that has this handle. Returns true if the effect was found and removed.
	UFUNCTION(Category="Entity Effects", BlueprintCallable)
	bool RemoveActiveEntityEffect(FActiveEntityEffectHandle EffectHandle); 

	/// @brief  Removes all active effects of this type, returns number of effects removed
	UFUNCTION(Category="Entity Effects", BlueprintCallable)
	int32 RemoveActiveEntityEffectsBySourceEffect(TSubclassOf<UEntityEffect> EntityEffect); 

	/// @brief  Removes all active effects instigated by this actor, returns number of effects removed
	UFUNCTION(Category="Entity Effects", BlueprintCallable)
	int32 RemoveActiveEntityEffectsByInstigator(AActor* Instigator); 

	/// @brief  Removes all active effects which have any of the tag definitions, returns number of effects removed
	UFUNCTION(Category="Entity Effects", BlueprintCallable)
	int32 RemoveActiveEntityEffectsByEffectTags(const FGameplayTagContainer& EffectTags);
	
	/// @brief  Removes all active effects that match with the query, returns number of effects removed
	UFUNCTION(Category="Entity Effects", BlueprintCallable)
	int32 RemoveActiveEntityEffects(const FEntityEffectQuery& Query);
	
	/// @brief  Helper function to be able to execute a modifier on an attribute on the fly without an effect (rather than setting it directly)
	UFUNCTION(Category="Entity Effects", BlueprintCallable, DisplayName="ExecuteModOnAttribute")
	void BP_ExecModOnAttribute(FEntityAttribute Attribute, EAttributeModifierOp ModifierOp, float ModifierMagnitude)
	{
		ExecModOnAttribute(Attribute, ModifierOp, ModifierMagnitude, nullptr);
	}

	void ExecModOnAttribute(const FEntityAttribute& Attribute, EAttributeModifierOp ModifierOp, float ModifierMagnitude, const FEntityEffectModCallbackData* ModData = nullptr);

	/// @brief  Checks if a given effect can be applied. Primarily sees if, after modifiers are applied, the attributes are non-negative
	UFUNCTION(Category="Entity Effects", BlueprintPure)
	bool CanApplyAttributeModifiers(TSubclassOf<UEntityEffect> EntityEffect, const FEntityEffectContext& EffectContext) const; 

	/// @brief  Checks if a given effect can be applied. Primarily sees if, after modifiers are applied, the attributes are non-negative
	bool CanApplyAttributeModifiers(const UEntityEffect* EntityEffect, const FEntityEffectContext& EffectContext) const; 
	
	/// @brief  Creates a spec with the given effect definition and context. Spec can be modified after creation before its applied.
	UFUNCTION(Category="Entity Effects", BlueprintCallable)
	FEntityEffectSpec MakeOutgoingSpec(TSubclassOf<UEntityEffect> EntityEffect, FEntityEffectContext Context) const;
	
	/// @brief	Makes an effect context, will auto set this attribute system to be the instigator  
	UFUNCTION(Category="Entity Effects", BlueprintCallable)
	FEntityEffectContext MakeEffectContext() const;

protected:

	void ExecuteEntityEffect(FEntityEffectSpec& EntityEffect); 

	void ExecutePeriodicEffect(FActiveEntityEffectHandle EffectHandle); 
	
	FActiveEntityEffect* InternalApplyEntityEffectSpec(const FEntityEffectSpec& Spec);

	bool InternalExecuteMod(FEntityEffectSpec& Spec, FAttributeModifierEvaluatedData& ModEvalData);

	bool InternalRemoveActiveEntityEffect(FActiveEntityEffectHandle EffectHandle, bool bPrematureRemoval);

	void InternalUpdateAttributeValue(FEntityAttribute Attribute, float NewValue);
	
	/// @brief  For duration/infinite effects. Will add the effects modifiers to the appropriate aggregators
	///			and recalculate the aggregator, updating CurrentValue of each attribute its modifying
	void AddActiveEffectModifiers(FActiveEntityEffect& ActiveEE);

	/// @brief  For duration/infinite effects. Will remove the effects modifiers to the appropriate aggregators
	///			and recalculate the aggregator, updating CurrentValue of each attribute its modifying
	void RemoveActiveEffectModifiers(FActiveEntityEffect& ActiveEE);

	/// @brief	Callback bound to the duration of the effect when its initially applied. Checks if the effect should be removed,
	///			and if periodic, will execute the effect one last time if we're close to the period duration. It then removes the effect.
	void CheckDurationExpired(FActiveEntityEffectHandle EffectHandle); 
	
	FAttributeModifierAggregator& FindOrCreateAttributeAggregator(const FEntityAttribute& Attribute);
	FAttributeModifierAggregator* FindAttributeAggregator(const FEntityAttribute& Attribute) const;
	
	/*--------------------------------------------------------------------------------------------------------------
	* Additional Effect Helpers
	*--------------------------------------------------------------------------------------------------------------*/

public:
	
	/// @brief  Returns the number of entity effects that are currently active and running on this component
	UFUNCTION(Category="Entity Effects", BlueprintPure)
	int32 GetNumActiveEntityEffects() const { return ActiveEntityEffects.Num(); }

	/// @brief	Tries to find the active effect associated with the incoming handle. Returns null if not found.
	FActiveEntityEffect* GetActiveEntityEffect(FActiveEntityEffectHandle Handle) 
	{
		for (FActiveEntityEffect& Effect : ActiveEntityEffects)
		{
			if (Effect.Handle == Handle) return &Effect;
		}
		return nullptr;
	}
	
	/// @brief	Returns handles to all effects that match with the given query.
	TArray<FActiveEntityEffectHandle> GetActiveEntityEffects(const FEntityEffectQuery& Query)
	{
		TArray<FActiveEntityEffectHandle> ReturnList;

		for (const FActiveEntityEffect& Effect : ActiveEntityEffects)
		{
			if (!Query.Matches(Effect)) continue;
			ReturnList.Add(Effect.Handle);
		}
		return ReturnList;
	}
	
	/*--------------------------------------------------------------------------------------------------------------
	* Callbacks / Notifies
	*--------------------------------------------------------------------------------------------------------------*/

public:
	
	FOnEntityAttributeValueChange& GetEntityAttributeValueChangeDelegate(const FEntityAttribute& Attribute)
	{
		return AttributeValueChangeDelegates.FindOrAdd(Attribute);
	}
	
	/// @brief  Custom queries that you can bind to in EffectBehaviors to evaluate whether to allow a given effect from applying (NOTE: Public so Behaviors can access this)
	TArray<FEntityEffectApplicationQuery> EntityEffectApplicationQueries;
	
	/// @brief  Invoked by immunity behaviors whenever they block an effect from applying
	UPROPERTY(Category="Entity Effects", BlueprintAssignable)
	FImmunityBlockEE OnImmunityBlockEntityEffectDelegate;

protected:
	
	/// @brief  Called whenever an effect (any effect) is applied to a target by us.
	UPROPERTY(Category="Entity Effects", BlueprintAssignable)
	FOnEntityEffectAppliedDelegate OnEntityEffectAppliedToTargetDelegate;

	/// @brief  Called whenever an effect (any effect) has been applied to us (by us or by a target).
	UPROPERTY(Category="Entity Effects", BlueprintAssignable)
	FOnEntityEffectAppliedDelegate OnEntityEffectAppliedToSelfDelegate;

	// TODO: Custom ones for just execute and split it from Applied?

	/// @brief  Called whenever a periodic effect executes on us.
	UPROPERTY(Category="Entity Effects", BlueprintAssignable)
	FOnEntityEffectAppliedDelegate OnPeriodicEntityEffectExecuteOnSelfDelegate;

	/// @brief  Called whenever a periodic effect we've applied to a target executes on them.
	UPROPERTY(Category="Entity Effects", BlueprintAssignable)
	FOnEntityEffectAppliedDelegate OnPeriodicEntityEffectExecuteOnTargetDelegate;
	
	/// @brief  Called whenever a non-instant effect that was applied on us has been removed
	UPROPERTY(Category="Entity Effects", BlueprintAssignable)
	FOnGivenActiveEntityEffectRemoved OnActiveEntityEffectRemovedDelegate;

	/// @brief  Called whenever a non-instant effect that was applied on us has been removed
	UPROPERTY(Category="Entity Effects", BlueprintAssignable)
	FOnEntityAttributeValueChange OnAttributesValueChanged;

	virtual void OnEntityEffectAppliedToSelf(UAttributeSystemComponent* Source, const FEntityEffectSpec& SpecApplied)
	{
		OnEntityEffectAppliedToSelfDelegate.Broadcast(Source, SpecApplied);
	}

	virtual void OnEntityEffectAppliedToTarget(UAttributeSystemComponent* Target, const FEntityEffectSpec& SpecApplied)
	{
		OnEntityEffectAppliedToTargetDelegate.Broadcast(Target, SpecApplied);
	}

	virtual void OnPeriodicEntityEffectExecuteOnSelf(UAttributeSystemComponent* Source, const FEntityEffectSpec& SpecExecuted)
	{
		OnPeriodicEntityEffectExecuteOnSelfDelegate.Broadcast(Source, SpecExecuted);
	}

	virtual void OnPeriodicEntityEffectExecuteOnTarget(UAttributeSystemComponent* Target, const FEntityEffectSpec& SpecExecuted)
	{
		OnPeriodicEntityEffectExecuteOnTargetDelegate.Broadcast(Target, SpecExecuted);
	}
	
	
	/*--------------------------------------------------------------------------------------------------------------
	* Owner Info
	*--------------------------------------------------------------------------------------------------------------*/
public:

	void InitActorInfo(AActor* InOwnerActor);
	FActionActorInfo GetActorInfo() const { return CurrentActorInfo; }
	
	void SetActorOwner(AActor* NewOwnerActor) { OwnerActor = NewOwnerActor; InitActorInfo(OwnerActor); }
	AActor* GetOwnerActor() const { return OwnerActor; }

private:

	friend class UEntityAttributeSet;
	
	UPROPERTY()
	TObjectPtr<AActor> OwnerActor;

	/// @brief  Shared cached info about the thing using us. Per UE, hopefully allocated
	///			once per actor and shared by many abilities owned by said actor.
	FActionActorInfo CurrentActorInfo; // NOTE: ActionSystemComponent is responsible for setting this up for us? Or do a manual setup again here
	
	
	/*--------------------------------------------------------------------------------------------------------------
	* Gameplay Tags Interface
	*--------------------------------------------------------------------------------------------------------------*/
	// NOTE: We'll hold a tag container here, but we'll also check if our owner has an ActionSystemComp. If they do, we pipe our calls to it, otherwise deal with it internally
public:
	
	// IGameplayTagAssetInterface
	virtual void GetOwnedGameplayTags(FGameplayTagContainer& TagContainer) const override;
	virtual bool HasMatchingGameplayTag(FGameplayTag TagToCheck) const override;
	virtual bool HasAllMatchingGameplayTags(const FGameplayTagContainer& TagContainer) const override;
	virtual bool HasAnyMatchingGameplayTags(const FGameplayTagContainer& TagContainer) const override;
	// ~ IGameplayTagAssetInterface


	UFUNCTION(Category = GameplayTags, BlueprintCallable)
	void AddTag(const FGameplayTag& TagToAdd);
	
	UFUNCTION(Category = GameplayTags, BlueprintCallable)
	void AddTags(const FGameplayTagContainer& TagContainer);
	
	UFUNCTION(Category = GameplayTags, BlueprintCallable)
	bool RemoveTag(const FGameplayTag& TagToRemove);
	
	UFUNCTION(Category = GameplayTags, BlueprintCallable)
	void RemoveTags(const FGameplayTagContainer& TagContainer);

	UFUNCTION(Category = GameplayTags, BlueprintCallable)
	void AddCountOfTag(const FGameplayTag& TagToAdd, const int Count);

	UFUNCTION(Category = GameplayTags, BlueprintCallable)
	void RemoveCountOfTag(const FGameplayTag& TagToRemove, const int Count);

	/// @brief	Allow events to be registered for specific gameplay tags being added or removed 
	FOnGameplayTagCountChanged& RegisterGameplayTagEvent(FGameplayTag Tag, EGameplayTagEventType::Type EventType=EGameplayTagEventType::NewOrRemoved);

	/// @brief	Unregister previously added events 
	void UnregisterGameplayTagEvent(FDelegateHandle DelegateHandle, FGameplayTag Tag, EGameplayTagEventType::Type EventType=EGameplayTagEventType::NewOrRemoved);

protected:

	UPROPERTY(Transient)
	TArray<TObjectPtr<UEntityAttributeSet>> SpawnedAttributes;;
	
	/// @brief	Our active list of effects. Don't access directly, use getters even in internal functions
	UPROPERTY()
	TArray<FActiveEntityEffect> ActiveEntityEffects;
	
	/// @brief  Cached pointer to current mod data needed for callbacks. Just so we don't have to pass it through each function
	const FEntityEffectModCallbackData* CurrentModCallbackData;

	// Need the reference counting here
	TMap<FEntityAttribute, TSharedPtr<FAttributeModifierAggregator>> AttributeAggregatorMap;

	TMap<FEntityAttribute, FOnEntityAttributeValueChange> AttributeValueChangeDelegates;
	
	FGameplayTagCountContainer AttributeSystemTags;

	/*--------------------------------------------------------------------------------------------------------------
	* DEBUG 
	*--------------------------------------------------------------------------------------------------------------*/
protected:

	void VisualizeAttributes() const;
};

