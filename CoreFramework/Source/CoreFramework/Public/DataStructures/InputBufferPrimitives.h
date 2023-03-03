// Copyright 2023 Abdulrahmen Almodaimegh. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "BufferContainer.h"
#include "InputBufferPrimitives.generated.h"


/* Struct defining the state of a given input in a frame, whether its been used, being held, and so on  */
USTRUCT()
struct FInputFrameState
{
	GENERATED_BODY()

public:
	/// @brief	Input Action ID Associated With This Object
	FName ID;

	// NOTE: Should we just store the FInputActionValue here instead of Value & Vector Value? I think so.
	
	/// @brief	The value of the input. If an axis, the value will be the accentuation along that axis. If a button,
	///			the value will be 1 or 0 depending on the state of the button.
	float Value{0};

	/// @brief  If an axis value, we register the actual normalized axis vector value here (we can retrieve the non-normalized
	///			one too given that we have @see Value which represents the accentuation/magnitude)
	FVector2D VectorValue;
	
	/// @brief	Input state value. (0) if no input, (-1) if the input was released the last frame, and time in which
	///			input has been held otherwise.
	int HoldTime{0};
	
	/// @brief	True if the input has been consumed already and invalid for further use.
	bool bUsed{false};

	float DebugTime{0};
public:
	FInputFrameState() {};
	explicit FInputFrameState(const FName InputID) : ID(InputID) {};
	
	/// @brief Checks the assigned input and resolves its state (Held, Released, Axis, etc...)
	void ResolveCommand();

	/// @brief Called when the input is being held
	/// @param Val Current value of the input in seconds
	void HoldUp(float Val);

	void RegisterVector(const FVector2D RawAxis) { VectorValue = RawAxis; }

	/// @brief Called when the assigned input has been released or has not been registered
	void ReleaseHold();

	/// @brief	Is an input registered in the current frame that has not already been used up?
	/// @return True if the input is usable
	bool CanExecute() const;

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
