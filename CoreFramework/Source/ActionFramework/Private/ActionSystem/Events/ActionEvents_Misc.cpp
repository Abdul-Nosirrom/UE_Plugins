// Copyright 2023 CoC All rights reserved


#include "ActionSystem/Events/ActionEvents_Misc.h"

#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "RadicalCharacter.h"
#include "TimerManager.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundBase.h"

/*--------------------------------------------------------------------------------------------------------------
* SFX
*--------------------------------------------------------------------------------------------------------------*/

void UActionEvent_PlaySFX::Initialize(UGameplayAction* InOwnerAction)
{
	Super::Initialize(InOwnerAction);

	
	if (PauseSFXOnEvent.IsValid()) PauseSFXOnEvent.GetEvent(OwnerAction.Get())->Event.AddUObject(this, &UActionEvent_PlaySFX::PauseSFX);
	if (UnpauseSFXOnEvent.IsValid()) UnpauseSFXOnEvent.GetEvent(OwnerAction.Get())->Event.AddUObject(this, &UActionEvent_PlaySFX::UnpauseSFX);
	OwnerAction->OnGameplayActionEnded.Event.AddUObject(this, &UActionEvent_PlaySFX::DestroySFX);

	for (auto& AdditionalEvent : AdditionalEventsToExecute)
	{
		if (AdditionalEvent.IsValid())
		{
			AdditionalEvent.GetEvent(OwnerAction.Get())->Event.AddUObject(this, &UActionEvent_PlaySFX::ExecuteEvent);
		}
	}
}

void UActionEvent_PlaySFX::ExecuteEvent()
{
	DestroySFX(); // For existing one that could be running
	
	if (bSound2D)
	{
		SpawnedSFX = UGameplayStatics::SpawnSound2D(this, SFX, VolumeMultiplier, PitchMultiplier, StartTime);
	}
	else
	{
		if (SpawnRules.SpawnType == EActionFXSpawnType::AtLocation)
		{
			const FVector Location = GetCharacterInfo()->CharacterOwner->GetActorLocation() + GetCharacterInfo()->CharacterOwner->GetActorRotation().RotateVector(SpawnRules.Location);
			SpawnedSFX = UGameplayStatics::SpawnSoundAtLocation(this, SFX, Location, FRotator::ZeroRotator, VolumeMultiplier, PitchMultiplier, StartTime);
		}
		else
		{			
			if (SpawnRules.AttachmentTarget == EActionEventFXAttachment::Capsule)
				SpawnedSFX = UGameplayStatics::SpawnSoundAttached(SFX, GetCharacterInfo()->CharacterOwner->GetCapsuleComponent(), NAME_None, SpawnRules.Location, FRotator::ZeroRotator, EAttachLocation::KeepRelativeOffset, true, VolumeMultiplier, PitchMultiplier, StartTime);
			else
				SpawnedSFX = UGameplayStatics::SpawnSoundAttached(SFX, GetCharacterInfo()->SkeletalMeshComponent.Get(), SpawnRules.AttachSocket, SpawnRules.Location, FRotator::ZeroRotator, EAttachLocation::KeepRelativeOffset, true, VolumeMultiplier, PitchMultiplier, StartTime);
		}
	}

	if (SpawnedSFX && Type == EActionFXLength::Looping)
		SpawnedSFX->bAutoDestroy = false;
}

void UActionEvent_PlaySFX::PauseSFX()
{
	if (SpawnedSFX && SpawnedSFX->IsPlaying()) SpawnedSFX->SetPaused(true);
}

void UActionEvent_PlaySFX::UnpauseSFX()
{
	if (SpawnedSFX && SpawnedSFX->bIsPaused) SpawnedSFX->SetPaused(false);
}

void UActionEvent_PlaySFX::DestroySFX()
{
	if (SpawnedSFX && Type == EActionFXLength::Looping)
	{
		SpawnedSFX->bAutoDestroy = true;
		SpawnedSFX->Deactivate();
	}
}

/*--------------------------------------------------------------------------------------------------------------
* VFX
*--------------------------------------------------------------------------------------------------------------*/


void UActionEvent_PlayVFX::Initialize(UGameplayAction* InOwnerAction)
{
	Super::Initialize(InOwnerAction);

	if (Type == EActionFXLength::Looping)
	{
		if (PauseVFXOnEvent.IsValid()) PauseVFXOnEvent.GetEvent(OwnerAction.Get())->Event.AddUObject(this, &UActionEvent_PlayVFX::DeactivateSpawnedFX);
		if (UnpauseVFXOnEvent.IsValid()) UnpauseVFXOnEvent.GetEvent(OwnerAction.Get())->Event.AddUObject(this, &UActionEvent_PlayVFX::ReactivateSpawnedFX);
	}
	else
	{
		if (DeactivationMethod == EActionEventVFXDeactivation::OnEvent && DeactivationEvent.IsValid())
		{
			DeactivationEvent.GetEvent(OwnerAction.Get())->Event.AddUObject(this, &UActionEvent_PlayVFX::DestroySpawnedFX);
		}
	}
	
	OwnerAction->OnGameplayActionEnded.Event.AddUObject(this, &UActionEvent_PlayVFX::DestroySpawnedFX);
}

void UActionEvent_PlayVFX::Cleanup()
{
	Super::Cleanup();
	GetWorld()->GetTimerManager().ClearTimer(DeactivationHandle);
}

void UActionEvent_PlayVFX::ExecuteEvent()
{
	const FVector Location = GetCharacterInfo()->CharacterOwner->GetActorLocation() + GetCharacterInfo()->CharacterOwner->GetActorRotation().RotateVector(SpawnRules.Location);
	DeactivateSpawnedFX(); // Check if we have one currently running, might be infinite so we wanna make sure we disable it before losing ref to it

	if (SpawnRules.SpawnType == EActionFXSpawnType::AtLocation)
	{
		SpawnedFX = UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, VFX, Location, SpawnRules.Rotation, SpawnRules.Scale, true, true);
	}
	else
	{
		if (SpawnRules.AttachmentTarget == EActionEventFXAttachment::Capsule)
			SpawnedFX = UNiagaraFunctionLibrary::SpawnSystemAttached(VFX, GetCharacterInfo()->CharacterOwner->GetCapsuleComponent(), NAME_None, SpawnRules.Location, SpawnRules.Rotation, SpawnRules.Scale, EAttachLocation::KeepRelativeOffset, false, ENCPoolMethod::None);
		else
			SpawnedFX = UNiagaraFunctionLibrary::SpawnSystemAttached(VFX, GetCharacterInfo()->SkeletalMeshComponent.Get(), SpawnRules.AttachSocket, SpawnRules.Location, SpawnRules.Rotation, SpawnRules.Scale, EAttachLocation::KeepRelativeOffset, false, ENCPoolMethod::None);
	}
	
	if (Type == EActionFXLength::OneShot && DeactivationMethod == EActionEventVFXDeactivation::AfterTime)
	{
		// Deactivation Delegate
		FTimerDelegate DeactivationDelegate;
		DeactivationDelegate.BindLambda([this]
		{
			DeactivateSpawnedFX();
		});
		GetCharacterInfo()->CharacterOwner->GetWorld()->GetTimerManager().SetTimer(DeactivationHandle, DeactivationDelegate, DeactivationTime, false);

		// Deparenting Delegate
		if (DeparentTime > 0.f)
		{
			FTimerDelegate DeparentDelegate;
			DeparentDelegate.BindLambda([this]
			{
				if (SpawnedFX)
				{
					SpawnedFX->DetachFromComponent(FDetachmentTransformRules(EDetachmentRule::KeepWorld, true));
				}
			});
			GetCharacterInfo()->CharacterOwner->GetWorld()->GetTimerManager().SetTimer(DeparentHandle, DeparentDelegate, DeparentTime, false);
		}
	}
}

void UActionEvent_PlayVFX::DestroySpawnedFX()
{
	if (SpawnedFX)
	{
		SpawnedFX->SetAutoDestroy(true);
		SpawnedFX->Deactivate();
	}
}

void UActionEvent_PlayVFX::ReactivateSpawnedFX()
{
	if (SpawnedFX) SpawnedFX->Activate(true);
}

void UActionEvent_PlayVFX::DeactivateSpawnedFX()
{
	if (SpawnedFX) SpawnedFX->Deactivate();
}


/*--------------------------------------------------------------------------------------------------------------*/

#if WITH_EDITOR 
void UActionEvent_PlaySFX::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (SFX)
	{
		Type = SFX->IsLooping() ? EActionFXLength::Looping : EActionFXLength::OneShot;
	}
}


FString UActionEvent_PlaySFX::GetEditorFriendlyName() const
{
	if (SFX)
	{
		return FString::Printf(TEXT("SFX [%s]"), *SFX->GetName());
	}
	
	return "SFX";
}

FString UActionEvent_PlayVFX::GetEditorFriendlyName() const
{
	EditorCacheSpawnType = SpawnRules.SpawnType;
	
	if (VFX)
	{
		return FString::Printf(TEXT("VFX [%s]"), *VFX->GetName());
	}
	
	return "VFX";
}
#endif