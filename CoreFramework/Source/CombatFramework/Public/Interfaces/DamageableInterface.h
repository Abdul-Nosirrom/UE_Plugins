// Copyright 2023 CoC All rights reserved

#pragma once

#include "CoreMinimal.h"
#include "Data/CombatData.h"
#include "UObject/Interface.h"
#include "DamageableInterface.generated.h"

// This class does not need to be modified.
UINTERFACE(BlueprintType)
class COMBATFRAMEWORK_API UDamageableInterface : public UInterface
{
	GENERATED_BODY()
};


DECLARE_MULTICAST_DELEGATE_OneParam(FOnGotHit, AActor*)

class COMBATFRAMEWORK_API IDamageableInterface
{
	GENERATED_BODY()

	// Add interface functions to this class. This is the class that will be inherited to implement this interface.
public:

	UFUNCTION(Category=Combat, BlueprintNativeEvent)
	bool GetHit(FAttackData AttackData, AActor* Attacker);
	virtual bool GetHit_Implementation(FAttackData AttackData, AActor* Attacker) { return true; }

	UFUNCTION(Category=Combat, BlueprintCallable, BlueprintNativeEvent)
	void OnHitConfirm(FAttackData AttackData, AActor* Victim);
	virtual void OnHitConfirm_Implementation(FAttackData AttackData, AActor* Victim) {}

	FOnGotHit OnGotHitDelegate;
};
