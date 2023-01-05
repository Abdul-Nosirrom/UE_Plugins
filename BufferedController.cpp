// Fill out your copyright notice in the Description page of Project Settings.


#include "BufferedController.h"

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"

#pragma region Input Buffer Core

FInputBuffer::FInputBuffer() : InputBuffer(BufferUtility::BUFFER_SIZE)
{
	/* Initialize Buffer Data Structure */
	for (int i = 0; i < BufferUtility::BUFFER_SIZE; i++)
	{
		FBufferFrame NewFrame = FBufferFrame();
		NewFrame.InitializeFrame();
		InputBuffer.PushBack(NewFrame);
	}

	/* Initialize Button State Container */
	for (auto ID : BufferUtility::InputIDs)
	{
		ButtonInputCurrentState.Add(ID, -1);
	}
}

void FInputBuffer::UpdateBuffer()
{
	/* Each frame, push a new list of inputs and their states to the front */
	FBufferFrame FrontFrame = InputBuffer.Front();
	FBufferFrame NewFrame = FBufferFrame();
	NewFrame.InitializeFrame();
	NewFrame.CopyFrameState(FrontFrame);
	NewFrame.UpdateFrameState();
	
	/* Push newly updated frame to the buffer, and delete oldest frame from heap */
	InputBuffer.PushBack(NewFrame);

	/* Store the frame value in which each input can be used */
	for (auto InputID : BufferUtility::InputIDs)
	{
		ButtonInputCurrentState[InputID] = -1;
		for (int frame = 0; frame < BufferUtility::BUFFER_SIZE; frame++)
		{
			/* Set the button state to the frame it can be used */
			if (InputBuffer[frame].InputsFrameState[InputID].CanExecute()) ButtonInputCurrentState[InputID] = frame;
		}
	}
}

void FInputBuffer::UseInput(const FName InputID)
{
	InputBuffer[ButtonInputCurrentState[InputID]].InputsFrameState[InputID].bUsed = true;
	ButtonInputCurrentState[InputID] = -1;
}

#pragma endregion Input Buffer Core

#pragma region Frame States

/* ~~~~~ Buffer Frame ~~~~~ */

void FBufferFrame::InitializeFrame()
{
	for (auto ID : BufferUtility::InputIDs)
	{
		FInputFrameState NewFS = FInputFrameState(ID);
		InputsFrameState.Add(ID, NewFS);
	}
}

void FBufferFrame::UpdateFrameState()
{
	for (auto FrameState : InputsFrameState)
	{
		FrameState.Value.ResolveCommand();
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
		//if (BufferUtility::RawAxisContainer[ID].IsZero()) HoldUp(BufferUtility::RawAxisContainer[ID]);
	}
}

// TODO: The condition for HoldTime == 1 is kind of rigid
bool FInputFrameState::CanExecute()
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

// Sets default values
ABufferedController::ABufferedController() 
{
	Super();
	
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	PrimaryActorTick.TickInterval = BufferUtility::TICK_INTERVAL;

}

// Called when the game starts or when spawned
void ABufferedController::BeginPlay()
{
	Super::BeginPlay();
	
	// Pass map to buffer
	InputBufferObject = FInputBuffer();
}

// Called every frame
void ABufferedController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	InputBufferObject.UpdateBuffer();
}

void ABufferedController::SetupInputComponent()
{
	Super::SetupInputComponent();

	/* Initialize ID array */
	for (auto Mapping : InputMap->GetMappings())
	{
		BufferUtility::InputIDs.Add(Mapping.Action.GetFName());
	}

	UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer());
	Subsystem->ClearAllMappings();
	Subsystem->AddMappingContext(InputMap, 0);
	
	UEnhancedInputComponent* PEI = Cast<UEnhancedInputComponent>(InputComponent);
	auto Mappings = InputMap->GetMappings();
	
	for (auto ActionMap : Mappings)
	{
		switch (ActionMap.Action->ValueType)
		{
			case EInputActionValueType::Boolean:
				BufferUtility::RawButtonContainer.Add(ActionMap.Action.GetFName(), -1.f);
				break;
			case EInputActionValueType::Axis2D:
				BufferUtility::RawAxisContainer.Add(ActionMap.Action.GetFName(), FVector2D::ZeroVector);
				break;
			default:
				break;
		}
		
		PEI->BindAction(ActionMap.Action, ETriggerEvent::Triggered, this, &ABufferedController::TriggerInput, ActionMap.Action.GetFName());
		PEI->BindAction(ActionMap.Action, ETriggerEvent::Completed, this, &ABufferedController::CompleteInput, ActionMap.Action.GetFName());
	}
}

void ABufferedController::TriggerInput(const FInputActionInstance& ActionInstance, const FName InputName)
{
	const auto Value = ActionInstance.GetValue();

	switch (Value.GetValueType())
	{
		case EInputActionValueType::Boolean:
			BufferUtility::RawButtonContainer[InputName] = Value.IsNonZero() ? ActionInstance.GetTriggeredTime() : 0.f;
			break;
		case EInputActionValueType::Axis2D:
			BufferUtility::RawAxisContainer[InputName] = Value.Get<FVector2D>();
			break;
		default:
			break;

	}
}

void ABufferedController::CompleteInput(const FInputActionValue& ActionValue, const FName InputName)
{
	switch (ActionValue.GetValueType())
	{
		case EInputActionValueType::Boolean:
			BufferUtility::RawButtonContainer[InputName] = 0.f;
			break;
		case EInputActionValueType::Axis2D:
			BufferUtility::RawAxisContainer[InputName] = FVector2D::ZeroVector;
			break;
		default:
			break;
	}
}