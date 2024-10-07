// Copyright 2023 CoC All rights reserved

#pragma once

#include "CoreMinimal.h"
#include "ActionSystem/ActionScript.h"
#include "ActionTrigger_LevelPrimitive.generated.h"


UCLASS(DisplayName="Level Primitive Registered")
class ACTIONFRAMEWORK_API UActionTrigger_LevelPrimitive : public UActionTrigger
{
	GENERATED_BODY()
protected:
	UActionTrigger_LevelPrimitive()
	{ DECLARE_ACTION_SCRIPT("Level Primitive Trigger") }
	
	UPROPERTY(EditDefaultsOnly, meta=(Categories="LevelPrimitive"))
	FGameplayTag LevelPrimitiveTag;

	virtual void SetupTrigger(int32 Priority) override;
	virtual void CleanupTrigger() override;

	UFUNCTION()
	void LevelPrimitiveRegistered(FGameplayTag PrimitiveTag);
	UFUNCTION()
	void LevelPrimitiveUnRegistered(FGameplayTag PrimitiveTag);
};
