// Fill out your copyright notice in the Description page of Project Settings.


#include "RootMotionTasks/RootMotionTask_JumpForce.h"

#include "CustomMovementComponent.h"
#include "OPCharacter.h"

URootMotionTask_JumpForce::URootMotionTask_JumpForce() : Super()
{
	PathOffsetCurve = nullptr;
	TimeMappingCurve = nullptr;
	bHasLanded = false;
}


void URootMotionTask_JumpForce::Finish()
{
	bIsFinished = true;


	AActor* MyActor = GetAvatarActor();
	if (MyActor)
	{
		OnFinish.Broadcast();
	}
	

	OnDestroy();
}

void URootMotionTask_JumpForce::OnLandedCallback(const FHitResult& Hit)
{
	bHasLanded = true;

	// TriggerLanded immediately if we're past time allowed, otherwise it'll get caught next valid tick
	const float CurrentTime = CharacterOwner->GetWorld()->GetTimeSeconds();
	if (CurrentTime >= (StartTime+MinimumLandedTriggerTime))
	{
		TriggerLanded();
	}
}

URootMotionTask_JumpForce* URootMotionTask_JumpForce::ApplyRootMotionJumpForce(AOPCharacter* Owner,
	FName TaskInstanceName, FRotator Rotation, float Distance, float Height, float Duration,
	float MinimumLandedTriggerTime, bool bFinishOnLanded, ERootMotionFinishVelocityMode VelocityOnFinishMode,
	FVector SetVelocityOnFinish, float ClampVelocityOnFinish, UCurveVector* PathOffsetCurve,
	UCurveFloat* TimeMappingCurve)
{
	URootMotionTask_JumpForce* MyTask = AOPCharacter::NewRootMotionTask<URootMotionTask_JumpForce>(Owner, TaskInstanceName);

	MyTask->ForceName = TaskInstanceName;
	MyTask->Rotation = Rotation;
	MyTask->Distance = Distance;
	MyTask->Height = Height;
	MyTask->Duration = FMath::Max(Duration, KINDA_SMALL_NUMBER); // No zero duration
	MyTask->MinimumLandedTriggerTime = MinimumLandedTriggerTime * Duration; // MinimumLandedTriggerTime is normalized
	MyTask->bFinishOnLanded = bFinishOnLanded;
	MyTask->FinishVelocityMode = VelocityOnFinishMode;
	MyTask->FinishSetVelocity = SetVelocityOnFinish;
	MyTask->FinishClampVelocity = ClampVelocityOnFinish;
	MyTask->PathOffsetCurve = PathOffsetCurve;
	MyTask->TimeMappingCurve = TimeMappingCurve;
	MyTask->SharedInitAndApply();

	return MyTask;
}

void URootMotionTask_JumpForce::Activate()
{
	if (CharacterOwner.Get())
	{
		CharacterOwner.Get()->LandedDelegate.AddDynamic(this, &URootMotionTask_JumpForce::OnLandedCallback);
	}
}

void URootMotionTask_JumpForce::TickTask(float DeltaTime)
{
	if (bIsFinished)
	{
		return;
	}

	const float CurrentTime = CharacterOwner->GetWorld()->GetTimeSeconds();

	if (bHasLanded && CurrentTime >= (StartTime+MinimumLandedTriggerTime))
	{
		TriggerLanded();
		return;
	}

	Super::TickTask(DeltaTime);

	AActor* MyActor = GetAvatarActor();
	if (MyActor)
	{
		const bool bTimedOut = HasTimedOut();

		if (!bFinishOnLanded && bTimedOut)
		{
			// Task has finished
			Finish();
		}
	}
	else
	{
		Finish();
	}
}

void URootMotionTask_JumpForce::PreDestroyFromReplication()
{
	Super::PreDestroyFromReplication();
}

void URootMotionTask_JumpForce::OnDestroy()
{
	if (CharacterOwner.Get())
	{
		CharacterOwner->LandedDelegate.RemoveDynamic(this, &URootMotionTask_JumpForce::OnLandedCallback);
	}

	if (MovementComponent)
	{
		MovementComponent->RemoveRootMotionSourceByID(RootMotionSourceID);
	}

	Super::OnDestroy();
}

void URootMotionTask_JumpForce::SharedInitAndApply()
{
	if (CharacterOwner.Get() && IsValid(CharacterOwner->GetMovementComponent()))
	{
		MovementComponent = Cast<UCustomMovementComponent>(CharacterOwner->GetMovementComponent());
		StartTime = CharacterOwner->GetWorld()->GetTimeSeconds();
		EndTime = StartTime + Duration;

		if (MovementComponent)
		{
			ForceName = ForceName.IsNone() ? FName("AbilityTaskApplyRootMotionJumpForce") : ForceName;
			TSharedPtr<FOPRootMotionSource_JumpForce> JumpForce = MakeShared<FOPRootMotionSource_JumpForce>();
			JumpForce->InstanceName = ForceName;
			JumpForce->AccumulateMode = ERootMotionAccumulateMode::Override;
			JumpForce->Priority = 500;
			JumpForce->Duration = Duration;
			JumpForce->Rotation = Rotation;
			JumpForce->Distance = Distance;
			JumpForce->Height = Height;
			JumpForce->Duration = Duration;
			JumpForce->bDisableTimeout = bFinishOnLanded; // If we finish on landed, we need to disable force's timeout
			JumpForce->PathOffsetCurve = PathOffsetCurve;
			JumpForce->TimeMappingCurve = TimeMappingCurve;
			JumpForce->FinishVelocityParams.Mode = FinishVelocityMode;
			JumpForce->FinishVelocityParams.SetVelocity = FinishSetVelocity;
			JumpForce->FinishVelocityParams.ClampVelocity = FinishClampVelocity;
			RootMotionSourceID = MovementComponent->ApplyRootMotionSource(JumpForce);
		}
	}
}

void URootMotionTask_JumpForce::TriggerLanded()
{
	OnLanded.Broadcast();

	if (bFinishOnLanded)
	{
		Finish();
	}
}
