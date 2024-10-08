﻿// Fill out your copyright notice in the Description page of Project Settings.


#include "Subsystems/InputBufferSubsystem.h"
#include "Debug/IB_LOG.h"
#include "Helpers/CoreFrameworkStatics.h"
#include "StaticLibraries/CoreMathLibrary.h"
#include "CoreFrameworkTags.h"

DECLARE_CYCLE_STAT(TEXT("Update Buffer"), STAT_UpdateBuffer, STATGROUP_InputBuffer)
DECLARE_CYCLE_STAT(TEXT("Eval Events"), STAT_EvalEvents, STATGROUP_InputBuffer)

DEFINE_LOG_CATEGORY(LogInputBuffer)


void UInputBufferSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	bInitialized = false;
	bInputDisabled = false;
	ElapsedTime = 0;
	ButtonInputValidFrame = TMap<FGameplayTag, FBufferStateTuple>();

	// Initialize
	CachedActionIDs = FGameplayTagContainer();
	CachedDirectionalActionIDs = FGameplayTagContainer();
	RawValueContainer = TMap<FGameplayTag, FRawInputValue>();

	// Set input buffer settings from project
	BUFFER_SIZE = UCoreFrameworkStatics::GetInputBufferFullFrameWindow();
	BUTTON_BUFFER_SIZE = UCoreFrameworkStatics::GetInputBufferButtonFrameWindow();
	TICK_INTERVAL = UCoreFrameworkStatics::GetInputBufferTickInterval();

	IB_FLog(Error, "Input Buffer Initialized")
}

void UInputBufferSubsystem::Deinitialize()
{
	Super::Deinitialize();
	ButtonInputValidFrame.Empty(); // Deallocs, Reset() does not dealloc
	IB_FLog(Error, "Input Buffer Deinitialized")
}

void UInputBufferSubsystem::PlayerControllerChanged(APlayerController* NewPlayerController)
{
	Super::PlayerControllerChanged(NewPlayerController);

	//Unbind all previously bound events, great place to do cleanup!
	ActionDelegates.Empty();
	ActionSeqDelegates.Empty();
	DirectionalDelegates.Empty();
	DirectionAndActionDelegates.Empty();
}

void UInputBufferSubsystem::Tick(float DeltaTime)
{
	if (LastFrameNumberWeTicked == GFrameCounter) return;
	
	// Ensure we've been initialized, otherwise don't update the buffer
	if (!bInitialized)
	{
		return;
	}

	// Increment our timer and update the buffer if we've met our tick interval
	ElapsedTime += DeltaTime;
	if (ElapsedTime >= TICK_INTERVAL)
	{
		UpdateBuffer();
		ElapsedTime = 0.f;
	}

	// Evaluate Events (More frequently than buffer updates)
	EvaluateEvents();
	LastFrameNumberWeTicked = GFrameCounter;
}



void UInputBufferSubsystem::AddMappingContext(UInputBufferMap* TargetInputMap, UEnhancedInputComponent* InputComponent)
{
	InputMap = TargetInputMap;
	InputMap->GenerateInputActions(); // We generate input actions once just so we dont have to go through GetMappings()
	CachedActionIDs = InputMap->GetActionIDs();
	CachedDirectionalActionIDs = InputMap->GetDirectionalIDs();
	InputComponent->bBlockInput = false;
	InitializeInputMapping(InputComponent);

	IB_FLog(Log, "Input Buffer Initialized")
}

void UInputBufferSubsystem::InitializeInputMapping(UEnhancedInputComponent* InputComponent)
{
	/* Empty our tracking data, we're gonna override it with current InputMap */
	RawValueContainer.Empty();
	
	/* If no input map has been assigned, we return (this is only called through AddMappingContext so its guaranteed to work?) */
	if (!InputMap) return;

	/* Ensure no overlap in tags between buttons & directional input */
	ensureMsgf(!CachedActionIDs.HasAny(CachedDirectionalActionIDs), TEXT("INPUT BUFFER ERROR: Duplicate tags found between button inputs & directional inputs!"));

	/* Add input map to the EnhancedInput subsystem*/
	UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer());
	Subsystem->ClearAllMappings();
	Subsystem->AddMappingContext(InputMap->InputActionMap, 0);
	
	auto InputActionCache = InputMap->GetInputActions();
	
	/* Generate Action Bindings */
	for (auto Action : InputActionCache)
	{
		RawValueContainer.Add(Action->GetID(), FRawInputValue());

		InputComponent->BindAction(Action, ETriggerEvent::Triggered, this, &UInputBufferSubsystem::TriggerInput, Action->GetID());
		InputComponent->BindAction(Action, ETriggerEvent::Completed, this, &UInputBufferSubsystem::CompleteInput, Action->GetID());
	}

	InitializeInputBufferData();
}

void UInputBufferSubsystem::InitializeInputBufferData()
{
	bInitialized = true;
	InputBuffer = TBufferContainer<FBufferFrame>(BUFFER_SIZE);
	
	/* Populate the buffer */
	for (int i = 0; i < BUFFER_SIZE; i++)
	{
		FBufferFrame NewFrame = FBufferFrame();
		NewFrame.InitializeFrame(CachedActionIDs);
		InputBuffer.PushBack(NewFrame);
	}
	
	/* Setup "tracking" data */
	ButtonInputValidFrame.Empty();
	for (auto ID : InputMap->GetActionIDs())
	{
		IB_FLog(Display, "[%s] - Action Added To Buffer", *ID.ToString())
		ButtonInputValidFrame.Add(ID, FBufferStateTuple());
	}

	DirectionalInputValidFrame.Empty();
	for (auto ID : InputMap->GetDirectionalIDs())
	{
		IB_FLog(Display, "[%s] - Directional Added To Buffer", *ID.ToString())
		DirectionalInputValidFrame.Add(ID, -1);
	}
}

//BEGIN Input Registration Events
void UInputBufferSubsystem::TriggerInput(const FInputActionInstance& ActionInstance, const FGameplayTag InputID)
{
	RawValueContainer[InputID] = FRawInputValue(ActionInstance);
}

void UInputBufferSubsystem::CompleteInput(const FInputActionInstance& ActionInstance, const FGameplayTag InputID)
{
	RawValueContainer[InputID] = FRawInputValue(ActionInstance);
}
//END Input Registration Events

void UInputBufferSubsystem::UpdateBuffer()
{
	SCOPE_CYCLE_COUNTER(STAT_UpdateBuffer)
	
	/* Each frame, push a new list of inputs and their states to the front */
	FBufferFrame FrontFrame = InputBuffer.Front();
	FBufferFrame NewFrame = FBufferFrame();
	NewFrame.InitializeFrame(CachedActionIDs);
	NewFrame.CopyFrameState(CachedActionIDs, FrontFrame);
	NewFrame.UpdateFrameState(RawValueContainer);
	
	/* Push newly updated frame to the buffer */
	InputBuffer.PushFront(NewFrame);
	
	/* Store the frame value in which each action input can be used */
	for (const auto InputID : InputMap->GetActionIDs())
	{
		ButtonInputValidFrame[InputID].ResetAll();
		bool bOnOlderState = true;

		// We fill out the "Old" input buffer state until we encounter a second valid input, then copy the old into the new and fill out the old
		/* Loop through all buffer frames in order of newest to oldest */
		for (int frame = BUTTON_BUFFER_SIZE - 1; frame >= 0; frame--)
		{
			auto FrameState = InputBuffer[frame].InputsFrameState[InputID];

			/* Evaluate the frame state of the given input */
			if (FrameState.CanInvokePress())
			{
				ButtonInputValidFrame[InputID].SetFrameStateValues(bOnOlderState, frame, 0, false);
				bOnOlderState = false;
			}
			if (FrameState.CanInvokeHold())
			{
				ButtonInputValidFrame[InputID].SetFrameStateValues(bOnOlderState, frame, RawValueContainer[InputID].GetTriggeredTime(), true); 
			}
			if (FrameState.CanInvokeRelease())
			{
				ButtonInputValidFrame[InputID].SetFrameStateValues(bOnOlderState, frame, -RawValueContainer[InputID].GetTriggeredTime(), true);
			}
		}
	}

	/* Store the frame value in which each directional input can be used */
	if (!InputMap->DirectionalActionMap) return;
	
	for (const auto DirectionalInput : InputMap->DirectionalActionMap->GetMappings())
	{
		const FGameplayTag InputID = DirectionalInput->GetID();
		
		// Input for directional input evaluation is stored in UMotionMappingContext
		const FGameplayTag DirectionInputAxisID = InputMap->DirectionalActionMap->GetDirectionalActionID();
		
		DirectionalInputValidFrame[InputID] = -1;
		DirectionalInput->Reset(); // Resets transient values that are to be checked during the buffer window
		
		for (int frame = BUFFER_SIZE-1; frame >= 0; frame--) // Iterating From Oldest To Newest (To Check Sequence Order)
		{
			const FVector2D DirectionInputVector = InputBuffer[frame].InputsFrameState[DirectionInputAxisID].Value.Get<FVector2D>();
			FVector ProcessedInputVector, PlayerForward, PlayerRight;
			ProcessDirectionVector(DirectionInputVector, ProcessedInputVector, PlayerForward, PlayerRight);
			
			if (DirectionalInput->CheckMotionDirection(DirectionInputVector, ProcessedInputVector, PlayerForward, PlayerRight))
			{
				DirectionalInputValidFrame[InputID] = frame; 
				break; // Break out of inner loop otherwise the frame value won't represent the frame in which the input is registered and will always default to 0 if directional was satisfied
			}
		}
	}
}

void UInputBufferSubsystem::EvaluateEvents()
{
	SCOPE_CYCLE_COUNTER(STAT_EvalEvents)

	/* Check if input is even enabled */
	if (bInputDisabled) return;

	// BUG: Somewhere here I'm causing an esnure fail by deleting an element in an array during the loop
	
	// NOTE: Consuming has possible overlap w/ ActionEvent if SEQUENCE_ButtonFirst
	/* Evaluate Directional + Action Events */
	for (auto DirActSeqBinding = DirectionAndActionDelegates.CreateIterator(); DirActSeqBinding; ++DirActSeqBinding)
	{
		// If no binding for the delegate, skip and delete same if the value is invalid
		if (!DirActSeqBinding->Key.Delegate.IsBound())
		{
			IB_FLog(Error, "Delegate In Action Bindings Not Bound [%s]", *DirActSeqBinding->Key.Delegate.GetFunctionName().ToString());
			DirActSeqBinding.RemoveCurrent();
			continue;
		}
		
		if (ButtonInputValidFrame.Contains(DirActSeqBinding->Value.InputAction) && DirectionalInputValidFrame.Contains(DirActSeqBinding->Value.DirectionalAction))
		{
			const auto ActionState = ButtonInputValidFrame[DirActSeqBinding->Value.InputAction].OlderState;
			const auto DirectionalFrame = DirectionalInputValidFrame[DirActSeqBinding->Value.DirectionalAction];
			
			if (ActionState.IsPress() && DirectionalFrame > 0)
			{
				// Skip if order matters and first action is more recent than second
				bool bInvokeEvent = true;
				switch (DirActSeqBinding->Value.SequenceOrder)
				{
					case SEQUENCE_None:
						break;
					case SEQUENCE_ButtonFirst:
						bInvokeEvent = ActionState.GetAssociatedFrame() >= DirectionalFrame;
						break;
					case SEQUENCE_DirectionalFirst:
						bInvokeEvent = ActionState.GetAssociatedFrame() <= DirectionalFrame;
						break;
					default:;
				}
		
				// Otherwise invoke event and maybe consume input
				if (bInvokeEvent && !IsInputConsumed(DirActSeqBinding->Value.InputAction))
				{
					auto Value = InputBuffer[ButtonInputValidFrame[DirActSeqBinding->Value.InputAction].OlderState.GetAssociatedFrame()].InputsFrameState[DirActSeqBinding->Value.InputAction].Value;
					DirActSeqBinding->Key.Delegate.Execute(Value, ButtonInputValidFrame[DirActSeqBinding->Value.InputAction].OlderState.GetHoldTime());
					if (DirActSeqBinding->Value.bAutoConsume)
					{
						ConsumeInput(DirActSeqBinding->Value.InputAction);
						ConsumeInput(DirActSeqBinding->Value.DirectionalAction);
					}
				}
			}
		}
		else
		{
			IB_FLog(Error, "One Or Both Bound Actions Don't Exit In Buffer [1 - %s] & [2 - %s]", *DirActSeqBinding->Value.InputAction.ToString(), *DirActSeqBinding->Value.DirectionalAction.ToString())
		}
	}


	/* Evaluate Button Seq Events */
	for (auto ActionSeqBinding = ActionSeqDelegates.CreateIterator(); ActionSeqBinding; ++ActionSeqBinding)
	{
		// If no binding for the delegate, skip and delete
		if (!ActionSeqBinding->Key.Delegate.IsBound())
		{
			IB_FLog(Error, "Delegate In Action Bindings Not Bound [%s]", *ActionSeqBinding->Key.Delegate.GetFunctionName().ToString());
			ActionSeqBinding.RemoveCurrent();
			continue;
		}
		
		if (ButtonInputValidFrame.Contains(ActionSeqBinding->Value.FirstAction) && ButtonInputValidFrame.Contains(ActionSeqBinding->Value.SecondAction))
		{
			// NOTE: Order doesn't matter here, but if we go with the tuple approach, we should guarantee first element is "older" than the second
			const bool bRepeatedAction = (ActionSeqBinding->Value.FirstAction == (ActionSeqBinding->Value.SecondAction));

			const auto FirstAction = ButtonInputValidFrame[ActionSeqBinding->Value.FirstAction].OlderState;
			const auto SecondAction = bRepeatedAction ? ButtonInputValidFrame[ActionSeqBinding->Value.SecondAction].NewerState : ButtonInputValidFrame[ActionSeqBinding->Value.SecondAction].OlderState;

			bool bInputStatesValid = false;
			
			if (ActionSeqBinding.Value().bFirstInputIsHold) // Check if first input is a hold and second input is a press that hasn't been consumed
			{
				ensureMsgf(!bRepeatedAction, TEXT("INPUT BUFFER BINDING: bRepated action was true during event evaluation for action sequence that treats first input as hold!"));
				bInputStatesValid = FirstAction.IsHold() && SecondAction.IsPress() && !IsInputConsumed(ActionSeqBinding->Value.SecondAction, false);
			}
			else // Check both are pressed and both havent been consumed. If order matters, check execution order
			{
				bInputStatesValid = FirstAction.IsPress() && SecondAction.IsPress() && !(ActionSeqBinding->Value.bButtonOrderMatters && (FirstAction.GetAssociatedFrame() < SecondAction.GetAssociatedFrame()));
				bInputStatesValid = bInputStatesValid && (!IsInputConsumed(ActionSeqBinding->Value.FirstAction) && !IsInputConsumed(ActionSeqBinding->Value.SecondAction, bRepeatedAction));
			}

			if (bInputStatesValid)
			{
				const FInputActionValue& FirstValue = InputBuffer[FirstAction.GetAssociatedFrame()].InputsFrameState[ActionSeqBinding->Value.FirstAction].Value;
				const FInputActionValue& SecondValue = InputBuffer[FirstAction.GetAssociatedFrame()].InputsFrameState[ActionSeqBinding->Value.FirstAction].Value;

				ActionSeqBinding->Key.Delegate.Execute(FirstValue, SecondValue); 
				if (ActionSeqBinding->Value.bAutoConsume)
				{
					if (!ActionSeqBinding->Value.bFirstInputIsHold) // Only consume first action if it is not a hold
						ConsumeInput(ActionSeqBinding->Value.FirstAction);
					ConsumeInput(ActionSeqBinding->Value.SecondAction, bRepeatedAction);
				}
			}
		}
		else
		{
			IB_FLog(Error, "One Or Both Bound Actions Don't Exist In Buffer [1 - %s] & [2 - %s]", *ActionSeqBinding->Value.FirstAction.ToString(), *ActionSeqBinding->Value.SecondAction.ToString())
		}
	}
	
	/* Evaluate Directional Events */
	for (auto DirectionalBindings = DirectionalDelegates.CreateIterator(); DirectionalBindings; ++DirectionalBindings)
	{
		// If no binding for the delegate, skip and delete
		if (!DirectionalBindings->Key.Delegate.IsBound())
		{
			IB_FLog(Error, "Delegate In Action Bindings Not Bound [%s]", *DirectionalBindings->Key.Delegate.GetFunctionName().ToString());
			DirectionalBindings.RemoveCurrent();
			continue;
		}
		
		if (DirectionalInputValidFrame.Contains(DirectionalBindings->Value.DirectionalAction))
		{
			if (DirectionalInputValidFrame[DirectionalBindings->Value.DirectionalAction] >= 0)
			{
				DirectionalBindings->Key.Delegate.Execute();
				if (DirectionalBindings->Value.bAutoConsume)
				{
					ConsumeInput(DirectionalBindings->Value.DirectionalAction);
				}
			}
		}
		else
		{
			IB_FLog(Error, "Bound Directional Action Doesn't Exist In Buffer [%s]", *DirectionalBindings->Value.DirectionalAction.ToString())
		}
	}

	/* Evaluate Action Events */
	for (auto ActionBindings = ActionDelegates.CreateIterator(); ActionBindings; ++ActionBindings)
	{
		// If no binding for the delegate, skip and delete
		if (!ActionBindings->Key.Delegate.IsBound())
		{
			IB_FLog(Error, "Delegate In Action Bindings Not Bound [%s]", *ActionBindings->Key.Delegate.GetFunctionName().ToString());
			ActionBindings.RemoveCurrent();
			continue;
		}
		
		if (ButtonInputValidFrame.Contains(ActionBindings->Value.InputAction))
		{
			switch (ActionBindings->Value.TriggerType)
			{
				case TRIGGER_Press:
					if (ButtonInputValidFrame[ActionBindings->Value.InputAction].OlderState.IsPress() && !IsInputConsumed(ActionBindings->Value.InputAction))
					{
						auto Value = InputBuffer[ButtonInputValidFrame[ActionBindings->Value.InputAction].OlderState.GetAssociatedFrame()].InputsFrameState[ActionBindings->Value.InputAction].Value;
						ActionBindings->Key.Delegate.Execute(Value, 0);
						if (ActionBindings->Value.bAutoConsume) // Only press knows what "consume" is
						{
							ConsumeInput(ActionBindings->Value.InputAction);
						}
					}
					break;
				case TRIGGER_Hold:
					if (ButtonInputValidFrame[ActionBindings->Value.InputAction].NewerState.IsHold()) // Newer input hold takes presedence as prev input was released and its hold invalidated
					{
						auto Value = InputBuffer[ButtonInputValidFrame[ActionBindings->Value.InputAction].NewerState.GetAssociatedFrame()].InputsFrameState[ActionBindings->Value.InputAction].Value;
						ActionBindings->Key.Delegate.Execute(Value, ButtonInputValidFrame[ActionBindings->Value.InputAction].NewerState.GetHoldTime());
					}
					else if (ButtonInputValidFrame[ActionBindings->Value.InputAction].OlderState.IsHold())
					{
						auto Value = InputBuffer[ButtonInputValidFrame[ActionBindings->Value.InputAction].OlderState.GetAssociatedFrame()].InputsFrameState[ActionBindings->Value.InputAction].Value;
						ActionBindings->Key.Delegate.Execute(Value, ButtonInputValidFrame[ActionBindings->Value.InputAction].OlderState.GetHoldTime());
					}
					break;
				case TRIGGER_Release:
					if (ButtonInputValidFrame[ActionBindings->Value.InputAction].NewerState.IsRelease())
					{
						if (InputBuffer[ButtonInputValidFrame[ActionBindings->Value.InputAction].NewerState.GetAssociatedFrame()].InputsFrameState[ActionBindings->Value.InputAction].bReleaseFlagged) continue;
						ActionBindings->Key.Delegate.Execute(FInputActionValue(), -ButtonInputValidFrame[ActionBindings->Value.InputAction].NewerState.GetHoldTime());
						InputBuffer[ButtonInputValidFrame[ActionBindings->Value.InputAction].NewerState.GetAssociatedFrame()].InputsFrameState[ActionBindings->Value.InputAction].bReleaseFlagged = true;
					}
					else if (ButtonInputValidFrame[ActionBindings->Value.InputAction].OlderState.IsRelease())
					{
						if (InputBuffer[ButtonInputValidFrame[ActionBindings->Value.InputAction].OlderState.GetAssociatedFrame()].InputsFrameState[ActionBindings->Value.InputAction].bReleaseFlagged) continue;
						ActionBindings->Key.Delegate.Execute(FInputActionValue(), -ButtonInputValidFrame[ActionBindings->Value.InputAction].OlderState.GetHoldTime());
						InputBuffer[ButtonInputValidFrame[ActionBindings->Value.InputAction].OlderState.GetAssociatedFrame()].InputsFrameState[ActionBindings->Value.InputAction].bReleaseFlagged = true;
					}
					break;
				default:;
			}
		}
		else
		{
			IB_FLog(Error, "Bound Action Doesn't Exist In Buffer [%s]", *ActionBindings->Value.InputAction.ToString())
		}
	}
}

// Can only really consume press events
bool UInputBufferSubsystem::ConsumeInput(const FGameplayTag& InputID, bool bConsumeNewer)
{
	if (!ButtonInputValidFrame.Contains(InputID) && !DirectionalInputValidFrame.Contains(InputID))
	{
		IB_FLog(Error, "%s - Input Action Registered But Not Collected In Buffer", *InputID.ToString())
		return false;
	}

	if (InputID.MatchesTag(TAG_Input_Button) || InputID.MatchesTag(TAG_Input_Axis))
	{
		// NOTE: We reset the states here because ButtonInputValidFrame won't be updated until the next buffer update (fixed tick interval), but EvalEvents has no fixed interval so we wanna avoid invoking the same event multiple times within a buffer-tick
		if (ButtonInputValidFrame[InputID].OlderState.IsPress() && !bConsumeNewer) // Check older input first
		{
			InputBuffer[ButtonInputValidFrame[InputID].OlderState.GetAssociatedFrame()].InputsFrameState[InputID].bUsed = true;
			PropagateConsume(InputID, ButtonInputValidFrame[InputID].OlderState.GetAssociatedFrame());
			ButtonInputValidFrame[InputID].OlderState.Reset();
			return true;
		}
		if (ButtonInputValidFrame[InputID].NewerState.IsPress()) // Now check newer input if older wasn't valid
		{
			InputBuffer[ButtonInputValidFrame[InputID].NewerState.GetAssociatedFrame()].InputsFrameState[InputID].bUsed = true;
			PropagateConsume(InputID, ButtonInputValidFrame[InputID].NewerState.GetAssociatedFrame());
			ButtonInputValidFrame[InputID].NewerState.Reset();
			return true;
		}
	}
	else if (InputID.MatchesTag(TAG_Input_Directional))
	{
		if (DirectionalInputValidFrame[InputID] < 0) return false;
		if (InputBuffer[DirectionalInputValidFrame[InputID]].InputsFrameState.Contains(InputID))
		{
			for (int frame = BUFFER_SIZE; frame >= DirectionalInputValidFrame[InputID]; frame--)
			{
				InputBuffer[frame].InputsFrameState[InputID].Value = 0;
			}
			DirectionalInputValidFrame[InputID] = -1;
			return true;
		}
	}
	return false;
}

bool UInputBufferSubsystem::IsConsumedInputHeld(const FGameplayTag& InputID) const
{
	if (!ButtonInputValidFrame.Contains(InputID))
	{
		IB_FLog(Error, "%s - Input Action Registered But Not Collected In Buffer", *InputID.ToString())
		return false;
	}

	const auto InputState = InputBuffer.Front().InputsFrameState[InputID];

	return InputState.bUsed && InputState.HoldTime > 0;
}

float UInputBufferSubsystem::GetTimeInputHeld(const FGameplayTag& InputID) const
{
	if (!ButtonInputValidFrame.Contains(InputID))
	{
		IB_FLog(Error, "%s - Input Action Registered But Not Collected In Buffer", *InputID.ToString())
		return false;
	}

	const auto InputState = InputBuffer.Front().InputsFrameState[InputID];

	return InputState.HoldTime * TICK_INTERVAL;
}

bool UInputBufferSubsystem::CanPressInput(const FGameplayTag& InputID)
{
	if (!ButtonInputValidFrame.Contains(InputID))
	{
		IB_FLog(Error, "%s - Input Action Registered But Not Collected In Buffer", *InputID.ToString())
		return false;
	}

	return ButtonInputValidFrame[InputID].IsEitherPress();
}

bool UInputBufferSubsystem::IsInputConsumed(const FGameplayTag& InputID, bool bCheckNewer)
{
	if (!ButtonInputValidFrame.Contains(InputID))
	{
		IB_FLog(Error, "%s - Input Action Registered But Not Collected In Buffer", *InputID.ToString())
		return false;
	}
	
	if (!bCheckNewer) // Check older input first
	{
		return InputBuffer[ButtonInputValidFrame[InputID].OlderState.GetAssociatedFrame()].InputsFrameState[InputID].bUsed;
	}
	if (ButtonInputValidFrame[InputID].NewerState.IsPress()) // Now check newer input if older wasn't valid
	{
		return InputBuffer[ButtonInputValidFrame[InputID].NewerState.GetAssociatedFrame()].InputsFrameState[InputID].bUsed;
	}
	
	return false;
}

void UInputBufferSubsystem::PropagateConsume(const FGameplayTag& InputID, const uint8 FromFrame)
{
	for (int frame = FromFrame; frame >= 0; frame--)
	{
		if (InputBuffer[frame].InputsFrameState[InputID].HoldTime == -1) return;
		InputBuffer[frame].InputsFrameState[InputID].bUsed = true;
	}
}


void UInputBufferSubsystem::ProcessDirectionVector(const FVector2D& DirectionInputVector, FVector& ProcessedInputVector, FVector& OutPlayerForward, FVector& OutPlayerRight) const
{
	const auto Controller = GetLocalPlayer<ULocalPlayer>()->GetPlayerController(GetWorld());
	
	if (Controller != nullptr)
	{
		//Controller->GetPawn()->GetLastMovementInputVector();
		const FQuat InputRotation = UCoreMathLibrary::ComputeRelativeInputVector(Controller->GetPawn());
		const FVector ForwardDirection = InputRotation.GetForwardVector();
		const FVector RightDirection = InputRotation.GetRightVector();

		ProcessedInputVector = ForwardDirection * DirectionInputVector.Y + RightDirection * DirectionInputVector.X;
		if (const auto Owner = Controller->GetPawn())
		{
			OutPlayerForward = Owner->GetActorForwardVector();
			OutPlayerRight = Owner->GetActorRightVector();
		}
	}
}


// Will color code soon, white for nothing, red for Hold, Green for CanPress, purple for Released
void UInputBufferSubsystem::DisplayDebug(UCanvas* Canvas, const FDebugDisplayInfo& DebugDisplay, float& YL, float& YPos)
{
	FDisplayDebugManager& DisplayDebugManager = Canvas->DisplayDebugManager;

	float XOffset = DisplayDebugManager.GetMaxCharHeight();
	
	DisplayDebugManager.SetDrawColor(FColor::Yellow);
	
	DisplayDebugManager.DrawString(TEXT("-----INPUT BUFFER DEBUG-----"));
	
	/* Draw Buffer Oldest Frame Vals*/
	FString InputFrames = "";
	for (auto ID : InputMap->GetActionIDs())
	{
		InputFrames += FString::FromInt(ButtonInputValidFrame[ID].NewerState.IsHold() ? ButtonInputValidFrame[ID].NewerState.GetHoldTime() : ButtonInputValidFrame[ID].OlderState.GetHoldTime());
		InputFrames += "            ";
	}
	DisplayDebugManager.SetDrawColor(FColor::Red);
	DisplayDebugManager.DrawString(InputFrames);
	
	DisplayDebugManager.SetDrawColor(FColor::Yellow);
	/* Write the input names on the first row */
	float YPosD = DisplayDebugManager.GetYPos();
	FString InputNames = "";
	for (auto ID : InputMap->GetActionIDs())
	{
		//FText InputName = Mapping.Action->ActionDescription;
		InputNames += ID.ToString();
		InputNames += "    "; // Spacing
	}
	DisplayDebugManager.DrawString(InputNames);

	/* Draw Total States */
	FString ButtonStates = "";
	DisplayDebugManager.SetDrawColor(FColor::Red);
	for (auto State : ButtonInputValidFrame)
	{
		ButtonStates += "New Value State";
		ButtonStates += "            ";
	}
	DisplayDebugManager.DrawString(ButtonStates);
	YPosD = DisplayDebugManager.GetYPos();
	
	DisplayDebugManager.SetDrawColor(FColor::White);
	ButtonStates = "";
	int j = 0;
	for (auto State : ButtonInputValidFrame)
	{
		DisplayDebugManager.SetYPos(YPosD);

		XOffset += j * 12.f * DisplayDebugManager.GetMaxCharHeight() * 0.75f;
		
		ButtonStates = "(Consumed) " + FString::FromInt(State.Value.NewerState.IsConsumed());
		DisplayDebugManager.DrawString(ButtonStates, XOffset);
		ButtonStates = "(Frame) " + FString::FromInt(State.Value.NewerState.GetAssociatedFrame());
		DisplayDebugManager.DrawString(ButtonStates, XOffset);
		ButtonStates = "(HoldTime) " + FString::FromInt(State.Value.NewerState.GetHoldTime());
		DisplayDebugManager.DrawString(ButtonStates, XOffset);
		j++;
	}

	XOffset = DisplayDebugManager.GetMaxCharHeight();
	ButtonStates = "";
	DisplayDebugManager.SetDrawColor(FColor::Red);
	for (auto State : ButtonInputValidFrame)
	{
		ButtonStates += "Old Value State";
		ButtonStates += "            ";
	}
	DisplayDebugManager.DrawString(ButtonStates);
	YPosD = DisplayDebugManager.GetYPos();
	
	DisplayDebugManager.SetDrawColor(FColor::White);
	ButtonStates = "";
	j = 0;
	for (auto State : ButtonInputValidFrame)
	{
		DisplayDebugManager.SetYPos(YPosD);

		XOffset += j * 12.f * DisplayDebugManager.GetMaxCharHeight() * 0.75f;
		
		ButtonStates = "(Consumed) " + FString::FromInt(State.Value.OlderState.IsConsumed());
		DisplayDebugManager.DrawString(ButtonStates, XOffset);
		ButtonStates = "(Frame) " + FString::FromInt(State.Value.OlderState.GetAssociatedFrame());
		DisplayDebugManager.DrawString(ButtonStates, XOffset);
		ButtonStates = "(HoldTime) " + FString::FromInt(State.Value.OlderState.GetHoldTime());
		DisplayDebugManager.DrawString(ButtonStates, XOffset);
		j++;
	}

	XOffset = DisplayDebugManager.GetMaxCharHeight();
	
	DisplayDebugManager.SetDrawColor(FColor::White);

	XOffset *= 0.75 * InputNames.Len();
	
	/* Next we iterate through each row of the input buffer, drawing it to the screen */
	for (int i = 0; i < BUFFER_SIZE; i++)
	{
		if (i >= BUTTON_BUFFER_SIZE) DisplayDebugManager.SetDrawColor(FColor::Red);
		
		auto BufferRow = InputBuffer[i].InputsFrameState;
		FString BufferRowText = "";
		for (auto InputID : InputMap->GetActionIDs())
		{
			BufferRowText += FString::FromInt(BufferRow[InputID].HoldTime);
			if (BufferRow[InputID].bUsed) BufferRowText += ">";
			BufferRowText += "            "; // Spacing
		}
		DisplayDebugManager.DrawString(BufferRowText);
	}
	
	DisplayDebugManager.SetYPos(YPosD);
	DisplayDebugManager.SetDrawColor(FColor::Yellow);
	float largestString = 0;
	FString DirectionalIDs = "";
	for (auto InputID : InputMap->GetDirectionalIDs())
	{
		if (InputID.ToString().Len() > largestString) largestString = InputID.ToString().Len();
		DirectionalIDs = InputID.ToString();
		DisplayDebugManager.DrawString(DirectionalIDs, XOffset);
	}
	XOffset += (largestString + 4) * DisplayDebugManager.GetMaxCharHeight();
	
	DisplayDebugManager.SetDrawColor(FColor::White);

	if (!InputMap->DirectionalActionMap) return;
	DisplayDebugManager.SetYPos(YPosD);
	FString DirectionalValues = "";	
	for (const auto DirectionalAction : InputMap->DirectionalActionMap->GetMappings())
	{
		DirectionalValues = FString::FromInt(DirectionalInputValidFrame[DirectionalAction->GetID()]);
		if (DirectionalAction->bAngleChange) DirectionalValues += "     " + FString::SanitizeFloat(DirectionalAction->CurAngle);
		DisplayDebugManager.DrawString(DirectionalValues, XOffset);
	}
}

