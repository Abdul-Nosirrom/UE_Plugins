// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Utility/BufferContainer.h"
#include "BufferedController.generated.h"

/* Forward Declarations */
struct FInputActionValue;
struct FInputActionInstance;
class UInputMappingContext;
/* ~~~~~~~~~~~~~~~~~~~ */

#pragma region Global Macros

/// @brief Collection of data we want to share across all buffer data structures. Keep it global and contained
///			in its own namespace as to not be messy and keep from consistently passing data around especially since
///			we'll be creating new structures at a 1/60 frequency
namespace BufferUtility
{
	static constexpr float TICK_INTERVAL{0.0167};
	static constexpr int BUFFER_SIZE{20};

	static TArray<FName> InputIDs;

	static TMap<FName, float> RawButtonContainer;
	static TMap<FName, FVector2D> RawAxisContainer;
}

#pragma endregion Global Macros

#pragma region Input Buffer Primitives

/* Defines the various inputs and their types, name corresponds to the Action name in the input system */
USTRUCT()
struct FRawInput
{
	GENERATED_BODY()
	
};

/* Primitive to define various motion commands */
USTRUCT()
struct FMotionCommand
{
	GENERATED_BODY()
	
};

#pragma endregion Input Buffer Primitives

#pragma region Frame States

/* Struct defining the state of a given input in a frame, whether its been used, being held, and so on  */
USTRUCT()
struct FInputFrameState
{
	GENERATED_BODY()

public:
	/// @brief Input Action ID Associated With This Object
	FName ID;
	/// @brief The value of the input. If an axis, the value will be the accentuation along that axis. If a button,
	///			the value will be the triggered time.
	float Value{0};
	/// @brief Input state value. (0) if no input, (-1) if the input was released the last frame, and time in which
	///			input has been held otherwise.
	int HoldTime{0};
	/// @brief True if the input has been consumed already and invalid for further use.
	bool bUsed{false};
		
public:
	explicit FInputFrameState(const FName InputID) : ID(InputID) {};
	
	/// @brief Checks the assigned input and resolves its state (Held, Released, Axis, etc...)
	void ResolveCommand();

	/// @brief Called when the input is being held
	/// @param Val Current value of the input in seconds
	void HoldUp(float Val);

	/// @brief Called when the assigned input has been released or has not been registered
	void ReleaseHold();

	/// @brief	Is an input registered in the current frame that has not already been used up?
	/// @return True if the input is usable
	bool CanExecute();

};

/* Row of an input buffer. Holds a collection of input frame states and defines the primitive of the actual buffer */
USTRUCT()
struct FBufferFrame
{
	GENERATED_BODY()

public:
	FBufferFrame() {};
	
	/// @brief Initializes a row of the input buffer
	void InitializeFrame();

	/// @brief Update the state of all inputs in the current frame, resolving any open commands 
	void UpdateFrameState();

	/// @brief Copies over the frame state into here
	/// @param FrameState Frame state to copy
	void CopyFrameState(FBufferFrame& FrameState);
	
public:
	/// @brief State of each input on a given frame
	TMap<FName, FInputFrameState> InputsFrameState;
};

#pragma endregion Frame States

#pragma region Input Buffer Core

USTRUCT(BlueprintType)
struct FInputBuffer
{
	GENERATED_BODY()

	FInputBuffer();

	/* ~~~~~ Core Data ~~~~~ */
	
	/// @brief	The rows of the input buffer, each containing a column corresponding to each input type
	///			each row is an input buffer frame (FBufferFrame), corresponding to the state of each input
	///			at the buffer frame (i) 
	TBufferContainer<FBufferFrame> InputBuffer;
	
	/// @brief	Holds the frame of each input in which it can be used. (-1) corresponds to no input that can used,
	///			meaning its not been registered, or been held for a while such that its no longer valid
	TMap<FName, uint32> ButtonInputCurrentState;

	/// @brief	Holds the frame of each motion command which it can be used
	//TMap<FName, uint32> MotionInputCurrentState;

	/* ~~~~~ Buffer Update Loop ~~~~~ */

	/// @brief	Called each buffer frame update. Clears out last row and samples recent input in the top-most row.
	void UpdateBuffer();
	
	/// @brief Registers an input in the buffer as used, setting its current state to (-1) which is maintained until it's released
	/// @param InputID ID of the input 
	void UseInput(FName InputID);
};

#pragma endregion Input Buffer Core


//DECLARE_DELEGATE_OneParam(FRegisterInputDelegate, const int32);

UCLASS()
class MOVEMENTTESTING_API ABufferedController : public APlayerController
{
	GENERATED_BODY()

protected:
	FInputBuffer InputBufferObject;
	
public:
	// Sets default values for this actor's properties
	ABufferedController();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input Mapping")
	TObjectPtr<UInputMappingContext> InputMap;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	virtual void SetupInputComponent() override;

	void TriggerInput(const FInputActionInstance& ActionInstance, const FName InputName);

	void CompleteInput(const FInputActionValue& ActionValue, const FName InputName);

	virtual void DisplayDebug(UCanvas* Canvas, const FDebugDisplayInfo& DebugDisplay, float& YL, float& YPos) override;
};
