// Fill out your copyright notice in the Description page of Project Settings.

// Copyright 2023 Abdulrahmen Almodaimegh. All Rights Reserved.

#include "InputBufferPrimitives.h"

#include "Subsystems/InputBufferSubsystem.h"


/* ~~~~~ Buffer Frame ~~~~~ */

void FBufferFrame::InitializeFrame(const FGameplayTagContainer& CachedActionIDs)
{
	InputsFrameState.Empty();
	
	for (auto ID : CachedActionIDs)
	{
		FInputFrameState NewFS = FInputFrameState(ID);
		InputsFrameState.Add(ID, NewFS);
	}
}

void FBufferFrame::UpdateFrameState(const TMap<FGameplayTag, FRawInputValue>& RawValueContainer)
{
	for (auto FrameState : InputsFrameState)
	{
		InputsFrameState[FrameState.Key].ResolveCommand(RawValueContainer);
	}
}

void FBufferFrame::CopyFrameState(const FGameplayTagContainer& CachedActionIDs, FBufferFrame& FrameState)
{
	for (auto ID : CachedActionIDs)
	{
		InputsFrameState[ID].Value = FrameState.InputsFrameState[ID].Value;
		InputsFrameState[ID].HoldTime = FrameState.InputsFrameState[ID].HoldTime;
		InputsFrameState[ID].bUsed = FrameState.InputsFrameState[ID].bUsed;
		InputsFrameState[ID].bReleaseFlagged = false;
	}
}

/* ~~~~~ Input State ~~~~~ */

void FInputFrameState::ResolveCommand(const TMap<FGameplayTag, FRawInputValue>& RawValueContainer)
{
	if (RawValueContainer.Contains(ID))
	{
		if (RawValueContainer[ID].IsThereInput())
		{
			HoldUp(RawValueContainer[ID].GetValue());
		}
		else
		{
			//bUsed = false; // NOTE: Let bUsed carry over from previous frames (to invoke valid Holds), it's reset when the input is released (if never consumed that carries over so its fine)
			ReleaseHold(RawValueContainer[ID].GetValue());
		}
	}
}

bool FInputFrameState::CanInvokePress() const
{
	return (HoldTime == 1 && !bUsed);
}

bool FInputFrameState::CanInvokeHold() const
{
	return (HoldTime > 1);// && bUsed); 
}

bool FInputFrameState::CanInvokeRelease() const
{
	return (HoldTime < 0);// && bUsed);
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
		HoldTime = -HoldTime;
	}
	else
	{
		HoldTime = 0;
		bUsed = false;
	}
}