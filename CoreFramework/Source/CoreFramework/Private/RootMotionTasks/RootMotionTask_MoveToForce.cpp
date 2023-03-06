// Fill out your copyright notice in the Description page of Project Settings.


#include "RootMotionTasks/RootMotionTask_MoveToForce.h"
#include "CustomMovementComponent.h"
#include "OPCharacter.h"
#include "OPRootMotionSource.h"

URootMotionTask_MoveToForce* URootMotionTask_MoveToForce::ApplyRootMotionMoveToForce(AOPCharacter* Owner,
	FName TaskInstanceName, FVector TargetLocation, float Duration, bool bSetNewMovementMode,
	EMovementState MovementMode, bool bRestrictSpeedToExpected, UCurveVector* PathOffsetCurve,
	ERootMotionFinishVelocityMode VelocityOnFinishMode, FVector SetVelocityOnFinish, float ClampVelocityOnFinish)
{

	URootMotionTask_MoveToForce* MyTask = AOPCharacter::NewRootMotionTask<URootMotionTask_MoveToForce>(Owner);

	MyTask->ForceName = TaskInstanceName;
	MyTask->TargetLocation = TargetLocation;
	MyTask->Duration = FMath::Max(Duration, KINDA_SMALL_NUMBER); // Avoid negative or divide-by-zero cases
	MyTask->bSetNewMovementMode = bSetNewMovementMode;
	MyTask->NewMovementMode = MovementMode;
	MyTask->bRestrictSpeedToExpected = bRestrictSpeedToExpected;
	MyTask->PathOffsetCurve = PathOffsetCurve;
	MyTask->FinishVelocityMode = VelocityOnFinishMode;
	MyTask->FinishSetVelocity = SetVelocityOnFinish;
	MyTask->FinishClampVelocity = ClampVelocityOnFinish;
	if (MyTask->GetAvatarActor() != nullptr)
	{
		MyTask->StartLocation = MyTask->GetAvatarActor()->GetActorLocation();
	}
	else
	{
		checkf(false, TEXT("UAbilityTask_ApplyRootMotionMoveToForce called without valid avatar actor to get start location from."));
		MyTask->StartLocation = TargetLocation;
	}
	MyTask->SharedInitAndApply();

	return MyTask;
}

void URootMotionTask_MoveToForce::TickTask(float DeltaTime)
{
	if (bIsFinished)
	{
		return;
	}

	Super::TickTask(DeltaTime);

	AActor* MyActor = GetAvatarActor();
	if (MyActor)
	{
		const bool bTimedOut = HasTimedOut();
		const float ReachedDestinationDistanceSqr = 50.f * 50.f;
		const bool bReachedDestination = FVector::DistSquared(TargetLocation, MyActor->GetActorLocation()) < ReachedDestinationDistanceSqr;

		if (bTimedOut)
		{
			// Task has finished
			bIsFinished = true;
			MyActor->ForceNetUpdate();
			if (bReachedDestination)
			{
				OnTimedOutAndDestinationReached.Broadcast();
			}
			else
			{
				OnTimedOut.Broadcast();
			}
			OnDestroy();
		}
	}
	else
	{
		bIsFinished = true;
		OnDestroy();
	}
}

void URootMotionTask_MoveToForce::OnDestroy()
{
	if (MovementComponent)
	{
		MovementComponent->RemoveRootMotionSourceByID(RootMotionSourceID);

		if (bSetNewMovementMode)
		{
			MovementComponent->SetMovementState(PreviousMovementMode);
		}
	}

	Super::OnDestroy();
}

void URootMotionTask_MoveToForce::SharedInitAndApply()
{
	if (CharacterOwner.Get() && CharacterOwner->GetMovementComponent())
	{
		MovementComponent = Cast<UCustomMovementComponent>(CharacterOwner->GetMovementComponent());
		StartTime = MovementComponent->GetWorld()->GetTimeSeconds();
		EndTime = StartTime + Duration;

		if (MovementComponent)
		{
			if (bSetNewMovementMode)
			{
				PreviousMovementMode = MovementComponent->GetMovementState();
				MovementComponent->SetMovementState(NewMovementMode);
			}

			ForceName = ForceName.IsNone() ? FName("AbilityTaskApplyRootMotionMoveToForce") : ForceName;
			TSharedPtr<FOPRootMotionSource_MoveToForce> MoveToForce = MakeShared<FOPRootMotionSource_MoveToForce>();
			MoveToForce->InstanceName = ForceName;
			MoveToForce->AccumulateMode = ERootMotionAccumulateMode::Override;
			MoveToForce->Settings.SetFlag(ERootMotionSourceSettingsFlags::UseSensitiveLiftoffCheck);
			MoveToForce->Priority = 1000;
			MoveToForce->TargetLocation = TargetLocation;
			MoveToForce->StartLocation = StartLocation;
			MoveToForce->Duration = Duration;
			MoveToForce->bRestrictSpeedToExpected = bRestrictSpeedToExpected;
			MoveToForce->PathOffsetCurve = PathOffsetCurve;
			MoveToForce->FinishVelocityParams.Mode = FinishVelocityMode;
			MoveToForce->FinishVelocityParams.SetVelocity = FinishSetVelocity;
			MoveToForce->FinishVelocityParams.ClampVelocity = FinishClampVelocity;
			RootMotionSourceID = MovementComponent->ApplyRootMotionSource(MoveToForce);
		}
	}
}
