// Copyright 2023 CoC All rights reserved

#pragma once

#include "CoreMinimal.h"
#include "AttributeModifierTypes.h"
#include "EntityAttributeSet.h"
#include "EntityEffectTypes.generated.h"

class UEntityEffect;

/*--------------------------------------------------------------------------------------------------------------
* Context
*--------------------------------------------------------------------------------------------------------------*/


USTRUCT(BlueprintType)
struct ACTIONFRAMEWORK_API FEntityEffectContext
{
	GENERATED_BODY()

	FEntityEffectContext() {}

	FEntityEffectContext(AActor* InInstigator, AActor* InEffectCauser)
	{
		AddInstigator(InInstigator, InEffectCauser);
	}

	void AddInstigator(AActor* InInstigator, AActor* InEffectCauser);

	UAttributeSystemComponent* GetInstigatorAttributeSystemComponent() const;

	UAttributeSystemComponent* GetTargetAttributeSystemComponent() const;
	
	AActor* GetInstigatorActor() const;

	AActor* GetEffectCauser() const;

	void SetEffectCauser(AActor* InEffectCauser);

	void SetTarget(UAttributeSystemComponent* InTargetASC);
	
	FString ToString() const;

	bool IsValid() const
	{
		return Instigator.IsValid() && EffectCauser.IsValid();
	}

protected:
	
	TWeakObjectPtr<AActor> Instigator;

	TWeakObjectPtr<AActor> EffectCauser;
	
	TWeakObjectPtr<UAttributeSystemComponent> InstigatorAttributeSystemComponent;
	
	/// @brief	AttributeSystemComponent of the target the effect is being executed/applied to. This is filled out during effect application/execution.  
	TWeakObjectPtr<UAttributeSystemComponent> TargetAttributeSystemComponent;
	
	TArray<TWeakObjectPtr<AActor>> ActorReferences;
};

/*--------------------------------------------------------------------------------------------------------------
* Misc Types
*--------------------------------------------------------------------------------------------------------------*/

struct ACTIONFRAMEWORK_API FEntityEffectConstants
{
	static const float INFINITE_DURATION;

	static const float INSTANT_APPLICATION;

	static const float NO_PERIOD;

	static const float UPDATE_FREQUENCY;
};

/*--------------------------------------------------------------------------------------------------------------
* Events
*--------------------------------------------------------------------------------------------------------------*/

/// @brief	Temporary parameter struct used when an attribute has changed 
USTRUCT(BlueprintType)
struct ACTIONFRAMEWORK_API FOnEntityAttributeChangeData
{
	GENERATED_BODY()
	
	FOnEntityAttributeChangeData()
		: NewValue(0.0f)
		, OldValue(0.0f)
		, GEModData(nullptr)
	{ }

	UPROPERTY(BlueprintReadOnly)
	FEntityAttribute Attribute;

	UPROPERTY(BlueprintReadOnly)
	float	NewValue;
	UPROPERTY(BlueprintReadOnly)
	float	OldValue;
	const FEntityEffectModCallbackData* GEModData;
};

/** Data struct for containing information pertinent to EntityEffects as they are removed */
USTRUCT(BlueprintType)
struct ACTIONFRAMEWORK_API FEntityEffectRemovalInfo
{
	GENERATED_BODY()
	
	/** True when the gameplay effect's duration has not expired, meaning the gameplay effect is being forcefully removed.  */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Removal")
	bool bPrematureRemoval = false;
	
	/** Actor this gameplay effect was targeting. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Removal")
	FEntityEffectContext EffectContext;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Removal")
	TObjectPtr<const UEntityEffect> EffectRemoved;
};

DECLARE_MULTICAST_DELEGATE_OneParam(FOnActiveEntityEffectRemoved_Info, const FEntityEffectRemovalInfo&);

/** Callback struct for different types of entity effect changes */
USTRUCT(BlueprintType)
struct ACTIONFRAMEWORK_API FActiveEntityEffectEvents
{
	GENERATED_BODY()

	FOnActiveEntityEffectRemoved_Info OnEffectRemoved;
};

/*--------------------------------------------------------------------------------------------------------------
* Spec
*--------------------------------------------------------------------------------------------------------------*/


USTRUCT(BlueprintType)
struct ACTIONFRAMEWORK_API FEntityEffectSpec
{
	GENERATED_BODY()

	FEntityEffectSpec() {}

	FEntityEffectSpec(const FEntityEffectSpec& Other);

	FEntityEffectSpec(const UEntityEffect* InDef, const FEntityEffectContext& InEffectContext)
	{
		Initialize(InDef, InEffectContext);
	}
	
	/// @brief	Sets up the Spec & Captures SourceAttributes for snapshot captures. As well as generate a list of mods that are non-snapshot
	void Initialize(const UEntityEffect* InDef, const FEntityEffectContext& InEffectContext);

	void InitializeFromSpec(const UEntityEffect* InDef, const FEntityEffectSpec& OriginalSpec);
	
	void CaptureAttributes(bool bInitialCapture);

	void RecaptureSourceSnapshots();
	
	bool AttemptCalculateDurationFromDef(OUT float& OutDefDuration) const;

	void SetDuration(float NewDuration);

	float GetDuration() const;
	float GetPeriod() const; 

	void SetContext(FEntityEffectContext NewEffectContext);

	void SetContextEffectCauser(AActor* InEffectCauser) { EffectContext.SetEffectCauser(InEffectCauser); }
	
	FEntityEffectContext GetContext() const { return EffectContext; }

	void InitializeTargetAndCaptures(const UAttributeSystemComponent* InTargetASC);

	void GetEffectTags(OUT FGameplayTagContainer& OutContainer) const;
	
	void CalculateModifierMagnitudes();

	void CalculateModifierMagnitude(int32 ModIdx);
	
	float GetModifierMagnitude(int32 ModIdx) const;

	void PrintAll() const;

	bool HasNonSnapshotMods() const { return NonSnapshotModsIdx.Num() > 0; }
	
public:

	UPROPERTY()
	TObjectPtr<const UEntityEffect> Def;
	
	UPROPERTY()
	float Duration;

	UPROPERTY()
	float Period;
	
	/// @brief  Tracker for evaluated magnitudes of our modifiers
	UPROPERTY()
	TArray<FAttributeModifierInfo> Modifiers;
	
protected:

	UPROPERTY(BlueprintReadOnly)
	FEntityEffectContext EffectContext;

	bool bHasNonSnapshotAttributes;

	TArray<int32> NonSnapshotModsIdx;

	TBitArray<> InvalidModCapturesIdx;
};

USTRUCT(BlueprintType)
struct ACTIONFRAMEWORK_API FActiveEntityEffectHandle
{
	GENERATED_BODY()

	FActiveEntityEffectHandle()
		: Handle(INDEX_NONE)
	{}

	FActiveEntityEffectHandle(int32 InHandle)
		: Handle(InHandle)
	{}

	/** True if this is tracking an active ongoing gameplay effect */
	bool IsValid() const
	{
		return Handle != INDEX_NONE;
	}

	void Invalidate()
	{
		Handle = INDEX_NONE;
	}

	FString ToString() const
	{
		return FString::Printf(TEXT("%d"), Handle);
	}

	bool operator==(const FActiveEntityEffectHandle& Other) const
	{
		return Handle == Other.Handle;
	}

	bool operator!=(const FActiveEntityEffectHandle& Other) const
	{
		return Handle != Other.Handle;
	}
	
	static FActiveEntityEffectHandle GenerateHandle()
	{
		static int32 GlobalHandle = 0;
		return FActiveEntityEffectHandle(GlobalHandle++);
	}


private:

	UPROPERTY()
	int32 Handle;
};

USTRUCT(BlueprintType)
struct ACTIONFRAMEWORK_API FActiveEntityEffect
{
	GENERATED_BODY()

	FActiveEntityEffect() {};

	FActiveEntityEffect(FActiveEntityEffectHandle InHandle, const FEntityEffectSpec& InSpec, float CurrentWorldTime)
		: Handle(InHandle), Spec(InSpec), StartWorldTime(CurrentWorldTime) {}

	float GetTimeRemaining(float WorldTime) const
	{
		const float Duration = GetDuration();
		return (Duration == FEntityEffectConstants::INFINITE_DURATION ? -1.f : Duration - (WorldTime - StartWorldTime));
	}

	float GetDuration() const
	{
		return Spec.GetDuration();
	}

	float GetPeriod() const
	{
		return Spec.GetPeriod();
	}

	float GetEndTime() const
	{
		const float Duration = GetDuration();
		return (Duration == FEntityEffectConstants::INFINITE_DURATION ? -1.f : Duration + StartWorldTime);
	}

	void PrintAll() const;
	FString GetDebugString() const;

	bool operator==(const FActiveEntityEffect& Other) const
	{
		return Handle == Other.Handle;
	}

	UPROPERTY(BlueprintReadOnly)
	FActiveEntityEffectHandle Handle;
	
	UPROPERTY(BlueprintReadOnly)
	FEntityEffectSpec Spec;

	float StartWorldTime;
	
	FTimerHandle PeriodHandle;
	FTimerHandle DurationHandle;

	UPROPERTY(BlueprintReadOnly)
	FActiveEntityEffectEvents EventSet;
};

/*--------------------------------------------------------------------------------------------------------------
* Query
*--------------------------------------------------------------------------------------------------------------*/

/// @brief	Every set condition within this query must match in order for it to match with an effect
USTRUCT(BlueprintType)
struct ACTIONFRAMEWORK_API FEntityEffectQuery
{
	GENERATED_BODY()
	
	/// @brief	Query that is matched against tags this EE has */
	UPROPERTY(Category=Query, BlueprintReadWrite, EditAnywhere)
	FGameplayTagQuery EffectTagQuery;
	
	/// @brief	Matches on EntityEffects which modify given attribute. 
	UPROPERTY(Category=Query, BlueprintReadWrite, EditAnywhere)
	FEntityAttribute ModifyingAttribute;

	/// @brief	Matches on EntityEffects which come from this source 
	UPROPERTY(Category=Query, BlueprintReadWrite, EditAnywhere)
	TObjectPtr<const UObject> EffectSource;
	
	/// @brief  Matches on EntityEffects which were instigated by this actor
	UPROPERTY(Category=Query, BlueprintReadWrite, EditAnywhere)
	TObjectPtr<AActor> Instigator;
	
	/// @brief	Matches on EntityEffects with this definition 
	UPROPERTY(Category=Query, BlueprintReadWrite, EditAnywhere)
	TSubclassOf<UEntityEffect> EffectDefinition;

	/// @brief	Handles to ignore as matches, even if other criteria is met
	TArray<FActiveEntityEffectHandle> IgnoreHandles;

	
	bool Matches(const FActiveEntityEffect& Effect) const;

	bool Matches(const FEntityEffectSpec& Effect) const;

	bool IsEmpty() const;

#pragma region Query Auto Creation Helpers

	// Shortcuts for easily creating common query types 
	
	/** Creates an effect query that will match if there are any common tags between the given tags and an ActiveEntityEffect's tags */
	static FEntityEffectQuery MakeQuery_MatchAnyEffectTags(const FGameplayTagContainer& InTags)
	{
		FEntityEffectQuery OutQuery;
		OutQuery.EffectTagQuery = FGameplayTagQuery::MakeQuery_MatchAnyTags(InTags);
		return OutQuery;
	}
	/** Creates an effect query that will match if all of the given tags are in the ActiveEntityEffect's tags */
	static FEntityEffectQuery MakeQuery_MatchAllEffectTags(const FGameplayTagContainer& InTags)
	{
		FEntityEffectQuery OutQuery;
		OutQuery.EffectTagQuery = FGameplayTagQuery::MakeQuery_MatchAllTags(InTags);
		return OutQuery;
	}
	/** Creates an effect query that will match if there are no common tags between the given tags and an ActiveEntityEffect's tags */
	static FEntityEffectQuery MakeQuery_MatchNoEffectTags(const FGameplayTagContainer& InTags)
	{
		FEntityEffectQuery OutQuery;
		OutQuery.EffectTagQuery = FGameplayTagQuery::MakeQuery_MatchNoTags(InTags);
		return OutQuery;
	}

#pragma endregion
};