// Copyright 2023 CoC All rights reserved

#pragma once

#include "CoreMinimal.h"
#include "ActionSystem/ActionScript.h"
#include "ActionEvent_ExitBehavior.generated.h"


UENUM()
enum class EActionAutoExitReason
{
	TagsAdded,
	InputReleased
};

UCLASS(DisplayName="Auto Exit Behavior")
class ACTIONFRAMEWORK_API UActionEvent_ExitBehavior : public UActionEvent
{
	GENERATED_BODY()
protected:
	UActionEvent_ExitBehavior()
	{ DECLARE_ACTION_EVENT("Auto-Exit Behavior", true) }

	UPROPERTY(EditAnywhere)
	EActionAutoExitReason Reason;
	UPROPERTY(EditDefaultsOnly, meta=(EditCondition="Reason==EActionAutoExitReason::TagsAdded", EditConditionHides))
	FGameplayTagContainer ExitWhenAnyTagsAdded;

	FButtonBinding ReleasedInputDelegate;

	virtual void Initialize(UGameplayAction* InOwnerAction) override;
	void Setup();
	void Cleanup();
	UFUNCTION()
	void MaybeExitAction(FGameplayTagContainer AddedTags);

	UFUNCTION()
	void MaybeExitActionFromInput(const FInputActionValue& Value, float ElapsedTime);

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif 
};
