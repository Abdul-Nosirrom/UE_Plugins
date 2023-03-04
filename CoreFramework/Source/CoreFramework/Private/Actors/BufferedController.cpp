// Copyright 2023 Abdulrahmen Almodaimegh. All Rights Reserved.
#include "BufferedController.h"
#include "CFW_PCH.h"
#include "Subsystems/InputBufferSubsystem.h"


// TODO: Rather than messing with gameplay tags and such, we can just directly select the MotionAction and InputAction data assets...

// Sets default values
ABufferedController::ABufferedController() : Super()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	//SetActorTickInterval(0.1);
}

// Called when the game starts or when spawned
void ABufferedController::BeginPlay()
{
	Super::BeginPlay();
}


void ABufferedController::DisplayDebug(UCanvas* Canvas, const FDebugDisplayInfo& DebugDisplay, float& YL,
	float& YPos)
{
	if (DebugDisplay.IsDisplayOn(TEXT("INPUTBUFFER")))
	{
		GetLocalPlayer()->GetSubsystem<UInputBufferSubsystem>()->DisplayDebug(Canvas, DebugDisplay, YL, YPos);
	}
}