// Fill out your copyright notice in the Description page of Project Settings.

// Copyright 2023 Abdulrahmen Almodaimegh. All Rights Reserved.

#include "InputBufferPrimitives.h"

#include "Subsystems/InputBufferSubsystem.h"


/* ~~~~~ Buffer Frame ~~~~~ */

void FBufferFrame::InitializeFrame()
{
	InputsFrameState.Empty();
	
	for (auto ID : UInputBufferSubsystem::CachedActionIDs)
	{
		FInputFrameState NewFS = FInputFrameState(ID);
		InputsFrameState.Add(ID, NewFS);
	}
}

void FBufferFrame::UpdateFrameState()
{
	for (auto FrameState : InputsFrameState)
	{
		InputsFrameState[FrameState.Key].ResolveCommand();
	}
}

void FBufferFrame::CopyFrameState(FBufferFrame& FrameState)
{
	for (auto ID : UInputBufferSubsystem::CachedActionIDs)
	{
		InputsFrameState[ID].Value = FrameState.InputsFrameState[ID].Value;
		InputsFrameState[ID].HoldTime = FrameState.InputsFrameState[ID].HoldTime;
		InputsFrameState[ID].bUsed = FrameState.InputsFrameState[ID].bUsed;
		InputsFrameState[ID].bReleaseFlagged = false;
	}
}

/* ~~~~~ Input State ~~~~~ */

void FInputFrameState::ResolveCommand()
{
	if (UInputBufferSubsystem::RawValueContainer.Contains(ID))
	{
		if (UInputBufferSubsystem::RawValueContainer[ID].IsThereInput())
		{
			HoldUp(UInputBufferSubsystem::RawValueContainer[ID].GetValue());
		}
		else
		{
			//bUsed = false; // NOTE: Let bUsed carry over from previous frames (to invoke valid Holds), it's reset when the input is released (if never consumed that carries over so its fine)
			ReleaseHold(UInputBufferSubsystem::RawValueContainer[ID].GetValue());
		}
	}
}

bool FInputFrameState::CanInvokePress() const
{
	return (HoldTime == 1 && !bUsed);
}

bool FInputFrameState::CanInvokeHold() const
{
	return (HoldTime > 1 && bUsed); 
}

bool FInputFrameState::CanInvokeRelease() const
{
	return (HoldTime == -1 && bUsed);
}


void FInputFrameState::HoldUp(const FInputActionValue InValue)
{
	Value = InValue;

	if (HoldTime < 0)
	{
		HoldTime = 1;
		bUsed = false;
	}
	else HoldTime += 1;
}

void FInputFrameState::ReleaseHold(const FInputActionValue InValue)
{
	Value = InValue;
	
	if (HoldTime > 0)
	{
		HoldTime = -1;
	}
	else
	{
		HoldTime = 0;
		bUsed = false;
	}
}