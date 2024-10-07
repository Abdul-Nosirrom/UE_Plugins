// Copyright 2023 CoC All rights reserved

#pragma once

#include "CoreMinimal.h"
#include "RadicalMovementComponent.h"
#include "ActionSystem/GameplayAction.h"
#include "Action_CombatBase.generated.h"


UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Action_Combat)


UCLASS(DisplayName="Combat Base")
class COMBATFRAMEWORK_API UAction_CombatBase : public UPrimaryAction
{
	GENERATED_BODY()
protected:
	SETUP_PRIMARY_ACTION(TAG_Action_Combat, true, false, false);

	bool bRegisteredZeroInput;
};
