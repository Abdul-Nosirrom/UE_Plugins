// Copyright 2023 CoC All rights reserved

#pragma once

#include "CoreMinimal.h"
#include "ActionSystem/ActionScript.h"
#include "ActionConditions_Misc.generated.h"


enum EMovementState : int;

UENUM()
enum class EActionConditionStateComparison
{
	Equals		UMETA(DisplayName="=="),
	NotEquals	UMETA(DisplayName="!=")
};

UCLASS(DisplayName="Valid Physics State")
class UActionCondition_PhysicsState : public UActionCondition
{
	GENERATED_BODY()
protected:
	UActionCondition_PhysicsState() { DECLARE_ACTION_SCRIPT("Valid Physics State") }

	virtual void Initialize(UGameplayAction* InOwnerAction) override;
	
	UPROPERTY(EditAnywhere)
	EActionConditionStateComparison ComparisonMethod;
	UPROPERTY(EditAnywhere, meta=(InvalidEnumValues="STATE_None"))
	TEnumAsByte<EMovementState> PhysicsState;
	UPROPERTY(Category="Jump Params", EditAnywhere, meta=(EditCondition="(PhysicsState!=EMovementState::STATE_Falling && ComparisonMethod==EActionConditionStateComparison::Equals) || (PhysicsState==EMovementState::STATE_Falling && ComparisonMethod==EActionConditionStateComparison::NotEquals)", EditConditionHides,ClampMin="0", UIMin="0", ClampMax="1", UIMax="1"))
	float CoyoteTime = 0.f;
	UPROPERTY(EditAnywhere)
	bool bEndActionOnStateChange = true;

	virtual bool DoesConditionPass() override;

	UFUNCTION()
	void OnPhysicsStateChanged(ARadicalCharacter* Char, EMovementState PrevState);

#if WITH_EDITOR 
	virtual FString GetEditorFriendlyName() const override;
#endif
};

/// @brief	Checks a condition and if that passes, checks the proper condition. Otherwise if the conditional fails, we return true and don't check the actual condition
UCLASS(DisplayName="Conditional Condition")
class UActionCondition_ConditionalCondition : public UActionCondition
{
	GENERATED_BODY()

protected:
	UActionCondition_ConditionalCondition()
	{ DECLARE_ACTION_SCRIPT("Conditional Condition")}

	/// @brief	Only check Condition if this additional requirement passes, otherwise return true
	UPROPERTY(EditDefaultsOnly, Instanced)
	UActionCondition* ConditionRequirement;
	/// @brief	Condition to adhere by if the requirement passes
	UPROPERTY(EditDefaultsOnly, Instanced)
	UActionCondition* Condition;

	virtual void Initialize(UGameplayAction* InOwnerAction) override;
	virtual bool DoesConditionPass() override;

#if WITH_EDITOR
	virtual FString GetEditorFriendlyName() const override;
#endif 
};


UCLASS(DisplayName="Boolean OR Operation")
class UActionCondition_OrCondition : public UActionCondition
{
	GENERATED_BODY()

protected:
	UActionCondition_OrCondition()
	{ DECLARE_ACTION_SCRIPT("Boolean OR Operation")}

	UPROPERTY(EditDefaultsOnly, Instanced)
	UActionCondition* ConditionOne;
	UPROPERTY(EditDefaultsOnly, Instanced)
	UActionCondition* ConditionTwo;

	virtual void Initialize(UGameplayAction* InOwnerAction) override;
	virtual bool DoesConditionPass() override;

#if WITH_EDITOR
	virtual FString GetEditorFriendlyName() const override;
#endif 
};