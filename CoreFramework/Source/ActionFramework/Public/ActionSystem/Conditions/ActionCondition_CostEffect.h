// Copyright 2023 CoC All rights reserved

#pragma once

#include "CoreMinimal.h"
#include "TimerManager.h"
#include "ActionSystem/ActionScript.h"
#include "AttributeSystem/EntityEffectTypes.h"
#include "ActionCondition_CostEffect.generated.h"

/**
 * 
 */
UCLASS(DisplayName="Cost Effect")
class ACTIONFRAMEWORK_API UActionCondition_CostEffect : public UActionCondition
{
	GENERATED_BODY()
protected:
	UActionCondition_CostEffect()
	{ DECLARE_ACTION_SCRIPT("Cost Effect") }

	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<UEntityEffect> CostEffect;
	UPROPERTY(EditDefaultsOnly)
	bool bShouldCancelActionIfCantApply = true;
	UPROPERTY(EditDefaultsOnly)
	bool bStopDurationEffectOnActionEnd = false;

	FEntityEffectSpec CostSpec;
	FEntityEffectContext EffectContext;
	FActiveEntityEffectHandle ActiveEffectHandle;
	
	FTimerDelegate CostEffectTimerDelegate;
	FTimerHandle CostEffectTimerHandle;

	// To bind to activation so we can apply the effect
	virtual void Initialize(UGameplayAction* InOwnerAction) override;

	virtual void Cleanup() override;

	virtual bool DoesConditionPass() override;

	void ApplyEffect();

	void CheckPeriodicApplication();

	void OnOwnerActionEnded();
};
