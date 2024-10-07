// Copyright 2023 CoC All rights reserved


#include "Behaviors/InteractionBehaviors_MoveAndRotation.h"

#include "InteractionSystemDebug.h"
#include "Components/InteractableComponent.h"
#include "Engine/TargetPoint.h"
#include "GameFramework/RotatingMovementComponent.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/DataValidation.h"

void UInteractionBehavior_RotateActorAdvanced::Initialize(UInteractableComponent* OwnerInteractable)
{
	Super::Initialize(OwnerInteractable);

	if (ActorToRotate && ActorToRotate->FindComponentByClass<URotatingMovementComponent>())
	{
		auto RotationMovementComp = ActorToRotate->FindComponentByClass<URotatingMovementComponent>();
		RotationMovementComp->RotationRate = FRotator::ZeroRotator;
		RotationMovementComp->PivotTranslation = LocalPivotLocation;
		RotationMovementComp->bRotationInLocalSpace = bRotationInLocalSpace;
	}

	ActivationCount = 0;
}

void UInteractionBehavior_RotateActorAdvanced::ExecuteInteraction(EInteractionDomain Domain, float DeltaTime)
{
	if (ActorToRotate && ActorToRotate->FindComponentByClass<URotatingMovementComponent>())
	{
		auto RotationMovementComp = ActorToRotate->FindComponentByClass<URotatingMovementComponent>();
		FRotator TargetRotationRate;

		if (RetriggerBehavior == EInteractRotateRetriggerBehavior::Stop)
		{
			TargetRotationRate = (ActivationCount % 2) * RotationRate;
		}
		else
		{
			TargetRotationRate = FMath::Sign(0.5f - ActivationCount % 2) * RotationRate;	
		}
		
		RotationMovementComp->RotationRate = TargetRotationRate;
	}

	CompleteBehavior();
}

void UInteractionBehavior_RotateActorSimple::Initialize(UInteractableComponent* OwnerInteractable)
{
	Super::Initialize(OwnerInteractable);

	if (ActorToRotate)
	{
		ActorToRotate->SetActorRotation(FromRotation);
	}
}

void UInteractionBehavior_RotateActorSimple::InteractionStarted()
{
	Super::InteractionStarted();

	if (!ActorToRotate) return;

	if (ActorToRotate->GetActorRotation().Equals(FromRotation, 0.1f))
	{
		TargetRotation = ToRotation;
	}
	else
	{
		TargetRotation = FromRotation;
	}

	CurrentTime = 0.f;
}

void UInteractionBehavior_RotateActorSimple::ExecuteInteraction(EInteractionDomain Domain, float DeltaTime)
{
	Super::ExecuteInteraction(Domain, DeltaTime);

	if (!ActorToRotate) return;

	const float NormalizedTime = CurrentTime/RotationTime;
	const FRotator InitRotation = FromRotation.Equals(TargetRotation, 0.1f) ? ToRotation : FromRotation;
	const FRotator CurrentRot = FMath::InterpEaseInOut(InitRotation, TargetRotation, NormalizedTime, 2.f);//FMath::RInterpTo(ActorToRotate->GetActorRotation(), TargetRotation, DeltaTime, RInterpSpeed);
	
	ActorToRotate->SetActorRotation(CurrentRot);
	CurrentTime += DeltaTime;
	
	if (CurrentRot.Equals(TargetRotation, 0.1f)) CompleteBehavior();
}

void UInteractionBehavior_MoveActorSimple::Initialize(UInteractableComponent* OwnerInteractable)
{
	Super::Initialize(OwnerInteractable);
	CurrentPointIdx = 0;
	bGoingForward = true; 
	bCompletedFullMove = false;

	if (ActorToMove && MoveToPoints.Num() > 0 && MoveToPoints[CurrentPointIdx].TargetLocation)
	{
		ActorToMove->SetActorLocation(MoveToPoints[CurrentPointIdx].TargetLocation->GetActorLocation());
		if (bFollowRotation)
		{
			ActorToMove->SetActorRotation(MoveToPoints[CurrentPointIdx].TargetLocation->GetActorRotation());
		}
	}
}

void UInteractionBehavior_MoveActorSimple::InteractionStarted()
{
	Super::InteractionStarted();
	
	bCompletedMovingToPoint = true;

	// Have we reached the last point and going forward?
	if (bGoingForward && CurrentPointIdx == MoveToPoints.Num()-1)
	{
		if (RetriggerBehavior == EInteractionMoveSimpleRetrigger::Reverse || RetriggerBehavior == EInteractionMoveSimpleRetrigger::IteratePointsAndReverse)
		{
			// Retrigger allows us to reverse
			bGoingForward = !bGoingForward;
			CurrentPointIdx--;
		}
	}
	// Have we reached the first point and going in reverse?
	else if (!bGoingForward && CurrentPointIdx == 0)
	{
		// This scope always means we've reversed
		bGoingForward = !bGoingForward;
		CurrentPointIdx++;
	}
	// Init first move
	else if (bGoingForward && CurrentPointIdx == 0) CurrentPointIdx = 1;
	// Iterate points for iterative retriggers
	else if (RetriggerBehavior == EInteractionMoveSimpleRetrigger::IteratePointsAndReverse || RetriggerBehavior == EInteractionMoveSimpleRetrigger::IteratePointsButOnce)
	{
		if (bGoingForward) CurrentPointIdx++;
		else CurrentPointIdx--;
	}

	// Ensure points in bound
	CurrentPointIdx = FMath::Clamp(CurrentPointIdx, 0, MoveToPoints.Num() - 1);
}

void UInteractionBehavior_MoveActorSimple::ExecuteInteraction(EInteractionDomain Domain, float DeltaTime)
{
	Super::ExecuteInteraction(Domain, DeltaTime);

	// Fail state
	if (!ActorToMove || MoveToPoints.Num() <= 1) { CompleteBehavior(); return; }

	// Check if we completed a DoOnce move
	if (bCompletedFullMove && (RetriggerBehavior == EInteractionMoveSimpleRetrigger::DoOnce || RetriggerBehavior == EInteractionMoveSimpleRetrigger::IteratePointsButOnce))
	{
		CompleteBehavior(); return;
	}
	
	// We're currently moving to some point
	if (!bCompletedMovingToPoint) return;

	// (Fail State) Check if there's a valid point we're going to
	if (!ensureMsgf(MoveToPoints[CurrentPointIdx].TargetLocation, TEXT("MoveActorSimple: Next Target Point Is Null!")))
	{
		// Broken event in this case
		INTERACTABLE_VLOG(InteractableOwner->GetOwner(), Error, "MoveActorSimple FAILED! Next Target Point Is Null");
		CompleteBehavior();
		return;
	}

	// Set up deferred move
	const FRotator TargetRotation = bFollowRotation ? MoveToPoints[CurrentPointIdx].TargetLocation->GetActorRotation() : ActorToMove->GetActorRotation();
	const FVector TargetLocation = MoveToPoints[CurrentPointIdx].TargetLocation->GetActorLocation();
	FLatentActionInfo LatentActionInfo;
	LatentActionInfo.CallbackTarget = ActorToMove;
	UKismetSystemLibrary::MoveComponentTo(ActorToMove->GetRootComponent(), TargetLocation, TargetRotation, MoveToPoints[CurrentPointIdx].bEaseOut, MoveToPoints[CurrentPointIdx].bEaseIn, MoveToPoints[CurrentPointIdx].MoveTime, true, EMoveComponentAction::Move, LatentActionInfo);
	bCompletedMovingToPoint = false;
	
	// Register callback for deferred move
	FTimerDelegate MoveDelegate;
	FTimerHandle MoveHandle;
	MoveDelegate.BindUObject(this, &UInteractionBehavior_MoveActorSimple::OnCompletedMoveToPoint);
	GetWorld()->GetTimerManager().SetTimer(MoveHandle, MoveDelegate, MoveToPoints[CurrentPointIdx].MoveTime, false);
}

void UInteractionBehavior_MoveActorSimple::OnCompletedMoveToPoint()
{
	bCompletedMovingToPoint = true;
	
	// If we've reached either end, we note that a full move was completed (applies to all cases)
	if ((bGoingForward && CurrentPointIdx == MoveToPoints.Num() - 1) || (!bGoingForward && CurrentPointIdx == 0))
	{
		bCompletedFullMove = true;
		CompleteBehavior();
		return;
	}

	if (RetriggerBehavior == EInteractionMoveSimpleRetrigger::IteratePointsAndReverse || RetriggerBehavior == EInteractionMoveSimpleRetrigger::IteratePointsButOnce)
	{
		// One move per interaction
		CompleteBehavior();
	}
	else
	{
		// Iterative retrigger iterates on the points during InteractionStart, so the iteration here is only for continous motions
		if (bGoingForward) CurrentPointIdx++;
		else CurrentPointIdx--;
	}
	
}


#if WITH_EDITOR 
#define LOCTEXT_NAMESPACE "InteractionBehaviorValidation"
EDataValidationResult UInteractionBehavior_RotateActorAdvanced::IsBehaviorValid(EInteractionDomain InteractionDomain,
	FDataValidationContext& Context)
{
	EDataValidationResult Result = Super::IsBehaviorValid(InteractionDomain, Context);
	if (Result == EDataValidationResult::Invalid) return Result;

	if (!ActorToRotate->FindComponentByClass<URotatingMovementComponent>())
	{
		FText CurrentError = FText::FormatOrdered(LOCTEXT("NoRotateComp", "[{0}] Actor Needs URotatingMovementComponent!"), FText::FromString(ActorToRotate->GetName()));
		Context.AddError(CurrentError);
		Result =  EDataValidationResult::Invalid;
	}

	return Result;
}

EDataValidationResult UInteractionBehavior_RotateActorSimple::IsBehaviorValid(EInteractionDomain InteractionDomain,
                                                                              FDataValidationContext& Context)
{
	EDataValidationResult Result = Super::IsBehaviorValid(InteractionDomain, Context);
	if (Result == EDataValidationResult::Invalid) return Result;
		
	if (ActorToRotate && !ActorToRotate->IsRootComponentMovable())
	{
		FText CurrentError = FText::FormatOrdered(LOCTEXT("MobilityInvalid", "[{0}] Actor Needs Mobility Set To Movable!"), FText::FromString(ActorToRotate->GetName()));
		Context.AddError(CurrentError);
		Result =  EDataValidationResult::Invalid;
	}

	return Result;
}

EDataValidationResult UInteractionBehavior_MoveActorSimple::IsBehaviorValid(EInteractionDomain InteractionDomain,
	FDataValidationContext& Context)
{
	EDataValidationResult Result = Super::IsBehaviorValid(InteractionDomain, Context);
	if (Result == EDataValidationResult::Invalid) return Result;
		
	if (ActorToMove && !ActorToMove->IsRootComponentMovable())
	{
		FText CurrentError = FText::FormatOrdered(LOCTEXT("MobilityInvalid", "[{0}] Actor Needs Mobility Set To Movable!"), FText::FromString(ActorToMove->GetName()));
		Context.AddError(CurrentError);
		Result =  EDataValidationResult::Invalid;
	}

	if (MoveToPoints.Num() <= 1)
	{
		FText CurrentError = LOCTEXT("NoPoints", "Not enough location markers! Require a minimum of 2");
		Context.AddError(CurrentError);
		Result =  EDataValidationResult::Invalid;
	}

	for (auto TargetPoint : MoveToPoints)
	{
		if (!TargetPoint.TargetLocation)
		{
			FText CurrentError = LOCTEXT("NullRef", "One of the target points null!");
			Context.AddError(CurrentError);
			Result =  EDataValidationResult::Invalid;
			break;
		}
	}

	return Result;
}
#undef LOCTEXT_NAMESPACE 
#endif 