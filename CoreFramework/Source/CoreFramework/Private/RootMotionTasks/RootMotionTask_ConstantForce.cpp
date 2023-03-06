// Fill out your copyright notice in the Description page of Project Settings.


#include "RootMotionTasks/RootMotionTask_ConstantForce.h"

#include "CustomMovementComponent.h"
#include "OPCharacter.h"
#include "OPRootMotionSource.h"


URootMotionTask_ConstantForce* URootMotionTask_ConstantForce::ApplyRootMotionConstantForce(AOPCharacter* Owner,
	FName TaskInstanceName, FVector WorldDirection, float Strength, float Duration, bool bIsAdditive,
	UCurveFloat* StrengthOverTime, ERootMotionFinishVelocityMode VelocityOnFinishMode, FVector SetVelocityOnFinish,
	float ClampVelocityOnFinish, bool bEnableGravity)
{

	URootMotionTask_ConstantForce* MyTask = AOPCharacter::NewRootMotionTask<URootMotionTask_ConstantForce>(Owner, TaskInstanceName);

	MyTask->ForceName = TaskInstanceName;
	MyTask->WorldDirection = WorldDirection.GetSafeNormal();
	MyTask->Strength = Strength;
	MyTask->Duration = Duration;
	MyTask->bIsAdditive = bIsAdditive;
	MyTask->StrengthOverTime = StrengthOverTime;
	MyTask->FinishVelocityMode = VelocityOnFinishMode;
	MyTask->FinishSetVelocity = SetVelocityOnFinish;
	MyTask->FinishClampVelocity = ClampVelocityOnFinish;
	MyTask->bEnableGravity = bEnableGravity;
	MyTask->SharedInitAndApply();

	return MyTask;
}

void URootMotionTask_ConstantForce::TickTask(float DeltaTime)
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
		const bool bIsInfiniteDuration = Duration < 0.f;

		if (!bIsInfiniteDuration && bTimedOut)
		{
			// Task has finished
			bIsFinished = true;
			MyActor->ForceNetUpdate();
			OnFinish.Broadcast();
			OnDestroy();
		}
	}
	else
	{
		bIsFinished = true;
		OnDestroy();
	}
}


void URootMotionTask_ConstantForce::OnDestroy()
{
	if (MovementComponent)
	{
		MovementComponent->RemoveRootMotionSourceByID(RootMotionSourceID);
	}

	Super::OnDestroy();
}

void URootMotionTask_ConstantForce::SharedInitAndApply()
{
	if (CharacterOwner.IsValid() && IsValid(CharacterOwner->GetCharacterMovement()))
	{
		MovementComponent = Cast<UCustomMovementComponent>(CharacterOwner->GetCharacterMovement());
		StartTime = CharacterOwner->GetWorld()->GetTimeSeconds();
		EndTime = StartTime + Duration;

		if (MovementComponent)
		{
			ForceName = ForceName.IsNone() ? FName("AbilityTaskApplyRootMotionConstantForce"): ForceName;
			TSharedPtr<FOPRootMotionSource_ConstantForce> ConstantForce = MakeShared<FOPRootMotionSource_ConstantForce>();
			ConstantForce->InstanceName = ForceName;
			ConstantForce->AccumulateMode = bIsAdditive ? ERootMotionAccumulateMode::Additive : ERootMotionAccumulateMode::Override;
			ConstantForce->Priority = 5;
			ConstantForce->Force = WorldDirection * Strength;
			ConstantForce->Duration = Duration;
			ConstantForce->StrengthOverTime = StrengthOverTime;
			ConstantForce->FinishVelocityParams.Mode = FinishVelocityMode;
			ConstantForce->FinishVelocityParams.SetVelocity = FinishSetVelocity;
			ConstantForce->FinishVelocityParams.ClampVelocity = FinishClampVelocity;
			if (bEnableGravity)
			{
				ConstantForce->Settings.SetFlag(ERootMotionSourceSettingsFlags::IgnoreZAccumulate);
			}
			RootMotionSourceID = MovementComponent->ApplyRootMotionSource(ConstantForce);
			
		}
	}

}
