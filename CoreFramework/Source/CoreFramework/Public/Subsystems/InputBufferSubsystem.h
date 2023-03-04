// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "BufferContainer.h"
#include "InputData.h"
#include "InputBufferPrimitives.h"
#include "InputBufferSubsystem.generated.h"

/* Profiling & Log Groups */
DECLARE_STATS_GROUP(TEXT("InputBuffer_Game"), STATGROUP_InputBuffer, STATCAT_Advanced);
DECLARE_LOG_CATEGORY_EXTERN(LogInputBuffer, Log, All);
/* ~~~~~~~~~~~~~~~~ */


/* FORWARD DECLARATIONS */
struct FBufferFrame;
struct FRawInputValue;
/*~~~~~~~~~~~~~~~~~~~~~*/

UCLASS()
class COREFRAMEWORK_API UInputBufferSubsystem : public ULocalPlayerSubsystem, public FTickableGameObject
{
	GENERATED_BODY()

	friend class ABufferedController;
	friend struct FBufferFrame;
	friend struct FInputFrameState;
	
 // BEGIN USubsystem Interface
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
 // END USubsystem Interface

// BEGIN FTickableGameObject Interface
	virtual void Tick( float DeltaTime ) override;
	virtual ETickableTickType GetTickableTickType() const override { return ETickableTickType::Always; }
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT( UInputBufferSubsystem, STATGROUP_Tickables ); }
	virtual bool IsTickableWhenPaused() const { return true; }
	virtual bool IsTickableInEditor() const { return false; }
// END FTickableGameObject Interface
	
public:
	/* ~~~~~ Input Buffer API ~~~~~ */
	/// @brief  Creates an input buffer from the given buffer map, and registers input and directional action events
	/// @param  TargetInputMap Input Buffer Data Asset To Initialize Actions With
	/// @param  InputComponent EIC Necessary For Binding To Its Events
	void AddMappingContext(UInputBufferMap* TargetInputMap, UEnhancedInputComponent* InputComponent);

	/// @brief Registers an input in the buffer as used, setting its current state to (-1) which is maintained until it's released
	/// @param Input Data asset corresponding to input to be consumed
	/// @return True if the input is consumed
	bool ConsumeButtonInput(const UInputAction* Input);

	/// @brief Registers an input in the buffer as used, setting its current state to (-1) which is maintained until it's released
	/// @param Input Data asset corresponding to input to be consumed
	/// @return True if the input is consumed
	bool ConsumeDirectionalInput(const UMotionAction* Input);

protected:
	/// @brief  Updates the buffer with input received from EIC [Fixed Tick Interval]
	void UpdateBuffer();

	/// @brief  Checks the buffers current state and broadcasts the appropriate input events [Tick constantly]
	void EvaluateEvents();

protected:
	/* ~~~~~ Input Registration & Initialization ~~~~~ */
	/// @brief  Adds the input map to the EIC subsystem & binds EIC events to buffer data
	void InitializeInputMapping(UEnhancedInputComponent* InputComponent);

	/// @brief Initializes buffer structures, called after InputMapping was initialized and IDs generated
	void InitializeInputBufferData();

	/// @brief Event triggered for an input once its been and continues to be triggered
	/// @param ActionInstance Action Instance From Input Action
	/// @param InputName FName descriptor of action used to index into Input Maps
	void TriggerInput(const FInputActionInstance& ActionInstance, const FName InputName);

	/// @brief Event triggered for an input once its no longer considered held
	/// @param ActionInstance Action Instance From Input Action
	/// @param InputName FName descriptor of action used to index into Input Maps
	void CompleteInput(const FInputActionInstance& ActionInstance, const FName InputName);
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

	void ProcessDirectionVector(const FVector2D& DirectionInputVector, FVector& ProcessedInputVector, FVector& OutPlayerForward, FVector& OutPlayerRight) const;
	
	void DisplayDebug(UCanvas* Canvas, const FDebugDisplayInfo& DebugDisplay, float& YL, float& YPos);
	
protected:
	/* References */
	TObjectPtr<UInputBufferMap> InputMap;

	/* CONSTANT BUFFER SETTINGS */
	static constexpr uint8 BUFFER_SIZE		= 15;
	static constexpr float TICK_INTERVAL	= 0.0167;
	
	/* ~~~~~ Initialization & Update Tracking ~~~~~ */
	UPROPERTY(Transient)
	bool bInitialized;

	UPROPERTY(Transient)
	double ElapsedTime;

	// NOTE: Static so buffer primitives can access it without having to go through a Subsystem getter (also in our case we're only ever gonna have 1 LocalPlayer)
	static TArray<FName> CachedActionIDs;
	static TMap<FName, FRawInputValue> RawValueContainer;

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
