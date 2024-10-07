// Copyright 2023 CoC All rights reserved


#include "Behaviors/InteractionBehavior_MovePlayerTo.h"

#include "RadicalCharacter.h"
#include "Blueprint/AIBlueprintHelperLibrary.h"
#include "Components/InteractableComponent.h"
#include "Kismet/GameplayStatics.h"


void UInteractionBehavior_MovePlayerTo::ExecuteInteraction(EInteractionDomain Domain, float DeltaTime)
{
	if (bDoneExecuting) return;
	
	bDoneExecuting = true;

	if (Delay > 0.f)
	{
		FTimerDelegate MoveDelegate;
		FTimerHandle MoveHandle;
		MoveDelegate.BindUObject(this, &UInteractionBehavior_MovePlayerTo::PerformMove);
		GetWorld()->GetTimerManager().SetTimer(MoveHandle, MoveDelegate, Delay, false);
	}
	else
	{
		PerformMove();
	}
}

void UInteractionBehavior_MovePlayerTo::InteractionEnded()
{
	Super::InteractionEnded();

	// Incase we haven't reached the destination or something happened, lets make sure we dont keep it hanging
	if (GetPlayerCharacter()->Controller)
		GetPlayerCharacter()->Controller->StopMovement();
}

void UInteractionBehavior_MovePlayerTo::PerformMove()
{
	UAIBlueprintHelperLibrary::SimpleMoveToActor(GetPlayerCharacter()->Controller, TargetLocation);
	
	// Block movement input until we reach the destination
	GetPlayerCharacter()->Controller->SetIgnoreMoveInput(true);
	

	FTimerDelegate Delegate;
	Delegate.BindLambda([this]
	{
		if (GetPlayerCharacter() && TargetLocation)
		{
			if (FVector::Dist2D(GetPlayerCharacter()->GetActorLocation(), TargetLocation->GetActorLocation()) <= AcceptanceRadius)
			{
				OnDestinationReached();
				GetWorld()->GetTimerManager().ClearTimer(ReachedLocationHandle);
			}
		}
		else
		{
			OnDestinationReached();
			GetWorld()->GetTimerManager().ClearTimer(ReachedLocationHandle);
		}
	});

	GetWorld()->GetTimerManager().SetTimer(ReachedLocationHandle, Delegate, 0.1f, true);
}

void UInteractionBehavior_MovePlayerTo::OnDestinationReached()
{
	if (TargetLocation)
	{
		GetPlayerCharacter()->SetActorRotation(TargetLocation->GetActorRotation());
	}

	GetPlayerCharacter()->Controller->ResetIgnoreMoveInput();
	CompleteBehavior();
}

void UInteractionBehavior_MovePlayerToAndPlayAnimation::OnDestinationReached()
{
	if (TargetLocation)
	{
		GetPlayerCharacter()->SetActorRotation(TargetLocation->GetActorRotation());
	}
	
	const float Duration = GetPlayerCharacter()->PlayAnimMontage(Animation, PlayRate);

	FTimerDelegate Delegate;
	Delegate.BindLambda([this]
	{
		Super::OnDestinationReached();
	});
	
	GetWorld()->GetTimerManager().SetTimer(AnimationFinishedHandle, Delegate, Duration, false);
}


