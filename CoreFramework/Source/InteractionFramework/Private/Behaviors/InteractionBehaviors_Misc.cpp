// Copyright 2023 CoC All rights reserved


#include "Behaviors/InteractionBehaviors_Misc.h"

#include "InteractionSystemDebug.h"
#include "LevelSequence.h"
#include "LevelSequencePlayer.h"
#include "NiagaraFunctionLibrary.h"
#include "RadicalCharacter.h"
#include "Components/InteractableComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/DataValidation.h"

void UInteractionBehavior_SpawnVFX::ExecuteInteraction(EInteractionDomain Domain, float DeltaTime)
{
	if (!SpawnLocation)
	{
		INTERACTABLE_VLOG(InteractableOwner->GetOwner(), Warning, "Spawn Location World Reference Null!");
		CompleteBehavior();
		return;
	}
	
	UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), VFX, SpawnLocation->GetActorLocation());
	CompleteBehavior();
}

void UInteractionBehavior_SpawnSFX::ExecuteInteraction(EInteractionDomain Domain, float DeltaTime)
{
	if (bSound2D)
	{
		UGameplayStatics::SpawnSound2D(GetWorld(), Sound, VolumeMultiplier);
	}
	else
	{
		if (!SoundLocation)
		{
			INTERACTABLE_VLOG(InteractableOwner->GetOwner(), Warning, "Sound Location World Reference Null!");
			CompleteBehavior();
			return;
		}
		
		UGameplayStatics::SpawnSoundAtLocation(GetWorld(), Sound, SoundLocation->GetActorLocation(), FRotator::ZeroRotator, VolumeMultiplier);
	}
	
	CompleteBehavior();
}

void UInteractionBehavior_FadeToBlack::ExecuteInteraction(EInteractionDomain Domain, float DeltaTime)
{
	if (bDoneExecuting) return;
	
	bDoneExecuting = true;
	
	UGameplayStatics::GetPlayerCameraManager(GetPlayerCharacter(), 0)->StartCameraFade(0.f, 1.f, FadeTime, FLinearColor::Black, true, true);

	FTimerDelegate FadeOutDelegate;
	FTimerHandle FadeOutHandle;

	FadeOutDelegate.BindLambda([this]()
	{
		UGameplayStatics::GetPlayerCameraManager(GetPlayerCharacter(), 0)->StartCameraFade(1.f, 0.f, FadeTime, FLinearColor::Black, true, false);
		CompleteBehavior();
	});

	if (bShouldTeleportPlayer)
	{
		FTimerDelegate TeleportDelegate;
		FTimerHandle TeleportHandle;

		TeleportDelegate.BindLambda([this]()
		{
			GetPlayerCharacter()->TeleportTo(PlayerTeleportTransform->GetActorLocation(), PlayerTeleportTransform->GetActorRotation());	
		});

		// teleport when fully faded out
		GetWorld()->GetTimerManager().SetTimer(TeleportHandle, TeleportDelegate, FadeTime, false);
	}

	GetWorld()->GetTimerManager().SetTimer(FadeOutHandle, FadeOutDelegate, FadeTime + HoldTime, false);
}

void UInteractionBehavior_LevelSequencer::Initialize(UInteractableComponent* OwnerInteractable)
{
	Super::Initialize(OwnerInteractable);

	if (Sequence)
		SequencePlayer = ULevelSequencePlayer::CreateLevelSequencePlayer(GetWorld(), Sequence, PlaybackSettings, SequenceActor);
}

void UInteractionBehavior_LevelSequencer::ExecuteInteraction(EInteractionDomain Domain, float DeltaTime)
{
	if (bDoneExecuting) return;

	bDoneExecuting = true;
	if (Sequence && SequencePlayer && !SequencePlayer->IsPlaying())
	{
		SequencePlayer->Play();
		SequencePlayer->OnFinished.AddDynamic(this, &UInteractionBehavior_LevelSequencer::OnSequenceComplete);

		if (!bWaitUntilComplete) CompleteBehavior();
	}
}

#if WITH_EDITOR 
#define LOCTEXT_NAMESPACE "InteractionBehaviorValidation"
EDataValidationResult UInteractionBehavior_LevelSequencer::IsBehaviorValid(EInteractionDomain InteractionDomain,
	FDataValidationContext& Context)
{
	EDataValidationResult Result = Super::IsBehaviorValid(InteractionDomain, Context);

	if (!Sequence)
	{
		FText CurrentError = LOCTEXT("SequenceNull","Referenced Level Sequence is null!");
		Context.AddError(CurrentError);
		Result =  EDataValidationResult::Invalid;
	}

	return Result;
}
#undef LOCTEXT_NAMESPACE
#endif