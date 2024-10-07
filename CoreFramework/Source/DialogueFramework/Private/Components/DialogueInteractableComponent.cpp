// Fill out your copyright notice in the Description page of Project Settings.


#include "Components/DialogueInteractableComponent.h"

#include "FlowSubsystem.h"
#include "InteractionSystemDebug.h"
#include "LevelSequenceActor.h"
#include "LevelSequenceDirector.h"
#include "LevelSequencePlayer.h"
#include "RadicalCharacter.h"
#include "Blueprint/UserWidget.h"
#include "Components/ActionSystemComponent.h"
#include "CoreFramework/Public/Components/RadicalMovementComponent.h"
#include "CoreFramework/Public/DataAssets/MovementData.h"
#include "FlowDialogue/DialogueAsset.h"
#include "FlowDialogue/DialogueChoice.h"
#include "FlowDialogue/DialogueLine.h"
#include "Interfaces/DialogueControllerInterface.h"
#include "Kismet/KismetMathLibrary.h"


UE_DEFINE_GAMEPLAY_TAG(TAG_Interaction_Dialogue, "Interaction.Dialogue")

// Sets default values for this component's properties
UDialogueInteractableComponent::UDialogueInteractableComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;
}

TArray<AActor*> UDialogueInteractableComponent::RetrieveEventActors() const
{
	TArray<AActor*> OutActorArray;

	for (auto EventKey : EventWorldReferences)
	{
		OutActorArray.Add(EventKey.Value);
	}

	return OutActorArray;
}

// Called when the game starts
void UDialogueInteractableComponent::BeginPlay()
{
	Super::BeginPlay();

	// Bind to interaction events
	OnInteractionStartedEvent.AddDynamic(this, &UDialogueInteractableComponent::StartDialogue);
	OnInteractionUpdateEvent.AddDynamic(this, &UDialogueInteractableComponent::UpdateDialogue);
	OnInteractionEndedEvent.AddDynamic(this, &UDialogueInteractableComponent::EndDialogue);

	CalcPlayerVelocityEvent.AddDynamic(this, &UDialogueInteractableComponent::CalcVelocity);
	UpdatePlayerRotationEvent.AddDynamic(this, &UDialogueInteractableComponent::UpdatePlayerRotation);
	
	// NOTE: Having the interactable tag being granted can be useful for queries. E.g enemies can check if Character Has Tag(Interactable.Dialogue)
	// and if they do, be not aggressive as to not interrupt / they "pause"
	//InteractableTag = TAG_Interaction_Dialogue;
}


void UDialogueInteractableComponent::RegisterDialogueChoice(FDialogueEvent& ChoiceSelectedEvent, const TArray<FText>& Choices)
{
	if (GetDialogueUI())
	{
		DialogueUI->SetRenderOpacity(1.f); // Reset if disabled by HideUI
		ActiveChoices = Choices;
		OnDialogueChoiceSelectedEvent = ChoiceSelectedEvent;
		DialogueController->OnDialoguePayloadReceived(this, FDialogueControllerPayload(ActiveChoices));
	}
}

void UDialogueInteractableComponent::UnregisterDialogueChoice()
{
	OnDialogueChoiceSelectedEvent = FDialogueEvent();
}

void UDialogueInteractableComponent::RegisterDialogueLine(FDialogueEvent& CompletedEvent, const FText& Speaker, const TArray<FText>& Lines)
{
	if (GetDialogueUI())
	{
		DialogueUI->SetRenderOpacity(1.f); // Reset if disabled by HideUI
		OnDialogueLinesCompleted = CompletedEvent;
		ActiveDialogueLines = Lines;
		ActiveSpeaker = Speaker;
		ActiveLineIdx = 0;
		OnDialogueLineDisplayCompleted();
	}
}

void UDialogueInteractableComponent::UnregisterDialogueLine()
{
	OnDialogueLinesCompleted = FDialogueEvent();
}

void UDialogueInteractableComponent::HideDialogueUI()
{
	if (GetDialogueUI())
	{
		DialogueUI->SetRenderOpacity(0.f);
	}
}

void UDialogueInteractableComponent::OnDialogueLineDisplayCompleted()
{
	if (!ensureMsgf(OnDialogueLinesCompleted.IsBound(), TEXT("Attempting to display dialogue line, but dialogue line node is null!")))
	{
		return;
	}
	
	// Check line stack
	if (ActiveLineIdx < ActiveDialogueLines.Num())
	{
		DialogueController->OnDialoguePayloadReceived(this, FDialogueControllerPayload(ActiveSpeaker, ActiveDialogueLines[ActiveLineIdx]));
		ActiveLineIdx++;
	}
	// Line stack complete
	else
	{
		OnDialogueLinesCompleted.Execute(-1);
	}
}

void UDialogueInteractableComponent::OnDialogueChoiceSelected(int ChoiceIdx)
{
	if (OnDialogueChoiceSelectedEvent.IsBound())
	{
		OnDialogueChoiceSelectedEvent.Execute(ChoiceIdx);
	}
}


UFlowSubsystem* UDialogueInteractableComponent::GetFlowSubsystem() const
{
	if (GetWorld() && GetWorld()->GetGameInstance())
	{
		if (UFlowSubsystem* Flow = GetWorld()->GetGameInstance()->GetSubsystem<UFlowSubsystem>())
		{
			return Flow;
		}
	}

	return nullptr;
}

void UDialogueInteractableComponent::StartDialogue()
{
	DialogueController = Cast<IDialogueControllerInterface>(PlayerCharacter->GetController());

	// Create and display UI
	if (ensureMsgf(DialogueController && DialogueController->OnDialoguePayloadReceived(this, FDialogueControllerPayload::Init),
		TEXT("Interacting Controller Doesn't Implement IDialogueControllerInterface!")))
	{
		// Start Dialogue
		if (DialogueInstance) // We've already instanced the asset
		{
			DialogueInstance->StartFlow();
		}
		else if (UFlowSubsystem* Flow = GetFlowSubsystem()) // No cached asset, need to create it & start it
		{
			Flow->StartRootFlow(this, DialogueAsset, false);
			DialogueInstance = Cast<UDialogueAsset>(Flow->GetRootFlow(this));
			if (DialogueInstance)
			{
				DialogueInstance->OnDialogueGraphCompleted.AddLambda([this] { StopInteraction(); });
			}
		}
	}
	else
	{
		StopInteraction();
		INTERACTABLE_VLOG(GetOwner(), Error, "Dialogue Initialization Failed! No valid interacting controller.")
	}
}

void UDialogueInteractableComponent::UpdateDialogue(float DeltaTime)
{
	// Send over info from the dialogue to the UI, it'll handle the rest in terms of updating the state machine
}

void UDialogueInteractableComponent::EndDialogue()
{
	if (!GetDialogueUI())
	{
		return; // We never started to begin with
	}
	
	DialogueController->OnDialoguePayloadReceived(this, FDialogueControllerPayload::Deinit);

	// Stop all montages that may have been playing
	ActionSystemComponent->GetCharacterOwner()->GetMesh()->GetAnimInstance()->StopAllMontages(0.25f);

	// Reset view target in case it was set during dialogue
	if (auto PC = Cast<APlayerController>(PlayerCharacter->GetController()))
	{
		PC->SetViewTargetWithBlend(PlayerCharacter.Get(), 0.5f, VTBlend_EaseIn, 1.f);
	}
}

#pragma region Misc Physics
void UDialogueInteractableComponent::CalcVelocity(URadicalMovementComponent* MovementComponent, float DeltaTime)
{
	TGuardValue<float> RestoreMaxSpeed(MovementComponent->GetMovementData()->TopSpeed, 600.f);
	MovementComponent->GetMovementData()->CalculateVelocity(MovementComponent, DeltaTime);
}

void UDialogueInteractableComponent::UpdatePlayerRotation(URadicalMovementComponent* MovementComponent, float DeltaTime)
{
	if (MovementComponent->Velocity.IsZero())
	{
		// Face us
		const FVector TargetForward = (SpeakerLocation + GetComponentLocation() - MovementComponent->GetActorLocation()).GetSafeNormal2D();
		const FVector TargetUp = FVector::UpVector;
		FQuat TargetRot = UKismetMathLibrary::MakeRotFromZX(TargetUp, TargetForward).Quaternion();
		TargetRot = FQuat::Slerp(MovementComponent->UpdatedComponent->GetComponentQuat(), TargetRot, DeltaTime * 5.f);
		MovementComponent->MoveUpdatedComponent(FVector::ZeroVector, TargetRot, false);
	}
	else
	{
		MovementComponent->GetMovementData()->UpdateRotation(MovementComponent, DeltaTime);
	}
}
#pragma endregion

#if WITH_EDITOR

void UDialogueInteractableComponent::ValidateEvents()
{
	/*
	if (!DialogueTreeType) return;
	
	const auto DummyInstance = USMBlueprintUtils::CreateStateMachineInstance(DialogueTreeType, this);
	TArray<USMStateInstance_Base*> AllStates;
	DummyInstance->GetAllStateInstances(AllStates);

	TArray<FName> AllEventNames;
	
	for (auto StateNode : AllStates)
	{
		if (const auto DialogueNode = Cast<UDialogueTextNode>(StateNode))
		{
			DialogueNode->EvaluateGraphProperties();
			for (auto Event : DialogueNode->GetAllEvents())
			{
				if (AllEventNames.Contains(Event)) continue;
				AllEventNames.Add(Event);
			}
		}
	}

	// Add the events
	for (auto StoredEvent : AllEventNames)
	{
		if (EventWorldReferences.Contains(StoredEvent)) continue;
		EventWorldReferences.Add(StoredEvent);
	}
		
	// Remove irrelevant keys
	for (auto EventKey : AllEventNames)
	{
		if (EventWorldReferences.Contains(EventKey)) continue;
			EventWorldReferences.Remove(EventKey);
	}
	*/
}



#endif 

void ADialogueSequencerManager::PlaySequencerDialogueLine(ULevelSequenceDirector* SequencePlayer, const FText& Speaker,
	const FText& Lines, float PauseAfterTime)
{
	bDialogueLineCompleted = false;
	
	OnDialogueLineDisplayed.BindLambda([this, SequencePlayer](int32 Payload)
	{
		if (SequencePlayer && SequencePlayer->Player && SequencePlayer->Player->IsPaused()) SequencePlayer->Player->Play();
		auto DialogueComp = DialogueActor->FindComponentByClass<UDialogueInteractableComponent>();
		DialogueComp->HideDialogueUI();
		TimerHandle.Invalidate();
		OnDialogueLineDisplayed.Unbind();
		bDialogueLineCompleted = true;
	});
	
	TArray<FString> LinesArrayString;
	Lines.ToString().ParseIntoArrayLines(LinesArrayString);
	TArray<FText> LinesArray; for (auto line : LinesArrayString) LinesArray.Add(FText::FromString(line));
	
	if (DialogueActor)
	{
		auto DialogueComp = DialogueActor->FindComponentByClass<UDialogueInteractableComponent>();
		if (DialogueComp)
		{
			DialogueComp->RegisterDialogueLine(OnDialogueLineDisplayed, Speaker, LinesArray);
		}
	}
	
	if (PauseAfterTime == 0.f && SequencePlayer && SequencePlayer->Player)
		SequencePlayer->Player->Pause();
	else if (PauseAfterTime > 0.f && SequencePlayer && SequencePlayer->Player)
	{
		FTimerDelegate TimerDelegate;
		TimerDelegate.BindLambda([this, SequencePlayer]()
		{
			if (!bDialogueLineCompleted) // For cases where we skip a line before the pause time, we should know about it. Otherwise we soft-lock
				SequencePlayer->Player->Pause();
		});
		GetWorld()->GetTimerManager().SetTimer(TimerHandle, TimerDelegate, PauseAfterTime, false);
	}
}

void ADialogueSequencerManager::SequencerDialogueLine(const FString& Speaker, const FString& Lines, float PauseAfterTime)
{
	bDialogueLineCompleted = false;
	
	OnDialogueLineDisplayed.BindLambda([this](int32 Payload)
	{
		if (DlgSequencerPlayer && DlgSequencerPlayer->IsPaused()) DlgSequencerPlayer->Play();
		auto DialogueComp = DialogueActor->FindComponentByClass<UDialogueInteractableComponent>();
		DialogueComp->HideDialogueUI();
		TimerHandle.Invalidate();
		OnDialogueLineDisplayed.Unbind();
		bDialogueLineCompleted = true;
	});
	
	TArray<FString> LinesArrayString;
	Lines.ParseIntoArray(LinesArrayString, TEXT("/"));
	TArray<FText> LinesArray; for (auto line : LinesArrayString) LinesArray.Add(FText::FromString(line));
	
	if (DialogueActor)
	{
		auto DialogueComp = DialogueActor->FindComponentByClass<UDialogueInteractableComponent>();
		if (DialogueComp)
		{
			DlgSequencerPlayer = DialogueComp->GetActiveSequencer();
			DialogueComp->RegisterDialogueLine(OnDialogueLineDisplayed, FText::FromString(Speaker), LinesArray);
		}
	}

	
	if (PauseAfterTime == 0.f && DlgSequencerPlayer)
		DlgSequencerPlayer->Pause();
	else if (PauseAfterTime > 0.f && DlgSequencerPlayer)
	{
		FTimerDelegate TimerDelegate;
		TimerDelegate.BindLambda([this]()
		{
			if (!bDialogueLineCompleted && DlgSequencerPlayer) // For cases where we skip a line before the pause time, we should know about it. Otherwise we soft-lock
				DlgSequencerPlayer->Pause();
		});
		GetWorld()->GetTimerManager().SetTimer(TimerHandle, TimerDelegate, PauseAfterTime, false);
	}
}
