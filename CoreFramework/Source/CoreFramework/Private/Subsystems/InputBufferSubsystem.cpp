// Fill out your copyright notice in the Description page of Project Settings.


#include "Subsystems/InputBufferSubsystem.h"
#include "InputBufferPrimitives.h"
#include "Debug/IB_LOG.h"

DECLARE_CYCLE_STAT(TEXT("Update Buffer"), STAT_UpdateBuffer, STATGROUP_InputBuffer)
DECLARE_CYCLE_STAT(TEXT("Eval Events"), STAT_EvalEvents, STATGROUP_InputBuffer)

DEFINE_LOG_CATEGORY(LogInputBuffer)

/*~~~~~ Initialize Static Members ~~~~~*/
TArray<FName> UInputBufferSubsystem::CachedActionIDs;
TMap<FName, FRawInputValue> UInputBufferSubsystem::RawValueContainer;

void UInputBufferSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	bInitialized = false;
	ElapsedTime = 0;
}

void UInputBufferSubsystem::Deinitialize()
{
	Super::Deinitialize();
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
	InputComponent->bBlockInput = false;
	InitializeInputMapping(InputComponent);

	IB_FLog(Log, "Input Buffer Initialized")
}

void UInputBufferSubsystem::InitializeInputMapping(UEnhancedInputComponent* InputComponent)
{
	/* Empty our tracking data, we're gonna override it with current InputMap */
	RawValueContainer.Empty();
	
	/* If no input map has been assigned, we return */
	if (!InputMap) return;

	/* Add input map to the EnhancedInput subsystem*/
	UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer());
	Subsystem->ClearAllMappings();
	Subsystem->AddMappingContext(InputMap->InputActionMap, 0);
	
	auto InputActionCache = InputMap->GetInputActions();
	
	/* Generate Action Bindings */
	for (auto Action : InputActionCache)
	{
		const FName ActionName = FName(Action->ActionDescription.ToString());
		RawValueContainer.Add(ActionName, FRawInputValue());

		InputComponent->BindAction(Action, ETriggerEvent::Triggered, this, &UInputBufferSubsystem::TriggerInput, ActionName);
		InputComponent->BindAction(Action, ETriggerEvent::Completed, this, &UInputBufferSubsystem::CompleteInput, ActionName);
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
		NewFrame.InitializeFrame();
		InputBuffer.PushBack(NewFrame);
	}
	
	/* Setup "tracking" data */
	ButtonInputValidFrame.Empty();
	for (auto ID : InputMap->GetActionIDs())
	{
		IB_FLog(Display, "[%s] - Action Added To Buffer", *ID.ToString())
		ButtonInputValidFrame.Add(ID, FBufferState());
	}

	DirectionalInputValidFrame.Empty();
	for (auto ID : InputMap->GetDirectionalIDs())
	{
		IB_FLog(Display, "[%s] - Directional Added To Buffer", *ID.ToString())
		DirectionalInputValidFrame.Add(ID, -1);
	}
}

//BEGIN Input Registration Events
void UInputBufferSubsystem::TriggerInput(const FInputActionInstance& ActionInstance, const FName InputName)
{
	RawValueContainer[InputName] = FRawInputValue(ActionInstance);
}

void UInputBufferSubsystem::CompleteInput(const FInputActionInstance& ActionInstance, const FName InputName)
{
	RawValueContainer[InputName] = FRawInputValue(ActionInstance);
}
//END Input Registration Events

void UInputBufferSubsystem::UpdateBuffer()
{
	SCOPE_CYCLE_COUNTER(STAT_UpdateBuffer)
	
	/* Each frame, push a new list of inputs and their states to the front */
	FBufferFrame FrontFrame = InputBuffer.Front();
	FBufferFrame NewFrame = FBufferFrame();
	NewFrame.InitializeFrame();
	NewFrame.CopyFrameState(FrontFrame);
	NewFrame.UpdateFrameState();
	
	/* Push newly updated frame to the buffer */
	InputBuffer.PushFront(NewFrame);
	
	/* Store the frame value in which each action input can be used */
	for (const auto InputID : InputMap->GetActionIDs())
	{
		ButtonInputValidFrame[InputID].Reset();
		
		for (int frame = 0; frame < BUFFER_SIZE; frame++) // Iterating From Newest To Oldest Input (Most Valid Oldest Input Takes Prescedence)
		{
			const auto FrameState = InputBuffer[frame].InputsFrameState[InputID];

			/* Set the button state to the frame it can be used */
			if (FrameState.CanInvokePress()) // This input has a valid hold
			{
				ButtonInputValidFrame[InputID] = FBufferState(frame, 0, FrameState.bUsed);
			}
			else if (FrameState.CanInvokeHold()) // This input has a valid hold
			{
				ButtonInputValidFrame[InputID] = FBufferState(frame, FrameState.HoldTime * TICK_INTERVAL, FrameState.bUsed);
			}
			else if (FrameState.CanInvokeRelease()) // This input has a valid release
			{
				ButtonInputValidFrame[InputID] = FBufferState(frame, 0, FrameState.bUsed);
			}
		}
	}

	/* Store the frame value in which each directional input can be used */
	for (const auto DirectionalInput : InputMap->DirectionalActionMap->GetMappings())
	{
		const FName InputID = DirectionalInput->GetID();
		
		// Input for directional input evaluation is stored in UMotionMappingContext
		const FName DirectionInputAxisID = InputMap->DirectionalActionMap->GetDirectionalActionID();
		
		DirectionalInputValidFrame[InputID] = -1;
		DirectionalInput->Reset(); // Resets transient values that are to be checked during the buffer window

		for (int frame = BUFFER_SIZE-1; frame >= 0; frame--) // Iterating From Oldest To Newest (To Check Sequence Order)
		{
			const FVector2D DirectionInputVector = InputBuffer[frame].InputsFrameState[DirectionInputAxisID].Value.Get<FVector2D>();
			FVector ProcessedInputVector, PlayerForward, PlayerRight;
			ProcessDirectionVector(DirectionInputVector, ProcessedInputVector, PlayerForward, PlayerRight);
			
			if (DirectionalInput->CheckMotionDirection(DirectionInputVector, ProcessedInputVector, PlayerForward, PlayerRight))
			{
				DirectionalInputValidFrame[InputID] = frame; // Will usually be zero, the frame in which the input was satisfied
			}
		}
	}
}


void UInputBufferSubsystem::EvaluateEvents()
{
	SCOPE_CYCLE_COUNTER(STAT_EvalEvents)
	
	/* Invoke Action Events */
	for (auto Action : InputMap->GetInputActions())
	{
		const FName ActionID = FName(Action->ActionDescription.ToString());
		const float HoldThreshold = 0.f;

		// Or could consolidate it into 1 event and have an Enum differentiate it?

		const bool bPressed = ButtonInputValidFrame[ActionID].IsPress();
		const bool bHeld = ButtonInputValidFrame[ActionID].IsHold();
		const bool bReleased = ButtonInputValidFrame[ActionID].IsRelase();

		if (bPressed)
		{
			InputPressedDelegate.Broadcast(Action, FInputActionValue());
		}
		//if (bHeld)
		//{
		//	float TimeHeld = 0.f;
			//InputHeldDelegateSignature.Broadcast(Action.Action, TimeHeld);
		//}
		if (bReleased)
		{
			InputReleasedDelegate.Broadcast(Action);
		}
	}

	/* For Directional Events */
	for (auto Direction : InputMap->DirectionalActionMap->GetMappings())
	{
		
		const bool bRegistered = DirectionalInputValidFrame[Direction->GetID()] >= 0;
		if (bRegistered)
		{
			DirectionalInputRegisteredDelegate.Broadcast(Direction);

			/*
			for (auto Action : InputMap->GetInputActions())
			{
				const FName ActionID = FName(Action->ActionDescription.ToString());
				const bool bPressed = ButtonInputValidFrame[ActionID] > 0 && DirectionalInputValidFrame[Direction->GetID()] > ButtonInputValidFrame[ActionID];

				if (bPressed)
				{
					DirectionalAndButtonDelegate.Broadcast(Action, Direction);
				}
			}
			*/
		}
	}
}

// Can only really consume press events
bool UInputBufferSubsystem::ConsumeButtonInput(const UInputAction* Input)
{
	if (!Input) return false;
	
	const FName InputID = FName(Input->ActionDescription.ToString());

	if (!ButtonInputValidFrame.Contains(InputID))
	{
		IB_FLog(Error, "%s - Input Action Registered But Not Collected In Buffer", *InputID.ToString())
		return false;
	}

	if (ButtonInputValidFrame[InputID].IsPress())
	{
		InputBuffer[ButtonInputValidFrame[InputID].GetAssociatedFrame()].InputsFrameState[InputID].bUsed = true;
		return true;
	}
	
	return false;
}

bool UInputBufferSubsystem::ConsumeDirectionalInput(const UMotionAction* Input)
{
	if (!Input) return false;
	
	const FName DirectionalID = Input->GetID();
	const FName ActionID = InputMap->DirectionalActionMap->GetDirectionalActionID();

	if (!DirectionalInputValidFrame.Contains(DirectionalID)) return false;
	
	if (DirectionalInputValidFrame[DirectionalID] < 0) return false;
	if (InputBuffer[DirectionalInputValidFrame[DirectionalID]].InputsFrameState[ActionID].CanInvokePress()) // TODO: UMotionAction ID doesnt exist in frame state
	{
		InputBuffer[DirectionalInputValidFrame[DirectionalID]].InputsFrameState[ActionID].bUsed = true;
		DirectionalInputValidFrame[DirectionalID] = -1;
		return true;
	}
	return false;
}

void UInputBufferSubsystem::ProcessDirectionVector(const FVector2D& DirectionInputVector, FVector& ProcessedInputVector, FVector& OutPlayerForward, FVector& OutPlayerRight) const
{
	const auto Controller = GetLocalPlayer<ULocalPlayer>()->GetPlayerController(GetWorld());
	
	if (Controller != nullptr)
	{
		// find out which way is forward
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		// get forward vector
		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	
		// get right vector 
		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

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
		InputFrames += FString::FromInt(ButtonInputValidFrame[ID].GetAssociatedFrame());
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
	DisplayDebugManager.SetDrawColor(FColor::White);

	XOffset *= 0.75 * InputNames.Len();
	
	/* Next we iterate through each row of the input buffer, drawing it to the screen */
	for (int i = 0; i < BUFFER_SIZE; i++)
	{
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
		if (InputID.GetStringLength() > largestString) largestString = InputID.GetStringLength();
		DirectionalIDs = InputID.ToString();
		DisplayDebugManager.DrawString(DirectionalIDs, XOffset);
	}
	XOffset += (largestString + 4) * DisplayDebugManager.GetMaxCharHeight();
	
	DisplayDebugManager.SetDrawColor(FColor::White);
	
	DisplayDebugManager.SetYPos(YPosD);
	FString DirectionalValues = "";	
	for (const auto DirectionalAction : InputMap->DirectionalActionMap->GetMappings())
	{
		DirectionalValues = FString::FromInt(DirectionalInputValidFrame[DirectionalAction->GetID()]);
		if (DirectionalAction->bAngleChange) DirectionalValues += "     " + FString::SanitizeFloat(DirectionalAction->CurAngle);
		DisplayDebugManager.DrawString(DirectionalValues, XOffset);
	}
}

