﻿// Copyright 2023 Abdulrahmen Almodaimegh. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BufferedController.generated.h"

UCLASS()
class COREFRAMEWORK_API ABufferedController : public APlayerController
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	ABufferedController();


protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	
	TObjectPtr<class UInputBufferSubsystem> InputBufferSubsystem;
	
public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;


private:
	double BufferUpdateTimer;
	double BufferTickInterval;
	
	virtual void DisplayDebug(UCanvas* Canvas, const FDebugDisplayInfo& DebugDisplay, float& YL, float& YPos) override;
};
