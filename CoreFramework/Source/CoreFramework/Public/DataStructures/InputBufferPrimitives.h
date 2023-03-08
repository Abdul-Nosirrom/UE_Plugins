// Copyright 2023 Abdulrahmen Almodaimegh. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "InputBufferPrimitives.generated.h"

/// @brief	State of a given input within the buffer window
///			- Press: Corresponds to an input in the buffer which hasn't been consumed and its first frame is in the window
///			- Hold: Corresponds to an input press that's been consumed, frame value is the most recent
///			- Released: Corresponds to an input press that's been consumed, frame value is that in which it was released
USTRUCT()
struct COREFRAMEWORK_API FBufferState 
{
	GENERATED_BODY()

	
protected:
	int8 FrameVal = -1;
	float HoldTime = 0.f;
	bool bPressInstanceConsumed = false;
	FInputActionValue AssociatedVal;

public:
	FBufferState() : FrameVal(-1), HoldTime(0), bPressInstanceConsumed(false) {}
	//FBufferState(uint8 InFrameVal, float InHoldTime, bool bConsumed) : FrameVal(InFrameVal), HoldTime(InHoldTime), bPressInstanceConsumed(bConsumed) {}

	void SetValues(int8 InFrameVal, float InHoldTime, bool bConsumed)
	{
		FrameVal = InFrameVal;
		HoldTime = InHoldTime;
		bPressInstanceConsumed = bConsumed;
	}
	
	void Reset()
	{
		FrameVal = -1;
		HoldTime = 0.f;
		bPressInstanceConsumed = false;
	};
public:
	int8 GetAssociatedFrame() const { return FrameVal; }
	float GetHoldTime() const { return HoldTime; } // NOTE: This is in seconds, we multiply w/ TICK_INTERVAL on construction
	void SetActionValue(const FInputActionValue InValue) { AssociatedVal = InValue; }
	bool IsConsumed() const { return bPressInstanceConsumed; }
	
	bool IsPress() const { return FrameVal >= 0 && HoldTime == 0 && bPressInstanceConsumed == false; }
	bool IsHold() const { return FrameVal >= 0 && HoldTime > 0 && bPressInstanceConsumed == true; }
	bool IsRelease() const { return FrameVal >= 0 && HoldTime == 0 && bPressInstanceConsumed == true; }
};

/* Wrapper for incoming input values to later be processed by FInputFrameState */
USTRUCT()
struct COREFRAMEWORK_API FRawInputValue
{
	GENERATED_BODY()
	
protected:
	UPROPERTY(Transient)
	FInputActionValue InputValue;

	UPROPERTY(Transient)
	float ElapsedTriggeredTime = 0.f;

public:
	FRawInputValue() : InputValue(FInputActionValue()), ElapsedTriggeredTime(0) {}

	explicit FRawInputValue(const FInputActionInstance& ValueInstance)
				: InputValue(ValueInstance.GetValue()), ElapsedTriggeredTime(ValueInstance.GetTriggeredTime()) {}
	
	// General accessors
	bool IsThereInput() const { return InputValue.IsNonZero(); }
	FInputActionValue GetValue() const { return InputValue; }
	EInputActionValueType GetValueType() const { return InputValue.GetValueType(); }
	bool GetBoolValue() const { return InputValue.Get<bool>(); }
	FVector2D GetVectorValue() const { return InputValue.Get<FVector2D>(); }
	float GetTriggeredTime() const { return ElapsedTriggeredTime; }
};

/// @brief	Container to hold two frame states to account for a sequence of the same action
///			NOTE: If any input is actually registered, the older frame is guaranteed to be valid but not the newer frame
///			(NewerState is only checked whenever there's a binding for Action Sequence where both actions are of the same input)
USTRUCT()
struct COREFRAMEWORK_API FBufferStateTuple
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	FBufferState OlderState;
	UPROPERTY(Transient)
	FBufferState NewerState;

	FBufferStateTuple()
	{
		OlderState = FBufferState();
		NewerState = FBufferState();
	}

	void SetFrameStateValues(bool bOlderFrame, int8 InFrameVal, float InHoldTime, bool bConsumed) 
	{
		bOlderFrame ? OlderState.SetValues(InFrameVal, InHoldTime, bConsumed) : NewerState.SetValues(InFrameVal, InHoldTime, bConsumed);
	}

	void ResetAll()
	{
		OlderState.Reset();
		NewerState.Reset();
	}

	bool IsEitherPress() const { return OlderState.IsPress() || NewerState.IsPress(); }
};

/* Struct defining the state of a given input in a frame, whether its been used, being held, and so on  */
USTRUCT()
struct COREFRAMEWORK_API FInputFrameState
{
	GENERATED_BODY()

public:
	/// @brief	Input Action ID Associated With This Object
	FName ID;
	
	/// @brief  Register the raw input action value from EIC into this frame state
	FInputActionValue Value;
	
	/// @brief	Input state value. (0) if no input, (-1) if the input was released the last frame, and time in which
	///			input has been held otherwise.
	int HoldTime{0};
	
	/// @brief	True if the input has been consumed already and invalid for further use.
	bool bUsed{false};

	/// @brief  Used to keep track of a release event being invoked so it doesnt linger in the buffer
	bool bReleaseFlagged{false};

	float DebugTime{0};
public:
	FInputFrameState() {};
	explicit FInputFrameState(const FName InputID) : ID(InputID) {};
	
	/// @brief Checks the assigned input and resolves its state (Held, Released, Axis, etc...)
	void ResolveCommand();

	/// @brief Called when the input is being held
	/// @param InValue Current value of the input in seconds
	void HoldUp(const FInputActionValue InValue);
	
	/// @brief Called when the assigned input has been released or has not been registered
	void ReleaseHold(const FInputActionValue InValue);

	/// @brief  Returns true if the current frame state has not been used and is the first of its input registered to the buffer
	///			(e.g HoldTime == 1)
	bool CanInvokePress() const;

	/// @brief  Returns true if the initial press was used, but this input has now been released (HoldTime == -1)
	bool CanInvokeRelease() const;

	/// @brief  Returns true if the initial press was used, and this input has continued to be held (HoldTime > 1)
	bool CanInvokeHold() const;

};

/* Row of an input buffer. Holds a collection of input frame states and defines the primitive of the actual buffer */
USTRUCT()
struct COREFRAMEWORK_API FBufferFrame
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
