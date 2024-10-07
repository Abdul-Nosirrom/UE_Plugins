// Copyright 2023 CoC All rights reserved

#pragma once

#include "CoreMinimal.h"
#include "ActionSystem/ActionScript.h"
#include "ActionCondition_ActivationCountLimit.generated.h"

/**
 * 
 */
UCLASS(DisplayName="Activation Count Limit")
 class ACTIONFRAMEWORK_API UActionCondition_ActivationCountLimit : public UActionCondition
{
	GENERATED_BODY()
protected:
	UActionCondition_ActivationCountLimit()
	{ DECLARE_ACTION_SCRIPT("Activation Count Limit") }
	
	/// @brief	Number of times the action can be activated before we start keeping track of reset tags that block it
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta=(ClampMin=1, UIMin=1))
	uint8 ActivationCount	= 1;

	/// @brief	After the action is activated, it will block it from being activated again until one of these tags have been added
	///			by the specified amount
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FGameplayTagContainer BlockActivationUntilTagsGranted; 
	
	uint32 NumTimesActivatedPreReset;
	
	virtual void Initialize(UGameplayAction* InOwnerAction) override;

	virtual bool DoesConditionPass() override;

	UFUNCTION()
	void TryResetActivationCount(FGameplayTagContainer AddedTags);

	// On Action Started
};
