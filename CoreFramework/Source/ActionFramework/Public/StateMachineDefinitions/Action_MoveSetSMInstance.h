// 

#pragma once

#include "CoreMinimal.h"
#include "ActionStateFlow.h"
#include "SMStateMachineInstance.h"
#include "Action_MoveSetSMInstance.generated.h"

/**
 * 
 */
UCLASS(Blueprintable, BlueprintType, ClassGroup = ActionManager, hideCategories = (SMStateInstance, Display, Color), meta = (DisplayName = "Move-Set", ShortTooltip="Container state machine, minimal logic inside of it"))
class ACTIONFRAMEWORK_API UAction_MoveSetSMInstance : public USMStateMachineInstance, public IActionStateFlow
{
	GENERATED_BODY()

public:
	UAction_MoveSetSMInstance();

protected:
	// BEGIN USMStateInstance Interface
	virtual void OnStateBegin_Implementation() override {};
	virtual void OnStateUpdate_Implementation(float DeltaSeconds) override {};
	virtual void OnStateEnd_Implementation() override {};
	// END USMStateInstance Interface


#pragma region Shared Amongst State Definitions
public:
	/* Base State Parameters */

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

	/// @brief  These tags will be granted upon entry
	UPROPERTY(Category=Tags, EditAnywhere, BlueprintReadWrite)
	FGameplayTagContainer GrantTags;
	
	/// @brief  Must have all these tags to enter the state
	UPROPERTY(Category=Tags, EditAnywhere, BlueprintReadWrite)
	FGameplayTagContainer RequiredTags;
	
	/// @brief  Cant enter the state if has any of these tag
	UPROPERTY(Category=Tags, EditAnywhere, BlueprintReadWrite)
	FGameplayTagContainer BlockTags;

	/* ~~~~~~~~~~~~~~~~~~~~~ */
protected:
	UPROPERTY(Transient)
	ARadicalPlayerCharacter* CharacterOwner;
	UPROPERTY(Transient)
	UInputBufferSubsystem* InputBuffer;

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
#pragma endregion Shared Amongst State Definitions
};
