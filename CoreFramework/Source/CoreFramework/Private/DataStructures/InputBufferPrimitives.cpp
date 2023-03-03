﻿// Fill out your copyright notice in the Description page of Project Settings.

// Copyright 2023 Abdulrahmen Almodaimegh. All Rights Reserved.

#include "InputBufferPrimitives.h"

#include "Subsystems/InputBufferSubsystem.h"


/* ~~~~~ Buffer Frame ~~~~~ */

void FBufferFrame::InitializeFrame()
{
	InputsFrameState.Empty();
	
	for (auto ID : UInputBufferSubsystem::InputActionIDs)
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
	for (auto ID : UInputBufferSubsystem::InputActionIDs)
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
	
	if (UInputBufferSubsystem::RawButtonContainer.Contains(ID))
	{
		if (UInputBufferSubsystem::RawButtonContainer[ID]) HoldUp(1.f);
		else ReleaseHold();
	}
	else if (UInputBufferSubsystem::RawAxisContainer.Contains(ID))
	{
		RegisterVector(UInputBufferSubsystem::RawAxisContainer[ID]);
		if (!UInputBufferSubsystem::RawAxisContainer[ID].IsZero()) HoldUp(UInputBufferSubsystem::RawAxisContainer[ID].Length());
		else ReleaseHold();
	}
}

// TODO: The condition for HoldTime == 1 is kind of rigid but makes sense because we don't wanna use old inputs
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