// Copyright 2023 CoC All rights reserved


#include "ActionSystem/Conditions/ActionCondition_ActivationCountLimit.h"

#include "Components/ActionSystemComponent.h"

void UActionCondition_ActivationCountLimit::Initialize(UGameplayAction* InOwnerAction)
{
	Super::Initialize(InOwnerAction);

	GetCharacterInfo()->ActionSystemComponent->OnGameplayTagAddedDelegate.AddUniqueDynamic(this, &UActionCondition_ActivationCountLimit::TryResetActivationCount);
	NumTimesActivatedPreReset = 0;

	OwnerAction->OnGameplayActionStarted.Event.AddLambda([this]()
	{
		NumTimesActivatedPreReset++;
	});
}

bool UActionCondition_ActivationCountLimit::DoesConditionPass()
{
	return NumTimesActivatedPreReset < ActivationCount;
}

void UActionCondition_ActivationCountLimit::TryResetActivationCount(FGameplayTagContainer AddedTags)
{
	if (BlockActivationUntilTagsGranted.HasAny(AddedTags))
	{
		NumTimesActivatedPreReset = 0;
	}
}
