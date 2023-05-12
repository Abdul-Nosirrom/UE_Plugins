// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ActionStateFlow.h"
#include "SMStateInstance.h"
#include "ActionSystem/GameplayAction.h"
#include "Action_CoreStateInstance.generated.h"


/// @brief Base gameplay state instance. Holds shared state defaults, inherit from this to build gameplay functionality
UCLASS(Blueprintable, BlueprintType, ClassGroup = ActionManager, hideCategories = (SMStateInstance, Display, Color), meta = (DisplayName = "Core State", ShortTooltip="Base State Class For Creating Gameplay Actions"))
class ACTIONFRAMEWORK_API UAction_CoreStateInstance : public USMStateInstance, public IActionStateFlow
{
	GENERATED_BODY()

public:
	UAction_CoreStateInstance();
	
protected:
	
	// BEGIN USMStateInstance Interface
	virtual void OnRootStateMachineStart_Implementation() override final;
	virtual void OnStateBegin_Implementation() override;
	virtual void OnStateEnd_Implementation() override;
	// END USMStateInstance Interface

	void OnActionEnded(UGameplayAction* Action, bool bWasCancelled) const;

#pragma region Shared Amongst State Definitions
public:
	/* Base State Parameters */

	UPROPERTY(Category=Action, EditAnywhere, BlueprintReadWrite, meta=(BlueprintBaseOnly))
	UGameplayActionData* ActionData; // TODO: Make SoftObjectPtr?
	UPROPERTY(Transient)
	TSoftObjectPtr<UGameplayAction> ActionInstance;

	// BEGIN Inputs
	UPROPERTY(Category=Input, EditAnywhere, BlueprintReadWrite)
	TEnumAsByte<EStateInputType> InputType = None;
	
	UPROPERTY(Category=Input, EditAnywhere, BlueprintReadWrite)
	TEnumAsByte<EBufferTriggerEvent> TriggerEvent;
	UPROPERTY(Category=Input, EditAnywhere, BlueprintReadWrite)
	TEnumAsByte<EDirectionalSequenceOrder> SequenceOrder;
	UPROPERTY(Category=Input, EditAnywhere, BlueprintReadWrite)
	bool bConsiderOrder;
	
	UPROPERTY(Category=Input, EditAnywhere, BlueprintReadWrite)
	TObjectPtr<UInputAction> ButtonInput;
	UPROPERTY(Category=Input, EditAnywhere, BlueprintReadWrite)
	TObjectPtr<UInputAction> SecondButtonInput;
	UPROPERTY(Category=Input, EditAnywhere, BlueprintReadWrite)
	TObjectPtr<UMotionAction> DirectionalInput;
	// END Inputs

	/* ~~~~~~~~~~~~~~~~~~~~~ */
protected:
	UPROPERTY(Transient)
	TWeakObjectPtr<UActionSystemComponent> ActionSystem;

	/// @brief  Shared amongst state definitions as they both share the same base params and reveal them the same way
	virtual void ConstructionScript_Implementation() override;
	
	// BEGIN IActionStateFlow Interface
	FORCEINLINE virtual bool GetConsiderOrder() const override {return bConsiderOrder;}
	FORCEINLINE virtual EStateInputType GetStateInputType() const override {return InputType;}
	FORCEINLINE virtual EBufferTriggerEvent GetTriggerType() const override {return TriggerEvent;}
	FORCEINLINE virtual EDirectionalSequenceOrder GetSequenceOrder() const override {return SequenceOrder;}
	FORCEINLINE virtual UInputAction* GetFirstButtonAction() const override {return ButtonInput.Get();}
	FORCEINLINE virtual UInputAction* GetSecondButtonAction() const override {return SecondButtonInput.Get();}
	FORCEINLINE virtual UMotionAction* GetDirectionalAction() const override {return DirectionalInput.Get();}
	// END IActionStateFlow Interface

	virtual bool CanEnter() override;
	virtual bool CanExit() override;
#pragma endregion Shared Amongst State Definitions
};
