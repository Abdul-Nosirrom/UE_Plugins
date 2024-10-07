// Copyright 2023 CoC All rights reserved

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTags.h"
#include "CharacterActionSet.generated.h"

class UGameplayActionData;

USTRUCT(BlueprintType)
struct FActionSetEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool bEnabled	= true;
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	UGameplayActionData* Action;

	bool operator==(const FActionSetEntry& Other) const { return Action == Other.Action; }
};

UCLASS()
class ACTIONFRAMEWORK_API UCharacterActionSet : public UDataAsset
{
	GENERATED_BODY()

public:
	
	UPROPERTY(Category=Default, EditDefaultsOnly, meta=(ForceInlineRow, ShowOnlyInnerProperties))
	TMap<FGameplayTag, FActionSetEntry> DefaultActions;

	UPROPERTY(Category=Base, EditDefaultsOnly, meta=(NoElementDuplicate, TitleProperty="Action"))
	TArray<FActionSetEntry> BaseActions;

	UPROPERTY(Category=Additive, EditDefaultsOnly, meta=(NoElementDuplicate, TitleProperty="Action"))
	TArray<FActionSetEntry> GlobalAdditiveActions;

};
