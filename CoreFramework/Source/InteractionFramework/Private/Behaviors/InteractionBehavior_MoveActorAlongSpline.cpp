// Copyright 2023 CoC All rights reserved


#include "Behaviors/InteractionBehavior_MoveActorAlongSpline.h"

#include "InteractionSystemDebug.h"
#include "Components/InteractableComponent.h"
#include "Components/SplineComponent.h"
#include "Misc/DataValidation.h"

void UInteractionBehavior_MoveActorAlongSpline::Initialize(UInteractableComponent* OwnerInteractable)
{
	Super::Initialize(OwnerInteractable);

	if (!SplineComponent)
	{
		SplineComponent = TargetSplineActor->FindComponentByClass<USplineComponent>();
		if (!SplineComponent)
		{
			INTERACTABLE_LOG(Warning, "No spline component found on target spline actor");
			return;
		}
	}
	if (!ActorToMove)
	{
		INTERACTABLE_LOG(Warning, "No actor to move component found for SplineMover behavior");
		return;
	}

	// Initialize actor to moves location to 0 on the spline
	FTransform StartTransform = SplineComponent->GetTransformAtTime(0.f, ESplineCoordinateSpace::World);
	StartTransform.SetScale3D(ActorToMove->GetActorScale3D());
	if (!bFollowRotation) StartTransform.SetRotation(ActorToMove->GetActorQuat());
	ActorToMove->SetActorTransform(StartTransform);
	CurrentDistance = 0.f;
	bGoingForward = true;

	OwnerInteractable->OnInteractionStartedEvent.AddDynamic(this, &UInteractionBehavior_MoveActorAlongSpline::OnInteractionStarted);
}

void UInteractionBehavior_MoveActorAlongSpline::ExecuteInteraction(EInteractionDomain Domain, float DeltaTime)
{
	if (!SplineComponent || !ActorToMove)
	{
		return;
	}

	const float CurrentTime = SplineComponent->GetTimeAtDistanceAlongSpline(CurrentDistance);
	const float CurrentSpeed = SpeedCurve.GetRichCurve()->Eval(CurrentTime);
	const float DirSign = bGoingForward ? 1.f : -1.f;

	CurrentDistance += CurrentSpeed * DirSign * DeltaTime;
	CurrentDistance = FMath::Clamp(CurrentDistance, 0.f, SplineComponent->GetSplineLength());
	
	FTransform CurrentTransform = SplineComponent->GetTransformAtDistanceAlongSpline(CurrentDistance, ESplineCoordinateSpace::World);
	CurrentTransform.SetScale3D(ActorToMove->GetActorScale3D());
	if (!bFollowRotation) CurrentTransform.SetRotation(ActorToMove->GetActorQuat());
	ActorToMove->SetActorTransform(CurrentTransform);

	if ((bGoingForward && CurrentTime >= 1.f) || (!bGoingForward && CurrentTime <= 0.f))
	{
		CompleteBehavior();
	}
}

#if WITH_EDITOR

TArray<AActor*> UInteractionBehavior_MoveActorAlongSpline::GetWorldReferences() const
{
	return {TargetSplineActor, ActorToMove};
}

#define LOCTEXT_NAMESPACE "InteractionBehaviorValidation"

EDataValidationResult UInteractionBehavior_MoveActorAlongSpline::IsBehaviorValid(EInteractionDomain InteractionDomain, FDataValidationContext& Context)
{
	EDataValidationResult Result = Super::IsBehaviorValid(InteractionDomain, Context);
	
	if (!ActorToMove || !TargetSplineActor)
	{
		FText CurrentError = FText::FormatOrdered(LOCTEXT("IsNull", "[{0}] One or more of referenced actors is null"), FText::FromString(StaticClass()->GetName()));
		Context.AddError(CurrentError);
		Result =  EDataValidationResult::Invalid;
		return Result;
	}

	if (!TargetSplineActor->FindComponentByClass<USplineComponent>())
	{
		FText CurrentError = FText::FormatOrdered(LOCTEXT("HasSpline", "[{0}] Target spline actor has no spline component"), FText::FromString(TargetSplineActor->GetName()));
		Context.AddError(CurrentError);
		Result =  EDataValidationResult::Invalid;
	}

	if (ActorToMove->GetRootComponent()->Mobility != EComponentMobility::Movable)
	{
		FText CurrentError = FText::FormatOrdered(LOCTEXT("IsMovable", "[{0}] Referenced actor to move needs to have mobility 'Movable'"), FText::FromString(ActorToMove->GetName()));
		Context.AddError(CurrentError);
		Result =  EDataValidationResult::Invalid;
	}
	
	return Result;
}
#undef LOCTEXT_NAMESPACE
#endif