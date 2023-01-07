// Fill out your copyright notice in the Description page of Project Settings.


#include "BufferedController.h"

#include "DisplayDebugHelpers.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameplayTagsManager.h"
#include "InputMappingContext.h"
#include "Engine/Canvas.h"

namespace BufferUtility
{
	TArray<FName> InputIDs;

	TMap<FName, bool> RawButtonContainer;
	TMap<FName, FVector2D> RawAxisContainer;
}

// TODO: Rather than messing with gameplay tags and such, we can just directly select the MotionAction and InputAction data assets...

// Sets default values
ABufferedController::ABufferedController() : Super()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
}

// Called when the game starts or when spawned
void ABufferedController::BeginPlay()
{
	Super::BeginPlay();
	
	// Pass map to buffer
	InputBufferObject = FInputBuffer(BufferSize);
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

	BufferUtility::InputIDs.Empty();
	BufferUtility::RawButtonContainer.Empty();
	BufferUtility::RawAxisContainer.Empty();
	
	/* If no input map has been assigned, we return */
	if (!InputMap) return;
	
	/* Initialize ID array */
	for (auto Mapping : InputMap->GetMappings())
	{
		if (BufferUtility::InputIDs.Contains(FName(Mapping.Action->ActionDescription.ToString()))) continue;
		BufferUtility::InputIDs.Add(FName(Mapping.Action->ActionDescription.ToString()));
		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Blue, Mapping.Action->ActionDescription.ToString());
	}

	/* Add input map to the EnhancedInput subsystem*/
	UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer());
	Subsystem->ClearAllMappings();
	Subsystem->AddMappingContext(InputMap, 0);
	
	UEnhancedInputComponent* PEI = Cast<UEnhancedInputComponent>(InputComponent);
	auto Mappings = InputMap->GetMappings();

	/* Generate Action Bindings */
	for (auto ActionMap : Mappings)
	{
		const FName ActionName = FName(ActionMap.Action->ActionDescription.ToString());
		switch (ActionMap.Action->ValueType)
		{
			case EInputActionValueType::Boolean:
				BufferUtility::RawButtonContainer.Add(ActionName, false);
				break;
			case EInputActionValueType::Axis2D:
				BufferUtility::RawAxisContainer.Add(ActionName, FVector2D::ZeroVector);
				break;
			default:
				break;
		}
		
		PEI->BindAction(ActionMap.Action, ETriggerEvent::Triggered, this, &ABufferedController::TriggerInput, ActionName);
		PEI->BindAction(ActionMap.Action, ETriggerEvent::Completed, this, &ABufferedController::CompleteInput, ActionName);
	}
}

void ABufferedController::TriggerInput(const FInputActionInstance& ActionInstance, const FName InputName)
{
	const auto Value = ActionInstance.GetValue();

	switch (Value.GetValueType())
	{
		case EInputActionValueType::Boolean:
			BufferUtility::RawButtonContainer[InputName] = Value.Get<bool>();
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
			BufferUtility::RawButtonContainer[InputName] = false;
			break;
		case EInputActionValueType::Axis2D:
			BufferUtility::RawAxisContainer[InputName] = FVector2D::ZeroVector;
			break;
		default:
			break;
	}
}

void ABufferedController::DisplayDebug(UCanvas* Canvas, const FDebugDisplayInfo& DebugDisplay, float& YL,
	float& YPos)
{

	FDisplayDebugManager& DisplayDebugManager = Canvas->DisplayDebugManager;
	DisplayDebugManager.SetDrawColor(FColor::Yellow);
	const float XOffset = 0.f;
	
	if (DebugDisplay.IsDisplayOn(NAME_Input))
	{
		DisplayDebugManager.DrawString(TEXT("-----INPUT BUFFER DEBUG-----"));
		/* Write the input names on the first row */
		FString InputNames = "";
		for (auto ID : BufferUtility::InputIDs)
		{
			//FText InputName = Mapping.Action->ActionDescription;
			InputNames += ID.ToString();
			InputNames += "    "; // Spacing
		}
		DisplayDebugManager.DrawString(InputNames);
		DisplayDebugManager.SetDrawColor(FColor::White);
		
		/* Next we iterate through each row of the input buffer, drawing it to the screen */
		for (int i = 0; i < BufferSize; i++)
		{
			auto BufferRow = InputBufferObject.InputBuffer[i].InputsFrameState;
			FString BufferRowText = "";
			for (auto InputID : BufferUtility::InputIDs)
			{
				BufferRowText += FString::FromInt(BufferRow[InputID].HoldTime);
				BufferRowText += "            "; // Spacing
			}
			DisplayDebugManager.DrawString(BufferRowText);
		}
	}

	Super::DisplayDebug(Canvas, DebugDisplay, YL, YPos);
}