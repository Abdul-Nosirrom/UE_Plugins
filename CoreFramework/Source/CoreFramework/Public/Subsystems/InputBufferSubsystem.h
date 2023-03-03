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

UCLASS()
class COREFRAMEWORK_API UInputBufferSubsystem : public ULocalPlayerSubsystem//, public FTickableGameObject
{
	GENERATED_BODY()

	friend struct FBufferFrame;
	friend struct FInputFrameState;
	
 // USubsystem implementation Begin
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
 // USubsystem implementation End

public:
	bool bInitialized{false};
	
	/* ~~~~~ Input Buffer API ~~~~~ */
	void AddMappingContext(UInputBufferMap* TargetInputMap, UEnhancedInputComponent* InputComponent);
	
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
	void InitializeInputMapping(UEnhancedInputComponent* InputComponent);

	void InitializeInputBufferData();

	/// @brief Event triggered for an input once its been and continues to be triggered
	/// @param ActionInstance Action Instance From Input Action
	/// @param InputName FName descriptor of action used to index into Input Maps
	void TriggerInput(const FInputActionInstance& ActionInstance, const FName InputName);

	/// @brief Event triggered for an input once its no longer considered held
	/// @param ActionValue Action Instance From Input Action
	/// @param InputName FName descriptor of action used to index into Input Maps
	void CompleteInput(const FInputActionValue& ActionValue, const FName InputName);

public:
	void DisplayDebug(UCanvas* Canvas, const FDebugDisplayInfo& DebugDisplay, float& YL, float& YPos);
	
private:
	/* References */
	TObjectPtr<UInputBufferMap> InputMap;
	
	/* ~~~~~ Primitive Data ~~~~~ */
	uint8 BufferSize;

	// NOTE: Static so buffer primitives can access it without having to go through a Subsystem getter (also in our case we're only ever gonna have 1 LocalPlayer)
	static TArray<FName> InputActionIDs;
	static TMap<FName, bool> RawButtonContainer;	// Most recent button state as received from EIC
	static TMap<FName, FVector2D> RawAxisContainer;	// Most recent axis state as received from EIC

	/* ~~~~~ Managed Data ~~~~~ */
	/// @brief	The rows of the input buffer, each containing a column corresponding to each input type
	///			each row is an input buffer frame (FBufferFrame), corresponding to the state of each input
	///			at the buffer frame (i)
	TBufferContainer<FBufferFrame> InputBuffer;

	/// @brief	Holds the oldest frame of each input in which it can be used. (-1) corresponds to no input that can used,
	///			meaning its not been registered, or been held for a while such that its no longer valid. Oldest frame to more
	///			easily check chorded actions/input sequence
	UPROPERTY(Transient)
	TMap<FName, int8> ButtonInputValidFrame;

	/// @brief	Holds the oldest frame of which a directional input was registered valid. (-1) corresponds to no input that can be used,
	///			meaning its not been registered (DI have no concept of "held"). Frame held is the oldest frame in the buffer in which the input
	///			was valid.
	UPROPERTY(Transient)
	TMap<FName, int8> DirectionalInputValidFrame;

#pragma region EVENTS
	
 // EVENT Input Press Ready To Consume
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FInputPressedSignature, const UInputAction*, InputAction, const FInputActionValue&, Value); // NOTE: Can maybe get a lot more info passed along, for now this is enough though
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDirectionalInputRegisteredSignature, const UMotionAction*, DirectionalInput);

 // EVENT Input Held Ready To Consume
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FInpueHeldSignature, const UInputAction*, InputAction, const FInputActionValue&, Value, float, TimeHeld);
 // EVENT Input Release Ready To Consume
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FInputReleasedSignature, const UInputAction*, InputAction);

// EVENT Chorded Action Sequence Ready To Consume
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FDirectionalAndButtonRegisteredSignature, const UInputAction*, InputAction, const UMotionAction*, DirectionalInput);
	
public:
	UPROPERTY(BlueprintAssignable)
	FInputPressedSignature InputPressedDelegate;

	UPROPERTY(BlueprintAssignable)
	FDirectionalInputRegisteredSignature DirectionalInputRegisteredDelegate;
			  
	UPROPERTY(BlueprintAssignable)
	FInpueHeldSignature InputHeldDelegate;
			  
	UPROPERTY(BlueprintAssignable)
	FInputReleasedSignature InputReleasedDelegate;

	UPROPERTY(BlueprintAssignable)
	FDirectionalAndButtonRegisteredSignature DirectionalAndButtonDelegate;

#pragma endregion EVENTS
};
