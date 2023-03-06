// Fill out your copyright notice in the Description page of Project Settings.


#include "RootMotionTasks/RootMotionTask_RadialForce.h"
#include "RadicalCharacter.h"
#include "RadicalMovementComponent.h"

inline URootMotionTask_RadialForce* URootMotionTask_RadialForce::ApplyRootMotionRadialForce(ARadicalCharacter* Owner,
	FName TaskInstanceName, FVector Location, AActor* LocationActor, float Strength, float Duration, float Radius,
	bool bIsPush, bool bIsAdditive, bool bNoZForce, UCurveFloat* StrengthDistanceFalloff, UCurveFloat* StrengthOverTime,
	bool bUseFixedWorldDirection, FRotator FixedWorldDirection, ERootMotionFinishVelocityMode VelocityOnFinishMode,
	FVector SetVelocityOnFinish, float ClampVelocityOnFinish)
{
	URootMotionTask_RadialForce* MyTask = ARadicalCharacter::NewRootMotionTask<URootMotionTask_RadialForce>(Owner, TaskInstanceName);

	MyTask->ForceName = TaskInstanceName;
	MyTask->Location = Location;
	MyTask->LocationActor = LocationActor;
	MyTask->Strength = Strength;
	MyTask->Radius = FMath::Max(Radius, SMALL_NUMBER); // No zero radius
	MyTask->Duration = Duration;
	MyTask->bIsPush = bIsPush;
	MyTask->bIsAdditive = bIsAdditive;
	MyTask->bNoZForce = bNoZForce;
	MyTask->StrengthDistanceFalloff = StrengthDistanceFalloff;
	MyTask->StrengthOverTime = StrengthOverTime;
	MyTask->bUseFixedWorldDirection = bUseFixedWorldDirection;
	MyTask->FixedWorldDirection = FixedWorldDirection;
	MyTask->FinishVelocityMode = VelocityOnFinishMode;
	MyTask->FinishSetVelocity = SetVelocityOnFinish;
	MyTask->FinishClampVelocity = ClampVelocityOnFinish;
	MyTask->SharedInitAndApply();

	return MyTask;
}

inline void URootMotionTask_RadialForce::TickTask(float DeltaTime)
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


inline void URootMotionTask_RadialForce::OnDestroy()
{
	if (MovementComponent)
	{
		MovementComponent->RemoveRootMotionSourceByID(RootMotionSourceID);
	}

	Super::OnDestroy();
}

inline void URootMotionTask_RadialForce::SharedInitAndApply()
{
	if (CharacterOwner.Get() && IsValid(CharacterOwner->GetMovementComponent()))
	{
		MovementComponent = Cast<URadicalMovementComponent>(CharacterOwner->GetMovementComponent());
		StartTime = MovementComponent->GetWorld()->GetTimeSeconds();
		EndTime = StartTime + Duration;

		if (MovementComponent)
		{
			ForceName = ForceName.IsNone() ? FName("AbilityTaskApplyRootMotionRadialForce") : ForceName;
			TSharedPtr<FRootMotionSourceCFW_RadialForce> RadialForce = MakeShared<FRootMotionSourceCFW_RadialForce>();
			RadialForce->InstanceName = ForceName;
			RadialForce->AccumulateMode = bIsAdditive ? ERootMotionAccumulateMode::Additive : ERootMotionAccumulateMode::Override;
			RadialForce->Priority = 5;
			RadialForce->Location = Location;
			RadialForce->LocationActor = LocationActor;
			RadialForce->Duration = Duration;
			RadialForce->Radius = Radius;
			RadialForce->Strength = Strength;
			RadialForce->bIsPush = bIsPush;
			RadialForce->bNoZForce = bNoZForce;
			RadialForce->StrengthDistanceFalloff = StrengthDistanceFalloff;
			RadialForce->StrengthOverTime = StrengthOverTime;
			RadialForce->bUseFixedWorldDirection = bUseFixedWorldDirection;
			RadialForce->FixedWorldDirection = FixedWorldDirection;
			RadialForce->FinishVelocityParams.Mode = FinishVelocityMode;
			RadialForce->FinishVelocityParams.SetVelocity = FinishSetVelocity;
			RadialForce->FinishVelocityParams.ClampVelocity = FinishClampVelocity;
			RadialForce->AssociatedTask = this;
			RootMotionSourceID = MovementComponent->ApplyRootMotionSource(RadialForce);
		}
	}
}