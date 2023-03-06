// Fill out your copyright notice in the Description page of Project Settings.


#include "RootMotionTasks/RootMotionTask_Base.h"
#include "RootMotionSourceCFW.h"
#include "RadicalMovementComponent.h"
#include "RadicalCharacter.h"

URootMotionTask_Base::URootMotionTask_Base()
	: Super()
{
	//bTickingTask = true; NOTE: Assuming All of these are ticking

	ForceName = NAME_None;
	FinishVelocityMode = ERootMotionFinishVelocityMode::MaintainLastRootMotionVelocity;
	FinishSetVelocity = FVector::ZeroVector;
	FinishClampVelocity = 0.0f;
	MovementComponent = nullptr;
	RootMotionSourceID = (uint16)ERootMotionSourceID::Invalid;
	bIsFinished = false;
	StartTime = 0.0f;
	EndTime = 0.0f;
}

void URootMotionTask_Base::Tick(float DeltaTime)
{
	if (LastFrameNumberWeTicked == GFrameCounter || !CharacterOwner.Get()) return;
	
	TickTask(DeltaTime);

	LastFrameNumberWeTicked = GFrameCounter;
}


void URootMotionTask_Base::ReadyForActivation()
{
	UE_LOG(LogTemp, Error, TEXT("Am I Called First?"));
	if (CharacterOwner.Get())
	{
		UE_LOG(LogTemp, Error, TEXT("Activating Root Motion Task"));
		PerformActivation();
		//OwningTaskManager->AddTaskReadyForActivation(*this); // TODO:!
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Destroying Root Motion Task"));
		OnDestroy();
	}
}

void URootMotionTask_Base::PerformActivation()
{
	Activate();
}


void URootMotionTask_Base::InitTask(ARadicalCharacter* InTaskOwner)
{
	CharacterOwner = InTaskOwner;
	UE_LOG(LogTemp, Error, TEXT("Character Owner = %s"), *CharacterOwner->GetName())
}

AActor* URootMotionTask_Base::GetAvatarActor() const
{
	if (CharacterOwner.IsValid())
	{
		return CharacterOwner.Get();
	}

	return nullptr;
}

bool URootMotionTask_Base::HasTimedOut() const
{
	const TSharedPtr<FRootMotionSourceCFW> RMS = (MovementComponent ? MovementComponent->GetRootMotionSourceByID(RootMotionSourceID) : nullptr);
	if (!RMS.IsValid())
	{
		return true;
	}

	return RMS->Status.HasFlag(ERootMotionSourceStatusFlags::Finished);
}
