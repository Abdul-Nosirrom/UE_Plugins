// 

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "InputData.h"
#include "GameplayTags.h"
#include "ActionStateFlow.generated.h"

/* ~~~~~ Forward Declarations ~~~~~ */
class ARadicalCharacter;
class URadicalMovementComponent;
enum EBufferTriggerEvent;
enum EDirectionalSequenceOrder;


UENUM()
enum EStateInputType
{
	None,
	Button,
	ButtonSequence,
	Directional,
	DirectionButtonSequence
};

DECLARE_DELEGATE(FOnForceExitSignature);

// This class does not need to be modified.
UINTERFACE(MinimalAPI, Category="State Machine Control Flow", meta=(CannotImplementInterfaceInBlueprint))
class UActionStateFlow : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class ACTIONFRAMEWORK_API IActionStateFlow
{
	friend class UAction_TransitionStateInstance;
	
	GENERATED_BODY()
	
public:
	// BEGIN General Accessors (so stuff can be marked as UPROPERTY in their respective classes)
	virtual bool GetConsiderOrder() const = 0;
	virtual EStateInputType GetStateInputType() const = 0;
	virtual EBufferTriggerEvent GetTriggerType() const = 0;
	virtual EDirectionalSequenceOrder GetSequenceOrder() const = 0;
	virtual UInputAction* GetFirstButtonAction() const = 0;
	virtual UInputAction* GetSecondButtonAction() const = 0;
	virtual UMotionAction* GetDirectionalAction() const = 0;
	// END General Accessors
	
protected:
	// BEGIN Input Delegates
	FDirectionalActionSignature DirectionalInputDelegate;
	FDirectionalAndActionSequenceSignature DirectionalAndButtonInputDelegate;
	FInputActionSequenceSignature ButtonSequenceInputDelegate;
	FInputActionEventSignature ButtonInputDelegate;
	// END Input Delegates
	
	// BEGIN Control Flow
	FOnForceExitSignature ForceExitDelegate;
	
	virtual bool CanExit() { return true; };
	virtual bool CanEnter() { return true; };
	// END Control Flow
	
	// BEGIN InputBuffer Interface
	UFUNCTION()
	virtual void DirectionalInputBinding() {};
	UFUNCTION()
	virtual void DirectionalAndButtonInputBinding(const FInputActionValue& ActionValue, float ActionElapsedTime) {};
	UFUNCTION()
	virtual void ButtonSequenceInputBinding(const FInputActionValue& ValueOne, const FInputActionValue& ValueTwo) {};
	UFUNCTION()
	virtual void ButtonInputBinding(const FInputActionValue& Value, float ElapsedTime) {};
	// END InputBuffer Interface
};
