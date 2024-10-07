// Copyright 2023 CoC All rights reserved

#pragma once

#include "CoreMinimal.h"
#include "ActionSystem/ActionScript.h"
#include "ActionCondition_CoolDown.generated.h"


UCLASS(DisplayName="Cool Down")
class ACTIONFRAMEWORK_API UActionCondition_CoolDown : public UActionCondition
{
	GENERATED_BODY()

	friend class FGameplayWindow_Actions;

protected:
	UActionCondition_CoolDown()
	{ DECLARE_ACTION_SCRIPT("Fixed Cooldown") }

	UPROPERTY(EditDefaultsOnly, meta=(ClampMin="0", UIMin="0"))
	float CoolDown;

	FTimerHandle CooldownTimerHandle;
	bool bIsOnCooldown;

	virtual void Initialize(UGameplayAction* InOwnerAction) override;
	virtual void Cleanup() override;
	virtual void StartCooldown();
	virtual bool DoesConditionPass() override { return !bIsOnCooldown; }
};
