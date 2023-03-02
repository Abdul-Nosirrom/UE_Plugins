// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "BufferContainer.h"
#include "InputData.h"
#include "InputBufferSubsystem.generated.h"

/* Profiling Groups */
DECLARE_STATS_GROUP(TEXT("InputBuffer_Game"), STATGROUP_InputBuffer, STATCAT_Advanced);
/* ~~~~~~~~~~~~~~~~ */

/* FORWARD DECLARATIONS */
struct FBufferFrame;
/*~~~~~~~~~~~~~~~~~~~~~*/

UCLASS(Abstract)
class COREFRAMEWORK_API UInputBufferSubsystem : public ULocalPlayerSubsystem, public FTickableGameObject
{
	GENERATED_BODY()
	
 // USubsystem implementation Begin
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
 // USubsystem implementation End

public:
	/* ~~~~~ Input Buffer API ~~~~~ */
	void UpdateBuffer();

	void EvaluateEvents();

	/// @brief Registers an input in the buffer as used, setting its current state to (-1) which is maintained until it's released
	/// @param Input Data asset corresponding to input to be consumed
	/// @return True if the input is consumed
	bool ConsumeButtonInput(const UInputAction* Input);

	/// @brief Registers an input in the buffer as used, setting its current state to (-1) which is maintained until it's released
	/// @param Input Data asset corresponding to input to be consumed
	/// @return True if the input is consumed
	bool ConsumeDirectionalInput(const UMotionAction* Input);

protected:
	/* ~~~~~ Input Registration & Initialization ~~~~~ */
	void InitializeInputMapping(UInputBufferMap* InputMap, UEnhancedInputComponent* InputComponent);

	/// @brief Event triggered for an input once its been and continues to be triggered
	/// @param ActionInstance Action Instance From Input Action
	/// @param InputName FName descriptor of action used to index into Input Maps
	void TriggerInput(const FInputActionInstance& ActionInstance, const FName InputName);

	/// @brief Event triggered for an input once its no longer considered held
	/// @param ActionValue Action Instance From Input Action
	/// @param InputName FName descriptor of action used to index into Input Maps
	void CompleteInput(const FInputActionValue& ActionValue, const FName InputName);

private:
	/* ~~~~~ Primitive Data ~~~~~ */
	uint8 BufferSize;

	TArray<FName> InputIDs;
	TMap<FName, bool> RawButtonContainer;
	TMap<FName, FVector2D> RawAxisContainer;

	/* ~~~~~ Managed Data ~~~~~ */
	/// @brief	The rows of the input buffer, each containing a column corresponding to each input type
	///			each row is an input buffer frame (FBufferFrame), corresponding to the state of each input
	///			at the buffer frame (i) 
	TBufferContainer<FBufferFrame> InputBuffer;

	/// @brief	Holds the oldest frame of each input in which it can be used. (-1) corresponds to no input that can used,
	///			meaning its not been registered, or been held for a while such that its no longer valid. Oldest frame to more
	///			easily check chorded actions/input sequence
	TMap<FName, int8> ButtonOldestValidFrame;
	
#pragma region EVENTS
	
 // EVENT Input Press Ready To Consume
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FInputPress, const UInputAction*, InputAction, const FInputActionValue&, Value); // NOTE: Can maybe get a lot more info passed along, for now this is enough though
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDirectionalInputRegisteredDelegate, const UMotionAction*, DirectionalInput);

 // EVENT Input Held Ready To Consume
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FInputHeld, const UInputAction*, FOnInputAction, const FInputActionValue&, Value, float, TimeHeld);
 // EVENT Input Release Ready To Consume
	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FInputReleased);

	
public:
	UPROPERTY(BlueprintAssignable)
	FInputPress InputPressedDelegateSignature;
			  
	UPROPERTY(BlueprintAssignable)
	FInputHeld InputHeldDelegateSignature;
			  
	UPROPERTY(BlueprintAssignable)
	FInputReleased InputReleasedDelegateSignature;

#pragma endregion EVENTS
};
