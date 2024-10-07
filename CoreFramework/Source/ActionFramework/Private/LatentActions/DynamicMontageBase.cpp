// Copyright 2023 CoC All rights reserved


#include "LatentActions/DynamicMontageBase.h"

#include "RadicalCharacter.h"
#include "RadicalMovementComponent.h"
#include "TimerManager.h"
#include "ActionSystem/ActionSystemInterface.h"
#include "Components/ActionSystemComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/PhysicsVolume.h"


UDynamicMontageBase* UDynamicMontageBase::PlayDynamicMontage(ARadicalCharacter* Character, FGameplayTagContainer CancelActionsWithTags, FGameplayTagContainer BlockActionsWithTag,
                                                             FGameplayTagContainer ActionsThatCanCancel, UAnimMontage* Montage, float& MontageLength,
                                                             float CanCancelAfterSeconds, float PlayRate, EDynamicMontageVelocity VelMethod,
                                                             FRuntimeVectorCurve VectorCurve, EDynamicMontageVectorBasis VectorBasis, bool bIsAdditive,
                                                             bool bApplyGravity, float GravityScale,
                                                             EDynamicMontageRotation RotMethod, FRuntimeVectorCurve RotationCurve, float RotationRate, bool bBlendMovement, bool bUnbindMovementOnBlendOut)
{
	auto Node = NewObject<UDynamicMontageBase>();
	if (Node)
	{
		MontageLength = (Montage->GetPlayLength()/Montage->RateScale) / PlayRate;
		Node->CharContext = Character;
		Node->CancelledActions = CancelActionsWithTags;
		Node->BlockedActions = BlockActionsWithTag;
		Node->CancelWhenTagsGranted = ActionsThatCanCancel;
		Node->Montage = Montage;
		Node->PlayRate = PlayRate;
		Node->VelMethod = VelMethod;
		Node->VelMethodCurve = VectorCurve;
		Node->VectorBasis = VectorBasis;
		Node->bIsAdditive = bIsAdditive;
		Node->RotMethod = RotMethod;
		Node->RotationRate = RotationRate;
		Node->bBlendMovement = bBlendMovement;
		Node->bBlendingOut = false;
		Node->bCanCancel = false;
		Node->RotMethodCurve = RotationCurve;
		Node->CanCancelAfterSeconds = CanCancelAfterSeconds;
		Node->bApplyGravity = bApplyGravity;
		Node->GravityScale = GravityScale;
		Node->bUnbindMovementOnBlendOut = bUnbindMovementOnBlendOut;
	}
	return Node;
}

UDynamicMontageBase* UDynamicMontageBase::PlayDynamicMontageAction(ARadicalCharacter* Character,
	FGameplayTagContainer CancelActionsWithTags, FGameplayTagContainer BlockActionsWithTag,
	FGameplayTagContainer ActionsThatCanCancel, UAnimMontage* Montage, float& MontageLength,
	FDynamicMontageCustomMovementCalculations& CustomCalculations, FDynamicMontageCustomVel CustomVelDelegate,
	FDynamicMontageCustomRotation CustomRotDelegate, float CanCancelAfterSeconds, float PlayRate,
	ERotationMethod RotMethod, bool bBlendMovement)
{
	auto Node = NewObject<UDynamicMontageBase>();
	if (Node)
	{
		MontageLength = (Montage->GetPlayLength()/Montage->RateScale) / PlayRate;
		CustomCalculations = Node->DynamicCalculations;
		Node->CharContext = Character;
		Node->CancelledActions = CancelActionsWithTags;
		Node->BlockedActions = BlockActionsWithTag;
		Node->CancelWhenTagsGranted = ActionsThatCanCancel;
		Node->Montage = Montage;
		Node->PlayRate = PlayRate;
		Node->VelMethod = EDynamicMontageVelocity::Custom;
		Node->CustomVelDelegate = CustomVelDelegate;
		Node->CustomRotDelegate = CustomRotDelegate;
		Node->RotMethod = Node->TranslateDynamicRotationMethod(RotMethod);
		Node->bBlendMovement = bBlendMovement;
		Node->bBlendingOut = false;
		Node->bCanCancel = false;
		Node->CanCancelAfterSeconds = CanCancelAfterSeconds;
	}
	return Node;
}

void UDynamicMontageBase::Activate()
{
	if (!(CharContext && CharContext->GetMesh()))
	{
		Failed(TEXT("Invalid Character Or Mesh"));
		return;
	}

	// Play Montage (Hope it calls BlendOut if a prev one was playing, and we can rebind to its events later)
	UAnimInstance* AnimInstance = CharContext->GetMesh()->GetAnimInstance();
	
	MontageLength = (AnimInstance->Montage_Play(Montage, PlayRate) / Montage->RateScale) / PlayRate;

	if (MontageLength <= 0.f)
	{
		Failed(TEXT("Bad Montage"));
		return;
	}

	StartingRotation = CharContext->GetActorRotation();

	// Set action system cancelling of this dynamic montage if available
	if (auto IAS = Cast<IActionSystemInterface>(CharContext))
	{
		if (auto AS = IAS->GetActionSystem())
		{
			AS->BlockActionsWithTags(BlockedActions);
			AS->CancelActionsWithTags(CancelledActions);

			// Bind to possibly cancel
			AS->OnGameplayTagAddedDelegate.AddDynamic(this, &UDynamicMontageBase::OnGameplayTagsAdded);
			
			if (CanCancelAfterSeconds > 0.f && CanCancelAfterSeconds <= MontageLength)
			{
				AS->BlockActionsWithTags(CancelWhenTagsGranted);
				CancelTimerDelegate.BindLambda([this]
				{
					bCanCancel = true;
					if (auto IAS = Cast<IActionSystemInterface>(this->CharContext))
					{
						if (auto AS = IAS->GetActionSystem())
						{
							AS->UnBlockActionsWithTags(this->CancelWhenTagsGranted);
						}
					}
				});
				CharContext->GetWorld()->GetTimerManager().SetTimer(CancelTimerHandle, CancelTimerDelegate, CanCancelAfterSeconds, false);
			}
			else
			{
				bCanCancel = true;
			}
		}
	}
	else bCanCancel = true;
	
	// Bind to montage events
	//AnimInstance->OnMontageStarted NOTE: Can use this to check if another montage is being started and reset events/cancel out of the movement
	AnimInstance->OnMontageBlendingOut.AddDynamic(this, &UDynamicMontageBase::OnMontageBlendingOut);
	AnimInstance->OnMontageEnded.AddDynamic(this, &UDynamicMontageBase::OnMontageCompleted);
	
	// Bind as secondary
	if (VelMethod != EDynamicMontageVelocity::None)
	{
		CharContext->GetCharacterMovement()->RequestSecondaryVelocityBinding().BindUObject(this, &UDynamicMontageBase::CalcRMVelocity);
	}
	if (RotMethod != EDynamicMontageRotation::None)
	{
		CharContext->GetCharacterMovement()->RequestSecondaryRotationBinding().BindUObject(this, &UDynamicMontageBase::UpdateRMRotation);
	}
	
	// Set start time & start vel
	TimeStarted = CharContext->GetWorld()->GetTimeSeconds();
	VelocityOnStart = CharContext->GetVelocity();
}

void UDynamicMontageBase::OnMontageBlendingOut(UAnimMontage* AnimMontage, bool bInterrupted)
{
	if (bInterrupted)
	{
		// Perform cleanup to rebind to whatever was previously bound, then we exit
		PerformCleanup();
		OnInterrupted.Broadcast(FGameplayTagContainer());
		return;
	}

	// Notify we started blending out, in case we need to blend movement stuff
	bBlendingOut = true;

	if (bUnbindMovementOnBlendOut)
	{
		PerformCleanup();
	}
}

void UDynamicMontageBase::OnMontageCompleted(UAnimMontage* AnimMontage, bool bInterrupted)
{
	PerformCleanup();
}
void UDynamicMontageBase::CalcRMVelocity(URadicalMovementComponent* MovementComponent, float DeltaTime)
{
	// NOTE: instead of this, add a "CancelOnTagAdded" param, these are technically gonna be dynamic actions so it makes sense
	// Check if the montage is currently being played, otherwise we cancel out (slot changes etc...)

	// RootMotionVelocity is just a ref pass of MovementComponent->Velocity (same in Rot)
	
	FVector CalculatedVel = MovementComponent->Velocity;

	FVector RawCurveEval = VelMethodCurve.GetValue(GetTimeSinceStarted()/MontageLength);
	FVector CurveEval = RawCurveEval;
	
	// TODO: Need to figure out rotation stuff. World, Player Basis, Player Planar Basis (Z is always up)
	auto PlayerRotation = MovementComponent->UpdatedComponent->GetComponentRotation();
	switch (VectorBasis)
	{
		case EDynamicMontageVectorBasis::Player: 
			CurveEval = PlayerRotation.RotateVector(RawCurveEval);
			break;
		case EDynamicMontageVectorBasis::PlayerZUp:
			PlayerRotation.Pitch = PlayerRotation.Roll = 0.f;
			CurveEval = PlayerRotation.RotateVector(RawCurveEval);
			break;
	}

	switch (VelMethod)
	{
		case EDynamicMontageVelocity::Default:
			if (MovementComponent->GetMovementData())
			{
				MovementComponent->GetMovementData()->CalculateVelocity(MovementComponent, DeltaTime);
				CalculatedVel = MovementComponent->GetVelocity();
			}
			break;
		case EDynamicMontageVelocity::Custom:
			CustomVelDelegate.Execute(CalculatedVel, DeltaTime, GetTimeSinceStarted()/MontageLength);
			CalculatedVel = DynamicCalculations.Velocity;
			break;
		case EDynamicMontageVelocity::AccelCurve:
			CalculatedVel += CurveEval * DeltaTime;
			break;
		case EDynamicMontageVelocity::VelCurve:
			if (bIsAdditive)
			{
				CalculatedVel += CurveEval;
			}
			else
			{
				CalculatedVel = CurveEval;
			}
			break;
	}

	if (MovementComponent->IsInAir() && bApplyGravity) // Apply gravity
	{
		TGuardValue<float> RestoreGravScale(MovementComponent->GravityScale, MovementComponent->GravityScale * GravityScale);
		MovementComponent->GetMovementData()->ApplyGravity(MovementComponent, CalculatedVel, MovementComponent->GetPhysicsVolume()->TerminalVelocity, DeltaTime);
	}
	else if (MovementComponent->IsMovingOnGround())
	{
		// Check velocity delta this frame, if not additive check velocity this frame
		const FVector VelDir = RawCurveEval; //bIsAdditive ? CurveEval : CalculatedVel;
		bool bLaunchInAir = (VelDir | MovementComponent->GetGravityDir()) < 0.f;
		if (bLaunchInAir) MovementComponent->SetMovementState(STATE_Falling);
	}

	// Blend the vels if necessary
	MovementComponent->Velocity = FMath::VInterpTo(VelocityOnStart, CalculatedVel, bBlendMovement ? GetTimeSinceStarted()/MontageLength : 1.f , 1.f);
}

void UDynamicMontageBase::UpdateRMRotation(URadicalMovementComponent* MovementComponent, float DeltaTime)
{
	FQuat Rot = MovementComponent->UpdatedComponent->GetComponentQuat();
	if (RotMethod == EDynamicMontageRotation::Custom)
	{
		CustomRotDelegate.Execute(Rot.Rotator(), DeltaTime, GetTimeSinceStarted()/MontageLength);
		Rot = DynamicCalculations.Rotation;
	}
	else if (RotMethod == EDynamicMontageRotation::RotationCurve)
	{
		const FVector CurveEval = RotMethodCurve.GetValue(GetTimeSinceStarted()/MontageLength);
		Rot = (StartingRotation + FRotator(CurveEval.X, CurveEval.Y, CurveEval.Z)).Quaternion();
		MovementComponent->MoveUpdatedComponent(FVector::ZeroVector, Rot, false);
	}
	else
	{
		TGuardValue<TEnumAsByte<ERotationMethod>> RestoreGRotMethod(MovementComponent->GetMovementData()->GroundRotationMethod, TranslateRotationMethod());
		TGuardValue<TEnumAsByte<ERotationMethod>> RestoreARotMethod(MovementComponent->GetMovementData()->AirRotationMethod, TranslateRotationMethod());
		TGuardValue<float> RestoreRotationRate(MovementComponent->GetMovementData()->RotationRate, RotationRate);
		MovementComponent->GetMovementData()->UpdateRotation(MovementComponent, DeltaTime);
		Rot = MovementComponent->UpdatedComponent->GetComponentQuat();
	}
	MovementComponent->MoveUpdatedComponent(FVector::ZeroVector, Rot, false);
}

void UDynamicMontageBase::OnGameplayTagsAdded(FGameplayTagContainer TagsAdded)
{
	if (CancelWhenTagsGranted.HasAny(TagsAdded))
	{
		// Stop montage
		CharContext->GetMesh()->GetAnimInstance()->Montage_Stop(Montage->BlendOut.GetBlendTime(), Montage);
		OnInterrupted.Broadcast(CancelWhenTagsGranted.Filter(TagsAdded));
		PerformCleanup();
	}
}

float UDynamicMontageBase::GetTimeSinceStarted()
{
	return CharContext->GetWorld()->GetTimeSeconds() - TimeStarted;
}

void UDynamicMontageBase::PerformCleanup()
{
	UnbindEvents();

	// Unblock actions & unbind
	if (auto IAS = Cast<IActionSystemInterface>(CharContext))
	{
		auto AS = IAS->GetActionSystem();
		if (AS)
		{
			AS->UnBlockActionsWithTags(BlockedActions);
			AS->OnGameplayTagAddedDelegate.RemoveDynamic(this, &UDynamicMontageBase::OnGameplayTagsAdded);
		}
	}

	// Check cancellation timer, force execute it to unblock actions if its running
	if (!bCanCancel && CancelTimerHandle.IsValid() && CancelTimerDelegate.IsBound())
	{
		CharContext->GetWorld()->GetTimerManager().ClearTimer(CancelTimerHandle);
		CancelTimerHandle.Invalidate();
		CancelTimerDelegate.Execute();
	}
	
	// Invoke completion
	OnComplete.Broadcast();
}

void UDynamicMontageBase::UnbindEvents()
{
	// Unbind
	const auto AnimInstance = CharContext->GetMesh()->GetAnimInstance();
	AnimInstance->OnMontageBlendingOut.RemoveDynamic(this, &UDynamicMontageBase::OnMontageBlendingOut);
	AnimInstance->OnMontageEnded.RemoveDynamic(this, &UDynamicMontageBase::OnMontageCompleted);

	CharContext->GetCharacterMovement()->UnbindSecondaryVelocity(this);
	CharContext->GetCharacterMovement()->UnbindSecondaryRotation(this);
}

void UDynamicMontageBase::Failed(const TCHAR* Reason)
{
	FFrame::KismetExecutionMessage(Reason, ELogVerbosity::Error);
}
