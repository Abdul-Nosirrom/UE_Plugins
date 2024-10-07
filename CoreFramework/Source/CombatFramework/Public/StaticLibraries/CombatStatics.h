// Copyright 2023 CoC All rights reserved

#pragma once

#include "CoreMinimal.h"
#include "Data/CombatData.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CombatStatics.generated.h"

/**
 * 
 */
UCLASS()
class COMBATFRAMEWORK_API UCombatStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:

	UFUNCTION(Category=Combat, BlueprintPure, meta=(Keywords="Combat, Hit, Component"))
	static class UCombatActionSystemComponent* GetCombatSystemFromActor(const AActor* Actor, bool bLookForComponent=true);
	
	// Lets only call damage through this function rather than directly, it'll make things easier to deal with as we can ensure OnHitConfirm, hit pause, and hit effects are done
	UFUNCTION(Category=Combat, BlueprintCallable, meta=(DefaultToSelf=Attacker, Keywords="Combat, Hit, GetHit"))
	static bool CombatHit(FAttackData AttackData, AActor* Attacker, AActor* Victim);
};
