// Copyright 2023 CoC All rights reserved

#pragma once
#include "GameplayTagContainer.h"
#include "CoreMinimal.h"
#include "GameplayTagData.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogActionSystemTags, Warning, All);

DECLARE_DELEGATE(FDeferredTagChangeDelegate);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnGameplayTagCountChanged, const FGameplayTag, int32);



UENUM(BlueprintType)
namespace EGameplayTagEventType
{
	/** Rather a tag was added or removed, used in callbacks */
	enum Type
	{		
		/** Event only happens when tag is new or completely removed */
		NewOrRemoved,

		/** Event happens any time tag "count" changes */
		AnyCountChange		
	};
}


/**
 * Struct that tracks the number/count of tag applications within it. Explicitly tracks the tags added or removed,
 * while simultaneously tracking the count of parent tags as well. Events/delegates are fired whenever the tag counts
 * of any tag (explicit or parent) are modified.
 */
struct ACTIONFRAMEWORK_API FGameplayTagCountContainer
{	
	FGameplayTagCountContainer()
	{}

	/**
	 * Check if the count container has a gameplay tag that matches against the specified tag (expands to include parents of asset tags)
	 * 
	 * @param TagToCheck	Tag to check for a match
	 * 
	 * @return True if the count container has a gameplay tag that matches, false if not
	 */
	FORCEINLINE bool HasMatchingGameplayTag(FGameplayTag TagToCheck) const
	{
		return GameplayTagCountMap.FindRef(TagToCheck) > 0;
	}

	/**
	 * Check if the count container has gameplay tags that matches against all of the specified tags (expands to include parents of asset tags)
	 * 
	 * @param TagContainer			Tag container to check for a match. If empty will return true
	 * 
	 * @return True if the count container matches all of the gameplay tags
	 */
	FORCEINLINE bool HasAllMatchingGameplayTags(const FGameplayTagContainer& TagContainer) const
	{
		// if the TagContainer count is 0 return bCountEmptyAsMatch;
		if (TagContainer.Num() == 0)
		{
			return true;
		}

		bool AllMatch = true;
		for (const FGameplayTag& Tag : TagContainer)
		{
			if (GameplayTagCountMap.FindRef(Tag) <= 0)
			{
				AllMatch = false;
				break;
			}
		}		
		return AllMatch;
	}
	
	/**
	 * Check if the count container has gameplay tags that matches against any of the specified tags (expands to include parents of asset tags)
	 * 
	 * @param TagContainer			Tag container to check for a match. If empty will return false
	 * 
	 * @return True if the count container matches any of the gameplay tags
	 */
	FORCEINLINE bool HasAnyMatchingGameplayTags(const FGameplayTagContainer& TagContainer) const
	{
		if (TagContainer.Num() == 0)
		{
			return false;
		}

		bool AnyMatch = false;
		for (const FGameplayTag& Tag : TagContainer)
		{
			if (GameplayTagCountMap.FindRef(Tag) > 0)
			{
				AnyMatch = true;
				break;
			}
		}
		return AnyMatch;
	}
	
	/**
	 * Update the specified container of tags by the specified delta, potentially causing an additional or removal from the explicit tag list
	 * 
	 * @param Container		Container of tags to update
	 * @param CountDelta	Delta of the tag count to apply
	 */
	FORCEINLINE void UpdateTagCount(const FGameplayTagContainer& Container, int32 CountDelta)
	{
		if (CountDelta != 0)
		{
			bool bUpdatedAny = false;
			TArray<FDeferredTagChangeDelegate> DeferredTagChangeDelegates;
			for (auto TagIt = Container.CreateConstIterator(); TagIt; ++TagIt)
			{
				bUpdatedAny |= UpdateTagMapDeferredParentRemoval_Internal(*TagIt, CountDelta, DeferredTagChangeDelegates);
			}

			if (bUpdatedAny && CountDelta < 0)
			{
				ExplicitTags.FillParentTags();
			}

			for (FDeferredTagChangeDelegate& Delegate : DeferredTagChangeDelegates)
			{
				Delegate.Execute();
			}
		}
	}
	
	/**
	 * Update the specified tag by the specified delta, potentially causing an additional or removal from the explicit tag list
	 * 
	 * @param Tag						Tag to update
	 * @param CountDelta				Delta of the tag count to apply
	 * 
	 * @return True if tag was *either* added or removed. (E.g., we had the tag and now dont. or didnt have the tag and now we do. We didn't just change the count (1 count -> 2 count would return false).
	 */
	FORCEINLINE bool UpdateTagCount(const FGameplayTag& Tag, int32 CountDelta)
	{
		if (CountDelta != 0)
		{
			return UpdateTagMap_Internal(Tag, CountDelta);
		}

		return false;
	}

	/**
	 * Update the specified tag by the specified delta, potentially causing an additional or removal from the explicit tag list.
	 * Calling code MUST call FillParentTags followed by executing the returned delegates.
	 * 
	 * @param Tag						Tag to update
	 * @param CountDelta				Delta of the tag count to apply
	 * @param DeferredTagChangeDelegates		Delegates to be called after this code runs
	 * 
	 * @return True if tag was *either* added or removed. (E.g., we had the tag and now dont. or didnt have the tag and now we do. We didn't just change the count (1 count -> 2 count would return false).
	 */
	FORCEINLINE bool UpdateTagCount_DeferredParentRemoval(const FGameplayTag& Tag, int32 CountDelta, TArray<FDeferredTagChangeDelegate>& DeferredTagChangeDelegates)
	{
		if (CountDelta != 0)
		{
			return UpdateTagMapDeferredParentRemoval_Internal(Tag, CountDelta, DeferredTagChangeDelegates);
		}

		return false;
	}

	/**
	 * Set the specified tag count to a specific value
	 * 
	 * @param Tag			Tag to update
	 * @param Count			New count of the tag
	 * 
	 * @return True if tag was *either* added or removed. (E.g., we had the tag and now dont. or didnt have the tag and now we do. We didn't just change the count (1 count -> 2 count would return false).
	 */
	FORCEINLINE bool SetTagCount(const FGameplayTag& Tag, int32 NewCount)
	{
		int32 ExistingCount = 0;
		if (int32* Ptr  = ExplicitTagCountMap.Find(Tag))
		{
			ExistingCount = *Ptr;
		}

		int32 CountDelta = NewCount - ExistingCount;
		if (CountDelta != 0)
		{
			return UpdateTagMap_Internal(Tag, CountDelta);
		}

		return false;
	}

	/**
	* return the count for a specified tag 
	*
	* @param Tag			Tag to update
	*
	* @return the count of the passed in tag
	*/
	FORCEINLINE int32 GetTagCount(const FGameplayTag& Tag) const
	{
		if (const int32* Ptr = GameplayTagCountMap.Find(Tag))
		{
			return *Ptr;
		}

		return 0;
	}

	/**
	 *	Broadcasts the AnyChange event for this tag. This is called when the stack count of the backing gameplay effect change.
	 *	It is up to the receiver of the broadcasted delegate to decide what to do with this.
	 */
	void Notify_StackCountChange(const FGameplayTag& Tag);

	/**
	 * Return delegate that can be bound to for when the specific tag's count changes to or off of zero
	 *
	 * @param Tag	Tag to get a delegate for
	 * 
	 * @return Delegate for when the specified tag's count changes to or off of zero
	 */
	FOnGameplayTagCountChanged& RegisterGameplayTagEvent(const FGameplayTag& Tag, EGameplayTagEventType::Type EventType=EGameplayTagEventType::NewOrRemoved);
	
	/**
	 * Return delegate that can be bound to for when the any tag's count changes to or off of zero
	 * 
	 * @return Delegate for when any tag's count changes to or off of zero
	 */
	FOnGameplayTagCountChanged& RegisterGenericGameplayEvent()
	{
		return OnAnyTagChangeDelegate;
	}

	/** Simple accessor to the explicit gameplay tag list */
	const FGameplayTagContainer& GetExplicitGameplayTags() const
	{
		return ExplicitTags;
	}

	void Reset();

	/** Fills in ParentTags from GameplayTags */
	void FillParentTags()
	{
		ExplicitTags.FillParentTags();
	}

private:

	struct FDelegateInfo
	{
		FOnGameplayTagCountChanged	OnNewOrRemove;
		FOnGameplayTagCountChanged	OnAnyChange;
	};

	/** Map of tag to delegate that will be fired when the count for the key tag changes to or away from zero */
	TMap<FGameplayTag, FDelegateInfo> GameplayTagEventMap;

	/** Map of tag to active count of that tag */
	TMap<FGameplayTag, int32> GameplayTagCountMap;

	/** Map of tag to explicit count of that tag. Cannot share with above map because it's not safe to merge explicit and generic counts */	
	TMap<FGameplayTag, int32> ExplicitTagCountMap;

	/** Delegate fired whenever any tag's count changes to or away from zero */
	FOnGameplayTagCountChanged OnAnyTagChangeDelegate;

	/** Container of tags that were explicitly added */
	FGameplayTagContainer ExplicitTags;

	/** Internal helper function to adjust the explicit tag list & corresponding maps/delegates/etc. as necessary */
	bool UpdateTagMap_Internal(const FGameplayTag& Tag, int32 CountDelta);

	/** Internal helper function to adjust the explicit tag list & corresponding maps/delegates/etc. as necessary. This does not call FillParentTags or any of the tag change delegates. These delegates are returned and must be executed by the caller. */
	bool UpdateTagMapDeferredParentRemoval_Internal(const FGameplayTag& Tag, int32 CountDelta, TArray<FDeferredTagChangeDelegate>& DeferredTagChangeDelegates);

	/** Internal helper function to adjust the explicit tag list & corresponding map. */
	bool UpdateExplicitTags(const FGameplayTag& Tag, int32 CountDelta, bool bDeferParentTagsOnRemove);

	/** Internal helper function to collect the delegates that need to be called when Tag has its count changed by CountDelta. */
	bool GatherTagChangeDelegates(const FGameplayTag& Tag, int32 CountDelta, TArray<FDeferredTagChangeDelegate>& TagChangeDelegates);
};

/** Structure that is used to combine tags from parent and child blueprints in a safe way */
USTRUCT(BlueprintType)
struct ACTIONFRAMEWORK_API FInheritedTagContainer
{
	GENERATED_USTRUCT_BODY()

	/** Tags that I inherited and tags that I added minus tags that I removed*/
	UPROPERTY(VisibleAnywhere, Transient, BlueprintReadOnly, Category = Application)
	FGameplayTagContainer CombinedTags;

	/** Tags that I have in addition to my parent's tags */
	UPROPERTY(EditDefaultsOnly, Transient, BlueprintReadOnly, Category = Application)
	FGameplayTagContainer Added;

	/** Tags that should be removed if my parent had them */
	UPROPERTY(EditDefaultsOnly, Transient, BlueprintReadOnly, Category = Application)
	FGameplayTagContainer Removed;

	void UpdateInheritedTagProperties(const FInheritedTagContainer* Parent);

	void PostInitProperties();

	void AddTag(const FGameplayTag& TagToAdd);
	void RemoveTag(FGameplayTag TagToRemove);
};

/** Encapsulate require and ignore tags */
USTRUCT(BlueprintType)
struct ACTIONFRAMEWORK_API FGameplayTagRequirements
{
	GENERATED_USTRUCT_BODY()

	/** All of these tags must be present */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = GameplayModifier)
	FGameplayTagContainer RequireTags;

	/** None of these tags may be present */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = GameplayModifier)
	FGameplayTagContainer IgnoreTags;

	/** True if all required tags and no ignore tags found */
	bool	RequirementsMet(const FGameplayTagContainer& Container) const;

	/** True if neither RequireTags or IgnoreTags has any tags */
	bool	IsEmpty() const;

	/** Return debug string */
	FString ToString() const;
};