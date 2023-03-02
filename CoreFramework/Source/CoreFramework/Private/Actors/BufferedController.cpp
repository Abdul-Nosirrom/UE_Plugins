// Copyright 2023 Abdulrahmen Almodaimegh. All Rights Reserved.
#include "BufferedController.h"
#include "CFW_PCH.h"
#include "InputData.h"
#include "Subsystems/InputBufferSubsystem.h"

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
	//RegisterInputSignature.
	
}

// Called every frame
void ABufferedController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	auto IB = GetLocalPlayer()->GetSubsystem<UInputBufferSubsystem>();
}


void ABufferedController::DisplayDebug(UCanvas* Canvas, const FDebugDisplayInfo& DebugDisplay, float& YL,
	float& YPos)
{

	FDisplayDebugManager& DisplayDebugManager = Canvas->DisplayDebugManager;
	DisplayDebugManager.SetDrawColor(FColor::Yellow);
	const float XOffset = 0.f;
	
	if (DebugDisplay.IsDisplayOn(TEXT("INPUTBUFFER")))
	{
		DisplayDebugManager.DrawString(TEXT("-----INPUT BUFFER DEBUG-----"));
		/* Draw Buffer Oldest Frame Vals*/
		FString InputFrames = "";
		for (auto ID : BufferUtility::InputIDs)
		{
			InputFrames += FString::FromInt(InputBufferObject.ButtonOldestValidFrame[ID]);
			InputFrames += "            ";
		}
		DisplayDebugManager.SetDrawColor(FColor::Red);
		DisplayDebugManager.DrawString(InputFrames);
		DisplayDebugManager.SetDrawColor(FColor::Yellow);
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
				if (BufferRow[InputID].bUsed) BufferRowText += ">";
				BufferRowText += "            "; // Spacing
			}
			DisplayDebugManager.DrawString(BufferRowText);
		}
	}

	//Super::DisplayDebug(Canvas, DebugDisplay, YL, YPos);
}