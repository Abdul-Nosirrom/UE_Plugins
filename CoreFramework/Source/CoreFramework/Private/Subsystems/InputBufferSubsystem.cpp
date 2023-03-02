// Fill out your copyright notice in the Description page of Project Settings.


#include "Subsystems/InputBufferSubsystem.h"
#include "InputBufferPrimitives.h"

DECLARE_CYCLE_STAT(TEXT("Update Buffer"), STAT_UpdateBuffer, STATGROUP_InputBuffer)


void UInputBufferSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

}

void UInputBufferSubsystem::Deinitialize()
{
	Super::Deinitialize();
}


void UInputBufferSubsystem::AddMappingContext(UInputBufferMap* TargetInputMap, UEnhancedInputComponent* InputComponent)
{
	InputMap = TargetInputMap;
	InitializeInputMapping(InputComponent);
}


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

	/* Store the frame value in which each input can be used */
	for (auto InputID : InputIDs)
	{
		ButtonInputValidFrame[InputID] = -1;
		for (int frame = 0; frame < BufferSize; frame++)
		{
			/* Set the button state to the frame it can be used */
			if (InputBuffer[frame].InputsFrameState[InputID].CanExecute()) ButtonInputValidFrame[InputID] = frame;
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

		const bool bPressed = false;//InputBufferObject.CheckButtonPressed(ActionID);
		const bool bHeld = false;//InputBufferObject.CheckButtonHeld(ActionID, HoldThreshold);
		const bool bReleased = false;//InputBufferObject.CheckButtonReleased(ActionID);
		
		if (bPressed)
		{
			//InputPressedDelegateSignature.Broadcast(Action.Action);
		}
		if (bHeld)
		{
			float TimeHeld = 0.f;
			//InputHeldDelegateSignature.Broadcast(Action.Action, TimeHeld);
		}
		if (bReleased)
		{
			//InputReleasedDelegateSignature.Broadcast(Action.Action);
		}
	}

	/* For Directional Events */
	for (auto Direction : InputMap->DirectionalActionMap->GetMappings())
	{
		
		const bool bRegistered = true;//InputBufferObject.CheckDirectionRegistered(Direction->GetID());
		if (bRegistered)
		{
			DirectionalInputRegisteredDelegate.Broadcast(Direction);
		}
	}
}

bool UInputBufferSubsystem::ConsumeButtonInput(const UInputAction* Input)
{
	const FName InputID = FName(Input->ActionDescription.ToString());
	
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
	const FName InputID = Input->GetID();
	
	if (DirectionalInputValidFrame[InputID] < 0) return false;
	if (InputBuffer[DirectionalInputValidFrame[InputID]].InputsFrameState[InputID].CanExecute())
	{
		InputBuffer[DirectionalInputValidFrame[InputID]].InputsFrameState[InputID].bUsed = true;
		DirectionalInputValidFrame[InputID] = -1;
		return true;
	}
	return false;
}

void UInputBufferSubsystem::InitializeInputMapping(UEnhancedInputComponent* InputComponent)
{
	/* Empty our tracking data, we're gonna override it with current InputMap */
	InputIDs.Empty();
	RawButtonContainer.Empty();
	RawAxisContainer.Empty();
	
	/* If no input map has been assigned, we return */
	if (!InputMap) return;
	
	/* Initialize ID array */
	for (auto Mapping : InputMap->InputActionMap->GetMappings())
	{
		if (InputIDs.Contains(FName(Mapping.Action->ActionDescription.ToString()))) continue;
		InputIDs.Add(FName(Mapping.Action->ActionDescription.ToString()));
	}

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
}


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
