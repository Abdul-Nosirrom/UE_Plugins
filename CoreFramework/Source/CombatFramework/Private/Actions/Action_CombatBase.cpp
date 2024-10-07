// Copyright 2023 CoC All rights reserved


#include "Actions/Action_CombatBase.h"

#include "RadicalCharacter.h"
#include "Data/CombatData.h"

UE_DEFINE_GAMEPLAY_TAG(TAG_Action_Combat, "Action.Combat")

void UAction_CombatBase::OnActionActivated_Implementation()
{
	bRegisteredZeroInput = false;
}

void UAction_CombatBase::OnActionTick_Implementation(float DeltaTime)
{
	if (!bRegisteredZeroInput) bRegisteredZeroInput = GetRadicalOwner()->GetLastMovementInputVector().IsZero();
	
	if (bRegisteredZeroInput && CanBeCanceled() && GetRadicalOwner()->GetLastMovementInputVector().Size() > 0.2f)
	{
		EndAction();
	}
}

void UAction_CombatBase::OnActionEnd_Implementation()
{}

bool UAction_CombatBase::EnterCondition_Implementation()
{
	return true;
}
