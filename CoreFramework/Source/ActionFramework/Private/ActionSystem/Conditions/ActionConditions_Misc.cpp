// Copyright 2023 CoC All rights reserved


#include "ActionSystem/Conditions/ActionConditions_Misc.h"

#include "RadicalCharacter.h"
#include "RadicalMovementComponent.h"
#include "ActionSystem/GameplayAction.h"

void UActionCondition_PhysicsState::Initialize(UGameplayAction* InOwnerAction)
{
	Super::Initialize(InOwnerAction);

	OwnerAction->OnGameplayActionStarted.Event.AddLambda([this]
	{
		if (bEndActionOnStateChange)
			GetCharacterInfo()->CharacterOwner->MovementStateChangedDelegate.AddUniqueDynamic(this, &UActionCondition_PhysicsState::OnPhysicsStateChanged);
	});

	OwnerAction->OnGameplayActionEnded.Event.AddLambda([this]
	{
		if (bEndActionOnStateChange)
			GetCharacterInfo()->CharacterOwner->MovementStateChangedDelegate.RemoveDynamic(this, &UActionCondition_PhysicsState::OnPhysicsStateChanged);
	});
}

bool UActionCondition_PhysicsState::DoesConditionPass()
{
	if (GetCharacterInfo()->MovementComponent.IsValid())
	{
		const bool bShouldCheckCoyoteTime = (PhysicsState!=STATE_Falling && ComparisonMethod==EActionConditionStateComparison::Equals) || (PhysicsState==STATE_Falling && ComparisonMethod==EActionConditionStateComparison::NotEquals);
		const bool bCoyoteTime = (CurrentActorInfo->MovementComponent->GetTimeSinceLeftGround() <= CoyoteTime) || !(CurrentActorInfo->CharacterOwner->GetCharacterMovement()->IsFalling());

		const bool bDoesEqual = GetCharacterInfo()->MovementComponent->GetMovementState() == PhysicsState;
		return (ComparisonMethod == EActionConditionStateComparison::Equals ? bDoesEqual : !bDoesEqual) || (bShouldCheckCoyoteTime && bCoyoteTime);
	}

	return false;
}

void UActionCondition_PhysicsState::OnPhysicsStateChanged(ARadicalCharacter* Char, EMovementState PrevState)
{
	if (!DoesConditionPass() && OwnerAction.IsValid() && OwnerAction->IsActive())
	{
		OwnerAction->SetCanBeCanceled(true);
		OwnerAction->CancelAction();
	}
}

void UActionCondition_ConditionalCondition::Initialize(UGameplayAction* InOwnerAction)
{
	Super::Initialize(InOwnerAction);

	if (ConditionRequirement && Condition)
	{
		ConditionRequirement->Initialize(InOwnerAction);
		Condition->Initialize(InOwnerAction);
	}
}

bool UActionCondition_ConditionalCondition::DoesConditionPass()
{
	if (ConditionRequirement && Condition)
	{
		if (ConditionRequirement->DoesConditionPass()) return Condition->DoesConditionPass();
	}

	return true;
}

void UActionCondition_OrCondition::Initialize(UGameplayAction* InOwnerAction)
{
	Super::Initialize(InOwnerAction);

	if (ConditionOne) ConditionOne->Initialize(InOwnerAction);
	if (ConditionTwo) ConditionTwo->Initialize(InOwnerAction);
}

bool UActionCondition_OrCondition::DoesConditionPass()
{
	const bool bCondiOne = ConditionOne ? ConditionOne->DoesConditionPass() : false;
	const bool bCondiTwo = ConditionTwo ? ConditionTwo->DoesConditionPass() : false;

	return bCondiOne || bCondiTwo;
}

#if WITH_EDITOR
FString UActionCondition_PhysicsState::GetEditorFriendlyName() const
{
	FString Name = "State";
	if (ComparisonMethod == EActionConditionStateComparison::Equals) Name.Append("==");
	else Name.Append("!=");

	switch (PhysicsState)
	{
		case STATE_Grounded: Name.Append("Grounded"); break;
		case STATE_Falling: Name.Append("Aerial"); break;
		case STATE_General: Name.Append("General"); break;
	}
	return Name;
}

FString UActionCondition_ConditionalCondition::GetEditorFriendlyName() const
{
	if (ConditionRequirement && Condition)
	{
		return FString::Printf(TEXT("[%s] if [%s]"), *Condition->GetEditorFriendlyName(), *ConditionRequirement->GetEditorFriendlyName());
	}
	
	return "[INVALID] Conditional Condition";
}

FString UActionCondition_OrCondition::GetEditorFriendlyName() const
{
	const FString ConditionOneName = ConditionOne ? ConditionOne->GetEditorFriendlyName() : "FALSE";
	const FString ConditionTwoName = ConditionTwo ? ConditionTwo->GetEditorFriendlyName() : "FALSE";
	
	return FString::Printf(TEXT("[%s] OR [%s]"), *ConditionOneName, *ConditionTwoName);
}

#endif
