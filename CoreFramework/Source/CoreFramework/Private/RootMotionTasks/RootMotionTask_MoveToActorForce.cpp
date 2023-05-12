// Fill out your copyright notice in the Description page of Project Settings.
#pragma once

#include "RootMotionTasks/RootMotionTask_MoveToActorForce.h"
#include "RootMotionSourceCFW.h"
#include "RadicalMovementComponent.h"
#include "RadicalCharacter.h"

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
int32 DebugMoveToActorForce = 0;
static FAutoConsoleVariableRef CVarRMDebugMoveToActorForce(
TEXT("RMTasks.DebugMoveToActorForce"),
	DebugMoveToActorForce,
	TEXT("Show debug info for MoveToActorForce"),
	ECVF_Default
	);
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

URootMotionTask_MoveToActorForce::URootMotionTask_MoveToActorForce()
: Super()
{
	bDisableDestinationReachedInterrupt = false;
	bSetNewMovementMode = false;
	NewMovementMode = EMovementState::STATE_Grounded;
	PreviousMovementMode = EMovementState::STATE_None;
	PreviousCustomMode = 0;
	TargetLocationOffset = FVector::ZeroVector;
	OffsetAlignment = ERootMotionTaskMoveToActorTargetOffsetType::AlignFromTargetToSource;
	bRestrictSpeedToExpected = false;
	PathOffsetCurve = nullptr;
	TimeMappingCurve = nullptr;
	TargetLerpSpeedHorizontalCurve = nullptr;
	TargetLerpSpeedVerticalCurve = nullptr;
}

URootMotionTask_MoveToActorForce* URootMotionTask_MoveToActorForce::ApplyRootMotionMoveToActorForce(
	ARadicalCharacter* Owner, FName TaskInstanceName, AActor* TargetActor,
	FVector TargetLocationOffset, ERootMotionTaskMoveToActorTargetOffsetType OffsetAlignment, float Duration,
	UCurveFloat* TargetLerpSpeedHorizontal, UCurveFloat* TargetLerpSpeedVertical, bool bSetNewMovementMode,
	EMovementState MovementMode, bool bRestrictSpeedToExpected, bool bApplyCurveInLocalSpace, UCurveVector* PathOffsetCurve,
	UCurveFloat* TimeMappingCurve, ERootMotionFinishVelocityMode VelocityOnFinishMode, FVector SetVelocityOnFinish,
	float ClampVelocityOnFinish, bool bDisableDestinationReachedInterrupt)
{
	UE_LOG(LogTemp, Error, TEXT("Or Am I Called First?"));
	URootMotionTask_MoveToActorForce* MyTask = ARadicalCharacter::NewRootMotionTask<URootMotionTask_MoveToActorForce>(Owner, TaskInstanceName);
	
	MyTask->ForceName = TaskInstanceName;
	MyTask->TargetActor = TargetActor;
	MyTask->TargetLocationOffset = TargetLocationOffset;
	MyTask->OffsetAlignment = OffsetAlignment;
	MyTask->Duration = FMath::Max(Duration, KINDA_SMALL_NUMBER); // Avoid negative or divide-by-zero cases
	MyTask->bDisableDestinationReachedInterrupt = bDisableDestinationReachedInterrupt;
	MyTask->TargetLerpSpeedHorizontalCurve = TargetLerpSpeedHorizontal;
	MyTask->TargetLerpSpeedVerticalCurve = TargetLerpSpeedVertical;
	MyTask->bSetNewMovementMode = bSetNewMovementMode;
	MyTask->NewMovementMode = MovementMode;
	MyTask->bRestrictSpeedToExpected = bRestrictSpeedToExpected;
	MyTask->bApplyCurveInLocalSpace = bApplyCurveInLocalSpace;
	MyTask->PathOffsetCurve = PathOffsetCurve;
	MyTask->TimeMappingCurve = TimeMappingCurve;
	MyTask->FinishVelocityMode = VelocityOnFinishMode;
	MyTask->FinishSetVelocity = SetVelocityOnFinish;
	MyTask->FinishClampVelocity = ClampVelocityOnFinish;
	if (MyTask->GetAvatarActor() != nullptr)
	{
		MyTask->StartLocation = MyTask->GetAvatarActor()->GetActorLocation();
	}
	else
	{
		checkf(false, TEXT("URootMotionTask_MoveToActorForce called without valid avatar actor to get start location from."));
		MyTask->StartLocation = TargetActor ? TargetActor->GetActorLocation() : FVector(0.f);
	}
	MyTask->SharedInitAndApply();

	return MyTask;
}

void URootMotionTask_MoveToActorForce::TickTask(float DeltaTime)
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

		// Update target location
		{
			const FVector PreviousTargetLocation = TargetLocation;
			if (UpdateTargetLocation(DeltaTime))
			{
				SetRootMotionTargetLocation(TargetLocation);
			}
			else
			{
				// TargetLocation not updated - TargetActor not around anymore, continue on to last set TargetLocation
			}
		}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		// Draw debug
		if (DebugMoveToActorForce > 0)
		{
			DrawDebugSphere(GetWorld(), TargetLocation, 50.f, 10, FColor::Green, false, 15.f);
		}
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

		const float ReachedDestinationDistanceSqr = 50.f * 50.f;
		const bool bReachedDestination = FVector::DistSquared(TargetLocation, MyActor->GetActorLocation()) < ReachedDestinationDistanceSqr;

		if (bTimedOut || (bReachedDestination && !bDisableDestinationReachedInterrupt))
		{
			// Task has finished
			bIsFinished = true;
			OnFinished.Broadcast(bReachedDestination, bTimedOut, TargetLocation);
			OnDestroy();
		}
	}
	else
	{
		bIsFinished = true;
		OnDestroy();
	}

}

void URootMotionTask_MoveToActorForce::OnDestroy()
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

void URootMotionTask_MoveToActorForce::SharedInitAndApply()
{
	if (CharacterOwner.IsValid() && IsValid(CharacterOwner->GetCharacterMovement()))
	{
		MovementComponent = CharacterOwner->GetCharacterMovement();
		StartTime = MovementComponent->GetWorld()->GetTimeSeconds();
		EndTime = StartTime + Duration;

		if (MovementComponent)
		{
			if (bSetNewMovementMode)
			{
				PreviousMovementMode = MovementComponent->GetMovementState();
				MovementComponent->SetMovementState(NewMovementMode);
			}

			// Set initial target location
			if (TargetActor)
			{
				TargetLocation = CalculateTargetOffset();
			}

			ForceName = ForceName.IsNone() ? FName("AbilityTaskApplyRootMotionMoveToActorForce") : ForceName;
			TSharedPtr<FRootMotionSourceCFW_MoveToDynamicForce> MoveToActorForce = MakeShared<FRootMotionSourceCFW_MoveToDynamicForce>();
			MoveToActorForce->InstanceName = ForceName;
			MoveToActorForce->AccumulateMode = ERootMotionAccumulateMode::Override;
			MoveToActorForce->Settings.SetFlag(ERootMotionSourceSettingsFlags::UseSensitiveLiftoffCheck);
			MoveToActorForce->Priority = 900;
			MoveToActorForce->InitialTargetLocation = TargetLocation;
			MoveToActorForce->TargetLocation = TargetLocation;
			MoveToActorForce->StartLocation = StartLocation;
			MoveToActorForce->Duration = FMath::Max(Duration, KINDA_SMALL_NUMBER);
			MoveToActorForce->bRestrictSpeedToExpected = bRestrictSpeedToExpected;
			MoveToActorForce->bApplyCurveInLocalSpace = bApplyCurveInLocalSpace;
			MoveToActorForce->PathOffsetCurve = PathOffsetCurve;
			MoveToActorForce->TimeMappingCurve = TimeMappingCurve;
			MoveToActorForce->FinishVelocityParams.Mode = FinishVelocityMode;
			MoveToActorForce->FinishVelocityParams.SetVelocity = FinishSetVelocity;
			MoveToActorForce->FinishVelocityParams.ClampVelocity = FinishClampVelocity;
			MoveToActorForce->AssociatedTask = this;
			RootMotionSourceID = MovementComponent->ApplyRootMotionSource(MoveToActorForce);

		}
	}
}

bool URootMotionTask_MoveToActorForce::UpdateTargetLocation(float DeltaTime)
{
	if (TargetActor && CharacterOwner.Get())
	{
		const FVector PreviousTargetLocation = TargetLocation;
		FVector ExactTargetLocation = CalculateTargetOffset();

		const float CurrentTime = CharacterOwner->GetWorld()->GetTimeSeconds();
		const float CompletionPercent = (CurrentTime - StartTime) / Duration;

		const float TargetLerpSpeedHorizontal = TargetLerpSpeedHorizontalCurve ? TargetLerpSpeedHorizontalCurve->GetFloatValue(CompletionPercent) : 1000.f;
		const float TargetLerpSpeedVertical = TargetLerpSpeedVerticalCurve ? TargetLerpSpeedVerticalCurve->GetFloatValue(CompletionPercent) : 500.f;

		const float MaxHorizontalChange = FMath::Max(0.f, TargetLerpSpeedHorizontal * DeltaTime);
		const float MaxVerticalChange = FMath::Max(0.f, TargetLerpSpeedVertical * DeltaTime);

		FVector ToExactLocation = ExactTargetLocation - PreviousTargetLocation;
		FVector TargetLocationDelta = ToExactLocation;

		// Cap vertical lerp
		if (FMath::Abs(ToExactLocation.Z) > MaxVerticalChange)
		{
			if (ToExactLocation.Z >= 0.f)
			{
				TargetLocationDelta.Z = MaxVerticalChange;
			}
			else
			{
				TargetLocationDelta.Z = -MaxVerticalChange;
			}
		}

		// Cap horizontal lerp
		if (FMath::Abs(ToExactLocation.SizeSquared2D()) > MaxHorizontalChange*MaxHorizontalChange)
		{
			FVector ToExactLocationHorizontal(ToExactLocation.X, ToExactLocation.Y, 0.f);
			ToExactLocationHorizontal.Normalize();
			ToExactLocationHorizontal *= MaxHorizontalChange;

			TargetLocationDelta.X = ToExactLocationHorizontal.X;
			TargetLocationDelta.Y = ToExactLocationHorizontal.Y;
		}

		TargetLocation += TargetLocationDelta;

		return true;
	}

	return false;
}

void URootMotionTask_MoveToActorForce::SetRootMotionTargetLocation(FVector NewTargetLocation)
{
	if (MovementComponent)
	{
		TSharedPtr<FRootMotionSourceCFW> RMS = MovementComponent->GetRootMotionSourceByID(RootMotionSourceID);
		if (RMS.IsValid())
		{
			if (RMS->GetScriptStruct() == FRootMotionSourceCFW_MoveToDynamicForce::StaticStruct())
			{
				FRootMotionSourceCFW_MoveToDynamicForce* MoveToActorForce = static_cast<FRootMotionSourceCFW_MoveToDynamicForce*>(RMS.Get());
				if (MoveToActorForce)
				{
					MoveToActorForce->SetTargetLocation(TargetLocation);
				}
			}
		}
	}
}

FVector URootMotionTask_MoveToActorForce::CalculateTargetOffset() const
{
	check(TargetActor != nullptr);

	const FVector TargetActorLocation = TargetActor->GetActorLocation();
	FVector CalculatedTargetLocation = TargetActorLocation;
	
	if (OffsetAlignment == ERootMotionTaskMoveToActorTargetOffsetType::AlignFromTargetToSource)
	{
		if (MovementComponent)
		{
			FVector ToSource = MovementComponent->GetActorLocation() - TargetActorLocation;
			ToSource.Z = 0.f;
			CalculatedTargetLocation += ToSource.ToOrientationQuat().RotateVector(TargetLocationOffset);
		}

	}
	else if (OffsetAlignment == ERootMotionTaskMoveToActorTargetOffsetType::AlignToTargetForward)
	{
		CalculatedTargetLocation += TargetActor->GetActorQuat().RotateVector(TargetLocationOffset);
	}
	else if (OffsetAlignment == ERootMotionTaskMoveToActorTargetOffsetType::AlignToWorldSpace)
	{
		CalculatedTargetLocation += TargetLocationOffset;
	}
	
	return CalculatedTargetLocation;
}
