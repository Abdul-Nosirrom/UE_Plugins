// Fill out your copyright notice in the Description page of Project Settings.

// Copyright 2023 Abdulrahmen Almodaimegh. All Rights Reserved.

#include "InputBuffer.h"

#pragma region Profiling Groups

DECLARE_CYCLE_STAT(TEXT("Update Buffer"), STAT_UpdateBuffer, STATGROUP_InputBuffer)
DECLARE_CYCLE_STAT(TEXT("Initialize Frame"), STAT_InitializeFrame, STATGROUP_InputBuffer)
DECLARE_CYCLE_STAT(TEXT("Resolve Frame"), STAT_ResolveFrame, STATGROUP_InputBuffer)

#pragma endregion Profiling Groups

#pragma region Input Buffer Core

FInputBuffer::FInputBuffer(uint8 GivenBufferSize) : BufferSize(GivenBufferSize), InputBuffer(GivenBufferSize)
{
	/* Initialize Buffer Data Structure */
	for (int i = 0; i < GivenBufferSize; i++)
	{
		FBufferFrame NewFrame = FBufferFrame();
		NewFrame.InitializeFrame();
		InputBuffer.PushBack(NewFrame);
	}

	/* Initialize Button State Container */
	for (auto ID : BufferUtility::InputIDs)
	{
		ButtonOldestValidFrame.Add(ID, -1);
	}
}

void FInputBuffer::UpdateBuffer()
{
	SCOPE_CYCLE_COUNTER(STAT_UpdateBuffer)
	
	/* Each frame, push a new list of inputs and their states to the front */
	FBufferFrame FrontFrame = InputBuffer.Front();
	FBufferFrame NewFrame = FBufferFrame();
	NewFrame.InitializeFrame();
	NewFrame.CopyFrameState(FrontFrame);
	NewFrame.UpdateFrameState();
	
	/* Push newly updated frame to the buffer, and delete oldest frame from heap */
	InputBuffer.PushFront(NewFrame);

	/* Store the frame value in which each input can be used */
	for (auto InputID : BufferUtility::InputIDs)
	{
		ButtonOldestValidFrame[InputID] = -1;
		for (int frame = 0; frame < BufferSize; frame++)
		{
			/* Set the button state to the frame it can be used */
			if (InputBuffer[frame].InputsFrameState[InputID].CanExecute()) ButtonOldestValidFrame[InputID] = frame;
		}
	}
}

bool FInputBuffer::UseInput(const FName InputID)
{
	if (ButtonOldestValidFrame[InputID] < 0) return false;
	if (InputBuffer[ButtonOldestValidFrame[InputID]].InputsFrameState[InputID].CanExecute())
	{
		InputBuffer[ButtonOldestValidFrame[InputID]].InputsFrameState[InputID].bUsed = true;
		ButtonOldestValidFrame[InputID] = -1;
		return true;
	}
	return false;
}

#pragma endregion Input Buffer Core

#pragma region Frame States

/* ~~~~~ Buffer Frame ~~~~~ */

void FBufferFrame::InitializeFrame()
{
	SCOPE_CYCLE_COUNTER(STAT_InitializeFrame)
	
	InputsFrameState.Empty();

	for (auto ID : BufferUtility::InputIDs)
	{
		FInputFrameState NewFS = FInputFrameState(ID);
		InputsFrameState.Add(ID, NewFS);
	}
}

void FBufferFrame::UpdateFrameState()
{
	SCOPE_CYCLE_COUNTER(STAT_ResolveFrame)
	
	for (auto FrameState : InputsFrameState)
	{
		InputsFrameState[FrameState.Key].ResolveCommand();
	}
}

void FBufferFrame::CopyFrameState(FBufferFrame& FrameState)
{
	for (auto ID : BufferUtility::InputIDs)
	{
		InputsFrameState[ID].Value = FrameState.InputsFrameState[ID].Value;
		InputsFrameState[ID].HoldTime = FrameState.InputsFrameState[ID].HoldTime;
		InputsFrameState[ID].bUsed = FrameState.InputsFrameState[ID].bUsed;
	}
}

/* ~~~~~ Input State ~~~~~ */

void FInputFrameState::ResolveCommand()
{
	bUsed = false;

	if (BufferUtility::RawButtonContainer.Contains(ID))
	{
		if (BufferUtility::RawButtonContainer[ID]) HoldUp(1.f);
		else ReleaseHold();
	}
	else if (BufferUtility::RawAxisContainer.Contains(ID))
	{
		// TODO: This is wrong, maybe we can split it up
		if (!BufferUtility::RawAxisContainer[ID].IsZero()) HoldUp(BufferUtility::RawAxisContainer[ID].Length());
		else ReleaseHold();
	}
}

// TODO: The condition for HoldTime == 1 is kind of rigid
bool FInputFrameState::CanExecute() const
{
	return (HoldTime == 1 && !bUsed);
}

void FInputFrameState::HoldUp(float Val)
{
	Value = Val;

	if (HoldTime < 0) HoldTime = 1;
	else HoldTime += 1;
}

void FInputFrameState::ReleaseHold()
{
	Value = 0;
	
	if (HoldTime > 0)
	{
		HoldTime = -1;
		bUsed = false;
	}
	else HoldTime = 0;
}


#pragma endregion Frame States