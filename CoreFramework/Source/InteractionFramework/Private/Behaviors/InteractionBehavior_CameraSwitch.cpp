// Copyright 2023 CoC All rights reserved


#include "Behaviors/InteractionBehavior_CameraSwitch.h"

#include "InteractionSystemDebug.h"
#include "RadicalCharacter.h"
#include "Camera/CameraComponent.h"
#include "Components/InteractableComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/DataValidation.h"

void UInteractionBehavior_CameraSwitch::ExecuteInteraction(EInteractionDomain Domain, float DeltaTime)
{
	if (!CameraActor)
	{
		INTERACTABLE_VLOG(InteractableOwner->GetOwner(), Warning, "Failed to execute CameraSwitch behavior, no valid camera actor specified");
		CompleteBehavior();
		return;
	}

	if (bDoneExecuting) return;

	bDoneExecuting = true;

	if (SwitchDelay > 0.f)
	{
		FTimerDelegate SwapCameraDelegate;
		FTimerHandle SwapCameraHandle;
		SwapCameraDelegate.BindUObject(this, &UInteractionBehavior_CameraSwitch::SwapCamera, true);
		GetWorld()->GetTimerManager().SetTimer(SwapCameraHandle, SwapCameraDelegate, SwitchDelay, false);
	}
	else
	{
		SwapCamera(true);
	}

	if (HoldTime > 0)
	{
		const float TimeToSwapBack = BlendInParams.BlendTime + SwitchDelay + HoldTime;
		FTimerDelegate SwapCameraDelegate;
		FTimerHandle SwapCameraHandle;
		SwapCameraDelegate.BindUObject(this, &UInteractionBehavior_CameraSwitch::SwapCamera, false);
		GetWorld()->GetTimerManager().SetTimer(SwapCameraHandle, SwapCameraDelegate, TimeToSwapBack, false);
	}
}

void UInteractionBehavior_CameraSwitch::SwapCamera(bool bToInteractionCamera)
{
	AActor* ViewTarget = bToInteractionCamera ? CameraActor : GetPlayerCharacter();
	const FViewTargetTransitionParams& BlendInfo = bToInteractionCamera ? BlendInParams : BlendOutParams;
	auto PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
	PC->SetViewTarget(ViewTarget, BlendInfo);

	// Set to complete only when no hold (completes when we first swap), or if hold, when we begin to swap back
	if (HoldTime == 0.f || !bToInteractionCamera)
	{
		CompleteBehavior();
	}
}

void UInteractionBehavior_CameraSwitch::InteractionEnded()
{
	Super::InteractionEnded();

	if (HoldTime == 0.f)
	{
		SwapCamera(false);
	}
}

#if WITH_EDITOR

TArray<AActor*> UInteractionBehavior_CameraSwitch::GetWorldReferences() const
{
	return {CameraActor};
}

#define LOCTEXT_NAMESPACE "InteractionBehaviorValidation"
EDataValidationResult UInteractionBehavior_CameraSwitch::IsBehaviorValid(EInteractionDomain InteractionDomain, FDataValidationContext& Context)
{
	EDataValidationResult Result = Super::IsBehaviorValid(InteractionDomain, Context);

	
	if (!CameraActor)
	{
		FText CurrentError = LOCTEXT("IsNull", "No CameraActor specified");
		Context.AddError(CurrentError);
		Result =  EDataValidationResult::Invalid;
	}
	else if (!CameraActor->FindComponentByClass<UCameraComponent>())
	{
		FText CurrentError = FText::FormatOrdered(LOCTEXT("IsCamera","[{0}] Referenced camera actor has no camera"), FText::FromString(CameraActor->GetName()));
		Context.AddError(CurrentError);
		Result =  EDataValidationResult::Invalid;
	}

	return Result;
}
#undef LOCTEXT_NAMESPACE

#endif 