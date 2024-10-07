// Copyright 2023 CoC All rights reserved

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "CombatUtilitiesSubsystem.generated.h"

/**
 * 
 */
UCLASS()
class COMBATFRAMEWORK_API UCombatUtilitiesSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:

	void SetHitStop(AActor* Attacker, AActor* Victim, float Duration);
};
