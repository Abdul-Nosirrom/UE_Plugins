// Copyright 2023 CoC All rights reserved

#pragma once

#include "CoreMinimal.h"
#include "ActionSystem/ActionScript.h"
#include "ActionEvent_CancelBehavior.generated.h"

UENUM()
enum class EActionCancelBehavior
{
	Never,
	AfterTime,
	AfterTagsAdded
};

USTRUCT(BlueprintType)
struct FActionCancelBehavior
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly)
	EActionCancelBehavior Behavior = EActionCancelBehavior::AfterTime;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta=(EditCondition="Behavior==EActionCancelBehavior::AfterTime", EditConditionHides))
	float TimeUntilCanBeCancelled = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta=(EditCondition="Behavior==EActionCancelBehavior::AfterTagsAdded", EditConditionHides))
	FGameplayTagContainer AnyTagsAdded;
};

UCLASS(DisplayName="Can Cancel Behavior")
class ACTIONFRAMEWORK_API UActionEvent_CancelBehavior : public UActionEvent
{
	GENERATED_BODY()

protected:
	UActionEvent_CancelBehavior()
	{ DECLARE_ACTION_EVENT("Can Cancel Behavior", true) }
	
	UPROPERTY(EditDefaultsOnly, meta=(ShowOnlyInnerProperties))
	FActionCancelBehavior CancelBehavior;

	FTimerHandle CancelTimerHandle;
	
	virtual void Initialize(UGameplayAction* InOwnerAction) override;
	virtual void Cleanup() override;

	void OnActionStarted();
	
	UFUNCTION()
	void OnCancelTagsMaybeAdded(FGameplayTagContainer AddedTags);

	void SetCanCancel();
};
