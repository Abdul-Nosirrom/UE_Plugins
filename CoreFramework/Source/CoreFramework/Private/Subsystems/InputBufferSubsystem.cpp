// Fill out your copyright notice in the Description page of Project Settings.


#include "Subsystems/InputBufferSubsystem.h"
#include "InputBufferPrimitives.h"

DECLARE_CYCLE_STAT(TEXT("Update Buffer"), STAT_UpdateBuffer, STATGROUP_InputBuffer)

/*~~~~~ Initialize Static Members ~~~~~*/
TArray<FName> UInputBufferSubsystem::InputActionIDs;
TMap<FName, bool> UInputBufferSubsystem::RawButtonContainer;
TMap<FName, FVector2D> UInputBufferSubsystem::RawAxisContainer;

void UInputBufferSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	bInitialized = false;
	Super::Initialize(Collection);
	BufferSize = 20;
}

void UInputBufferSubsystem::Deinitialize()
{
	Super::Deinitialize();
}


void UInputBufferSubsystem::AddMappingContext(UInputBufferMap* TargetInputMap, UEnhancedInputComponent* InputComponent)
{
	InputMap = TargetInputMap;
	InputActionIDs = InputMap->GetActionIDs();
	InputComponent->bBlockInput = false;
	InitializeInputMapping(InputComponent);
}

void UInputBufferSubsystem::InitializeInputMapping(UEnhancedInputComponent* InputComponent)
{
	/* Empty our tracking data, we're gonna override it with current InputMap */
	RawButtonContainer.Empty();
	RawAxisContainer.Empty();
	
	/* If no input map has been assigned, we return */
	if (!InputMap) return;

	/* Add input map to the EnhancedInput subsystem*/
	UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer());
	Subsystem->ClearAllMappings();
	Subsystem->AddMappingContext(InputMap->InputActionMap, 0);
	
	auto Mappings = InputMap->InputActionMap->GetMappings();
	
	/* Generate Action Bindings */
	for (auto ActionMap : Mappings)
	{
		const FName ActionName = FName(ActionMap.Action->ActionDescription.ToString());
		switch (ActionMap.Action->ValueType)
		{
			case EInputActionValueType::Boolean:
				RawButtonContainer.Add(ActionName, false);
			break;
			case EInputActionValueType::Axis2D:
				RawAxisContainer.Add(ActionName, FVector2D::ZeroVector);
			break;
			default:
				break;
		}
		
		InputComponent->BindAction(ActionMap.Action, ETriggerEvent::Triggered, this, &UInputBufferSubsystem::TriggerInput, ActionName);
		InputComponent->BindAction(ActionMap.Action, ETriggerEvent::Completed, this, &UInputBufferSubsystem::CompleteInput, ActionName);
	}

	InitializeInputBufferData();
}

void UInputBufferSubsystem::InitializeInputBufferData()
{
	//BufferSize = 20;
	InputBuffer = TBufferContainer<FBufferFrame>(BufferSize);

	/* Populate the buffer */
	for (int i = 0; i < BufferSize; i++)
	{
		FBufferFrame NewFrame = FBufferFrame();
		NewFrame.InitializeFrame();
		InputBuffer.PushBack(NewFrame);
	}
	
	/* Setup "tracking" data */
	ButtonInputValidFrame.Empty();
	for (auto ID : InputMap->GetActionIDs())
	{
		ButtonInputValidFrame.Add(ID, -1);
	}

	DirectionalInputValidFrame.Empty();
	for (auto ID : InputMap->GetDirectionalIDs())
	{
		DirectionalInputValidFrame.Add(ID, -1);
	}

	bInitialized = true;
}

//BEGIN Input Registration Events
void UInputBufferSubsystem::TriggerInput(const FInputActionInstance& ActionInstance, const FName InputName)
{
	const auto Value = ActionInstance.GetValue();

	switch (Value.GetValueType())
	{
		case EInputActionValueType::Boolean:
			RawButtonContainer[InputName] = Value.Get<bool>();
		break;
		case EInputActionValueType::Axis2D:
			RawAxisContainer[InputName] = Value.Get<FVector2D>();
		break;
		default:
			break;

	}
}

void UInputBufferSubsystem::CompleteInput(const FInputActionValue& ActionValue, const FName InputName)
{
	switch (ActionValue.GetValueType())
	{
		case EInputActionValueType::Boolean:
			RawButtonContainer[InputName] = false;
		break;
		case EInputActionValueType::Axis2D:
			RawAxisContainer[InputName] = FVector2D::ZeroVector;
		break;
		default:
			break;
	}
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
	
	/* Push newly updated frame to the buffer, and delete oldest frame from heap */
	InputBuffer.PushFront(NewFrame);
	
	/* Store the frame value in which each action input can be used */
	for (const auto InputID : InputMap->GetActionIDs())
	{
		ButtonInputValidFrame[InputID] = -1;
		for (int frame = 0; frame < BufferSize; frame++)
		{
			/* First set release, let press override it though */
			if (InputBuffer[frame].InputsFrameState[InputID].HoldTime == -1) ButtonInputValidFrame[InputID] = -2; // NOTE: temp to recognize release
			/* Set the button state to the frame it can be used */
			if (InputBuffer[frame].InputsFrameState[InputID].CanExecute()) ButtonInputValidFrame[InputID] = frame;
		}
	}

	/* Store the frame value in which each directional input can be used */
	for (const auto DirectionalInput : InputMap->DirectionalActionMap->GetMappings())
	{
		const FName InputID = DirectionalInput->GetID();
		
		// Input for directional input evaluation is stored in UMotionMappingContext
		const FName DirectionInputAxisID = InputMap->DirectionalActionMap->GetDirectionalActionID();
		
		DirectionalInputValidFrame[InputID] = -1;
		DirectionalInput->Reset();
		// Reset CheckStep and CurAngle, their requirements must be satisfied within the buffer time-frame
		for (int frame = 0; frame < BufferSize; frame++)
		{
			const FVector2D DirectionInputVector = InputBuffer[frame].InputsFrameState[DirectionInputAxisID].VectorValue;
			
			if (DirectionalInput->CheckMotionDirection(DirectionInputVector))
			{
				DirectionalInputValidFrame[InputID] = frame;
			}
		}
	}
}


void UInputBufferSubsystem::EvaluateEvents()
{
	
	/* Invoke Action Events */
	for (auto Action : InputMap->InputActionMap->GetMappings())
	{
		const FName ActionID = FName(Action.Action->ActionDescription.ToString());
		const float HoldThreshold = 0.f;

		// Or could consolidate it into 1 event and have an Enum differentiate it?

		const bool bPressed = ButtonInputValidFrame[ActionID] > 0;
		//const bool bHeld = false;//InputBufferObject.CheckButtonHeld(ActionID, HoldThreshold);
		const bool bReleased = ButtonInputValidFrame[ActionID] == -2;
		
		if (bPressed)
		{
			UE_LOG(LogTemp, Error, TEXT("Broadcasting pressed event for action %s"), *Action.Action->ActionDescription.ToString());
			InputPressedDelegate.Broadcast(Action.Action, FInputActionValue());
		}
		//if (bHeld)
		//{
		//	float TimeHeld = 0.f;
			//InputHeldDelegateSignature.Broadcast(Action.Action, TimeHeld);
		//}
		if (bReleased)
		{
			InputReleasedDelegate.Broadcast(Action.Action);
		}
	}

	/* For Directional Events */
	for (auto Direction : InputMap->DirectionalActionMap->GetMappings())
	{
		
		const bool bRegistered = DirectionalInputValidFrame[Direction->GetID()] > 0;
		if (bRegistered)
		{
			DirectionalInputRegisteredDelegate.Broadcast(Direction);

			for (auto Action : InputMap->InputActionMap->GetMappings())
			{
				const FName ActionID = FName(Action.Action->ActionDescription.ToString());
				const bool bPressed = ButtonInputValidFrame[ActionID] > 0 && DirectionalInputValidFrame[Direction->GetID()] > ButtonInputValidFrame[ActionID];

				if (bPressed)
				{
					DirectionalAndButtonDelegate.Broadcast(Action.Action, Direction);
				}
			}
		}
	}
}

bool UInputBufferSubsystem::ConsumeButtonInput(const UInputAction* Input)
{
	if (!Input) return false;
	
	const FName InputID = FName(Input->ActionDescription.ToString());

	// TODO: Logging...
	if (!ButtonInputValidFrame.Contains(InputID)) return false;
	
	if (ButtonInputValidFrame[InputID] < 0) return false;
	if (InputBuffer[ButtonInputValidFrame[InputID]].InputsFrameState[InputID].CanExecute())
	{
		InputBuffer[ButtonInputValidFrame[InputID]].InputsFrameState[InputID].bUsed = true;
		ButtonInputValidFrame[InputID] = -1;
		return true;
	}
	return false;
}

bool UInputBufferSubsystem::ConsumeDirectionalInput(const UMotionAction* Input)
{
	if (!Input) return false;
	
	const FName InputID = Input->GetID();

	if (!DirectionalInputValidFrame.Contains(InputID)) return false;
	
	if (DirectionalInputValidFrame[InputID] < 0) return false;
	if (InputBuffer[DirectionalInputValidFrame[InputID]].InputsFrameState[InputID].CanExecute())
	{
		InputBuffer[DirectionalInputValidFrame[InputID]].InputsFrameState[InputID].bUsed = true;
		DirectionalInputValidFrame[InputID] = -1;
		return true;
	}
	return false;
}

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
		InputFrames += FString::FromInt(ButtonInputValidFrame[ID]);
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
	for (int i = 0; i < BufferSize; i++)
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
	for (auto ID : InputMap->GetDirectionalIDs())
	{
		DirectionalValues = FString::FromInt(DirectionalInputValidFrame[ID]);
		DisplayDebugManager.DrawString(DirectionalValues, XOffset);
	}
}

