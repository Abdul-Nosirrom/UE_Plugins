// Fill out your copyright notice in the Description page of Project Settings.


#include "Components/InteractableComponent.h"

#include "ImageUtils.h"
#include "TimerManager.h"
#include "Components/ActionSystemComponent.h"
#include "RadicalCharacter.h"
#include "InputBufferSubsystem.h"
#include "InteractionSystemDebug.h"
#include "Components/BillboardComponent.h"
#include "Interfaces/IPluginManager.h"
#include "Kismet/GameplayStatics.h"

#include "Behaviors/InteractionBehavior.h"
#include "Conditions/InteractionCondition.h"
#include "Engine/TargetPoint.h"
#include "Misc/DataValidation.h"
#include "Subsystems/InteractionManager.h"
#include "Triggers/InteractionTrigger.h"
#include "Triggers/InteractionTrigger_Overlap.h"

UE_DEFINE_GAMEPLAY_TAG(TAG_Interaction_Base, "Interaction")

namespace InteractCVars
{
#if ALLOW_CONSOLE && !NO_LOGGING
	int32 VisualizeInteractionState = 0;
	FAutoConsoleVariableRef CVarShowAttributeStates
	(
		TEXT("interaction.DisplayStates"),
		VisualizeInteractionState,
		TEXT("Display States. 0: Disable, 1: Enable"),
		ECVF_Default
	);
#endif 
}

DECLARE_CYCLE_STAT(TEXT("Tick Interactables"), STAT_TickInteractable, STATGROUP_InteractionSystem)
DECLARE_CYCLE_STAT(TEXT("Initialize Interactables"), STAT_InitInteractable, STATGROUP_InteractionSystem)
DECLARE_CYCLE_STAT(TEXT("Interactables Conditions"), STAT_CondiInteractable, STATGROUP_InteractionSystem)

/*--------------------------------------------------------------------------------------------------------------
* Core Setup
*--------------------------------------------------------------------------------------------------------------*/


// Sets default values for this component's properties
UInteractableComponent::UInteractableComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	bAllowStopExternal = true;

	StopTrigger = EInteractionStopTrigger::Automatic;

#if WITH_EDITORONLY_DATA
	static FString ContentDir = IPluginManager::Get().FindPlugin(TEXT("CoreFramework"))->GetBaseDir() / TEXT("Resources");
	static FString IconDir = (ContentDir / TEXT("InteractableIcon")) + TEXT(".png");
	const auto Icon = FImageUtils::ImportFileAsTexture2D(IconDir);
	Sprite = Icon;
	bIsScreenSizeScaled = true;
	ScreenSize = -0.00025;
	bUseInEditorScaling = false;
	OpacityMaskRefVal = .3f;
	SetDepthPriorityGroup(ESceneDepthPriorityGroup::SDPG_MAX);
	bHiddenInGame = true;
#endif

}

void UInteractableComponent::BeginPlay()
{
	Super::BeginPlay();

	if (const auto OverlapTrigger = Cast<UInteractionTrigger_Overlap>(InteractionTrigger))
	{
		if (OverlapTrigger->bStopOnEndOverlap) StopTrigger = EInteractionStopTrigger::Manual;
	}
	
	InteractionState = EInteractionState::NotStarted;
	// Broadcast initial lock status in case its useful for initialization (e.g modifying visuals to indicate status)
	if (LockStatus == EInteractionLockStatus::Locked)
	{
		OnInteractableLocked.Broadcast();
	}
	else if (LockStatus == EInteractionLockStatus::Unlocked)
	{
		OnInteractableUnlocked.Broadcast();
	}

	// Cache player reference
	if (const auto Player = UGameplayStatics::GetPlayerPawn(GetWorld(), 0))
	{
		ActionSystemComponent = Cast<UActionSystemComponent>(Player->FindComponentByClass(UActionSystemComponent::StaticClass()));
		PlayerCharacter = Cast<ARadicalCharacter>(Player);
	}

	// Initialize behaviors & conditions & triggers
	if (InteractionTrigger)										InteractionTrigger->Initialize(this);
	for (auto Condition : InteractionConditions)				if (Condition) {Condition->Initialize(this); }
	for (auto Start : StartBehaviors)		if (Start) { Start->Initialize(this); }
	for (auto Update : UpdateBehaviors)		if (Update) { Update->Initialize(this); }
	for (auto End : EndBehaviors)			if (End) { End->Initialize(this); }

#if WITH_EDITOR
	bWarningsMsgsIssued = false;
#endif
}


// Called every frame
void UInteractableComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                           FActorComponentTickFunction* ThisTickFunction)
{
	SCOPE_CYCLE_COUNTER(STAT_TickInteractable);
	
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	
	if (InteractionState == EInteractionState::InProgress)
	{
		UpdateInteractionBehavior(DeltaTime);
	}

#if WITH_EDITOR
	
	if (!bWarningsMsgsIssued && ValidationResult == EDataValidationResult::Invalid)
	{
		if (UWorld * World = GetWorld())
		{
			if (World->HasBegunPlay() && IsRegistered())
			{
#define LOCTEXT_NAMESPACE "PIEError"
				FText ErrorTxt = FText::FormatOrdered(LOCTEXT("ErrorsFmt", "[{0}]: {1}"), FText::FromString(GetOwner()->GetActorNameOrLabel()), FText::Join(FText::FromStringView(TEXT(", ")), EditorStatusText));
				FMessageLog("PIE").Error(ErrorTxt);
				bWarningsMsgsIssued = true;
#undef LOCTEXT_NAMESPACE 
			}
		}
	}
	
	if (InteractCVars::VisualizeInteractionState > 0)
	{
		FString Domain = InteractionDomain == EInteractionDomain::OnStart ? "OnStart" : (InteractionDomain == EInteractionDomain::OnUpdate ? "OnUpdate" : "OnEnd");
		FString State = InteractionState == EInteractionState::NotStarted ? "NotStarted" : (InteractionState == EInteractionState::Viable ? "Viable" : "InProgress");

		FVector DrawLoc = GetOwner()->GetActorLocation();
		DrawDebugString(GetWorld(), DrawLoc + FVector::UpVector * 35.f, "STATE: " + State, 0, FColor::Purple, 0.f, true, 1.5f);
		DrawDebugString(GetWorld(), DrawLoc, "DOMAIN: " + Domain, 0, FColor::Purple, 0.f, true, 1.5f);

	}
#endif 
}

/*--------------------------------------------------------------------------------------------------------------
* Behavior
*--------------------------------------------------------------------------------------------------------------*/


void UInteractableComponent::InitializeInteraction()
{
	SCOPE_CYCLE_COUNTER(STAT_InitInteractable);
	
	if (!IsInteractionValid()) return;

	Internal_StartEvent.Broadcast();

	bStopRequested = false;

	SetInteractionState(EInteractionState::InProgress);

	SetInteractionDomain(EInteractionDomain::OnStart);

	INTERACTABLE_VLOG(GetOwner(), Log, "Initialized Interaction")
}

void UInteractableComponent::UpdateInteractionBehavior(float DeltaTime)
{
	TArray<UInteractionBehavior*> BehaviorsToTick = TArray<UInteractionBehavior*>();
	BehaviorsToTick = InteractionDomain == EInteractionDomain::OnStart ? StartBehaviors : (InteractionDomain == EInteractionDomain::OnUpdate ? UpdateBehaviors : EndBehaviors);

	uint8 CompletedBehaviors = 0;
	for (auto Behavior : BehaviorsToTick)
	{
		if (!Behavior) continue;
		
		if (Behavior->IsComplete())
		{
			CompletedBehaviors++;
			continue;
		}
		
		Behavior->ExecuteInteraction(InteractionDomain, DeltaTime);
	}

	// We prolly wanna wait up on OnUpdate? How do we end up going into StopInteraction like this? a bit confuuuused.
	// cuz we're forgetting about our own ones like this.

	if (CompletedBehaviors == BehaviorsToTick.Num())
	{
		// Finished executing all "OnStart" events, move onto OnUpdate
		switch(InteractionDomain)
		{
			case EInteractionDomain::OnStart:
				SetInteractionDomain(EInteractionDomain::OnUpdate);
				break;
			case EInteractionDomain::OnUpdate:
				if (StopTrigger == EInteractionStopTrigger::Automatic || bStopRequested)
				{
					SetInteractionDomain(EInteractionDomain::OnEnd);
				}
				break;
			case EInteractionDomain::OnEnd:
				if (StopTrigger == EInteractionStopTrigger::Automatic || bStopRequested)
				{
					StopInteraction();
				}
				break;
		}
	}


	if (InteractionDomain == EInteractionDomain::OnUpdate)
	{
		OnInteractionUpdateEvent.Broadcast(DeltaTime);
		OnInteractionUpdate(DeltaTime);
	}
}

void UInteractableComponent::StopInteraction(bool bCalledExternally)
{
	if (bCalledExternally && !bAllowStopExternal) return;
	if (InteractionState != EInteractionState::InProgress) return; // Don't double call

	INTERACTABLE_VLOG(GetOwner(), Log, "Compeleted Interaction")

	bStopRequested = true;
	
	// Make sure we allow all our EndBehaviors to run fully (?)
	if (InteractionDomain == EInteractionDomain::OnUpdate)
	{
		SetInteractionDomain(EInteractionDomain::OnEnd);
	}
	uint8 NumBehaviorsComplete = 0;
	for (auto EndBeh : EndBehaviors)
	{
		if (EndBeh->IsComplete()) NumBehaviorsComplete++;
	}
	// Don't exit out just yet, let the OnEnd behaviors run first!
	if (NumBehaviorsComplete != EndBehaviors.Num()) return;
	
	OnInteractionEndedEvent.Broadcast();
	OnInteractionEnded();
	SetInteractionState(EInteractionState::NotStarted);
}

/*--------------------------------------------------------------------------------------------------------------
* Trigger Methods/Queries
*--------------------------------------------------------------------------------------------------------------*/


void UInteractableComponent::SetInteractionState(EInteractionState NewState)
{
	switch (NewState)
	{
		case EInteractionState::NotStarted:
			if (InteractionState == EInteractionState::Viable)
			{
				OnInteractableNoLongerViable.Broadcast();
			}
			break;
		case EInteractionState::Viable:
			if (InteractionState < EInteractionState::Viable)
			{
				OnInteractableBecomeViable.Broadcast();
			}
			break;
		case EInteractionState::InProgress:
			if (InteractionState < EInteractionState::InProgress)
			{
				//InitializeInteraction();
				//PrimaryComponentTick.RegisterTickFunction(GetWorld()->GetCurrentLevel());
				// If we've successfully started the interaction and no longer viable, notify for cleanup
				OnInteractableNoLongerViable.Broadcast();
			}
			break;
	}
	
	InteractionState = NewState;
}

void UInteractableComponent::SetInteractionDomain(EInteractionDomain NewDomain)
{
	InteractionDomain = NewDomain;

	switch (InteractionDomain)
	{
		case EInteractionDomain::OnUpdate: // We call our OnStart once OnStart behaviors have finished
			OnInteractionStartedEvent.Broadcast();
			OnInteractionStarted();
			break;
		case EInteractionDomain::OnEnd:
			OnInteractionEndedEvent.Broadcast();
			OnInteractionEnded();
			break;
		default: ;
	}
}

void UInteractableComponent::SetInteractionLockStatus(EInteractionLockStatus InStatus)
{
	if (LockStatus == InStatus) return;

	LockStatus = InStatus;
	
	if (LockStatus == EInteractionLockStatus::Locked)
	{
		OnInteractableLocked.Broadcast();
	}
	else if (LockStatus == EInteractionLockStatus::Unlocked)
	{
		OnInteractableUnlocked.Broadcast();
	}
}

bool UInteractableComponent::IsInteractionValid() const
{
	SCOPE_CYCLE_COUNTER(STAT_CondiInteractable)
	
	// Maybe one day we wanna retrigger, but for now doing this
	if (InteractionState == EInteractionState::InProgress)
	{
		INTERACTABLE_VLOG(GetOwner(), Warning, "Tried Initialiazing already active interactable and failed")
		return false;
	}

	if (LockStatus == EInteractionLockStatus::Locked)
	{
		INTERACTABLE_VLOG(GetOwner(), Warning, "Tried Initializing interactable that is currently locked")
		return false;

	}
	
	for (auto InteractionCondition : InteractionConditions)
	{
		if (!InteractionCondition) continue;
		
		if (!InteractionCondition->CanInteract(this))
		{
			INTERACTABLE_VLOG(GetOwner(), Warning, "[%s] Interaction Condition Failed", *InteractionCondition->GetName())
			return false;
		}
	}

	return true;
}


#if WITH_EDITOR

void UInteractableComponent::SpawnTransformMarker() const
{
	if (GetWorld())
	{
		auto mutableThis = const_cast<UInteractableComponent*>(this);
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Owner = GetOwner();
		//SpawnParameters.Name = FName(GetOwner()->GetName().Append("_InteractionMarker"));
		//SpawnParameters.NameMode = FActorSpawnParameters::ESpawnActorNameMode::Requested;
		auto TargetPoint = GetWorld()->SpawnActor<ATargetPoint>(ATargetPoint::StaticClass(), GetComponentTransform(), SpawnParameters);
		TargetPoint->Tags.Add(FName("InteractionMarker"));
		TargetPoint->AttachToActor(GetOwner(), FAttachmentTransformRules(EAttachmentRule::KeepWorld, false));
		mutableThis->CachedSpawnedMarkers.Add(TargetPoint);
	}
}

#define LOCTEXT_NAMESPACE "InteractionValidation"

void UInteractableComponent::PostLoad()
{
	Super::PostLoad();

#if WITH_EDITORONLY_DATA
	static FString ContentDir = IPluginManager::Get().FindPlugin(TEXT("CoreFramework"))->GetBaseDir() / TEXT("Resources");
	static FString IconDir = (ContentDir / TEXT("InteractableIcon")) + TEXT(".png");
	const auto Icon = FImageUtils::ImportFileAsTexture2D(IconDir);
	Sprite = Icon;
	SetWorldScale3D(FVector(1,1,1));
	ScreenSize = -0.00025;
	bIsScreenSizeScaled = true;
	bUseInEditorScaling = false;
	OpacityMaskRefVal = .3f;
	SetDepthPriorityGroup(ESceneDepthPriorityGroup::SDPG_MAX);
#endif

	
	// Validation
	FDataValidationContext Context;
	IsDataValid(Context);
}

void UInteractableComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	FDataValidationContext Context;
	IsDataValid(Context);
}

EDataValidationResult UInteractableComponent::IsDataValid(FDataValidationContext& Context)
{
	ValidationResult = EDataValidationResult::NotValidated;

	if (InteractionTrigger)
	{
		ValidationResult = InteractionTrigger->IsDataValid(Context);
	}

	if (ValidationResult != EDataValidationResult::Invalid)
	{
		for (auto Condition : InteractionConditions)
		{
			if (!Condition)
			{
				Context.AddWarning(LOCTEXT("BehIsNull", "Null entry in InteractionBehaviors/Conditions"));
				ValidationResult = EDataValidationResult::Invalid;
				break;
			}
			ValidationResult = Condition->IsDataValid(Context);
			if (ValidationResult == EDataValidationResult::Invalid) break;
		}

	
		if (ValidationResult != EDataValidationResult::Invalid)
		{
			auto Behaviors = TMap<EInteractionDomain, TArray<UInteractionBehavior*>>();
			Behaviors.Add(EInteractionDomain::OnStart, StartBehaviors);
			Behaviors.Add(EInteractionDomain::OnUpdate, UpdateBehaviors);
			Behaviors.Add(EInteractionDomain::OnEnd, EndBehaviors);
		
			for (auto Behavior : Behaviors)
			{
				for (auto BehaviorScript : Behavior.Value)
				{
					if (!BehaviorScript)
					{
						Context.AddWarning(LOCTEXT("BehIsNull", "Null entry in InteractionBehaviors/Conditions"));
						ValidationResult = EDataValidationResult::Invalid;
						break;
					}
					ValidationResult = BehaviorScript->IsBehaviorValid(Behavior.Key, Context);
					if (ValidationResult == EDataValidationResult::Invalid) break;
				}
				if (ValidationResult == EDataValidationResult::Invalid) break;
			}
		}
	}

	TArray<FText> Warnings, Errors;
	Context.SplitIssues(Warnings, Errors);

	if (Errors.Num() > 0)
	{
		EditorStatusText = FText::FormatOrdered(LOCTEXT("ErrorsFmt", "Error: {0}"), FText::Join(FText::FromStringView(TEXT(", ")), Errors));
	}
	else if (Warnings.Num() > 0)
	{
		EditorStatusText = FText::FormatOrdered(LOCTEXT("WarningsFmt", "Warning: {0}"), FText::Join(FText::FromStringView(TEXT(", ")), Warnings));
	}
	else
	{
		const FString StopMethod = StopTrigger == EInteractionStopTrigger::Automatic ? "AUTOMATIC" : "MANUAL";
		EditorStatusText = FText::FormatOrdered(LOCTEXT("AllOk", "[Stop Trigger {0}] All Ok"), FText::FromString(StopMethod));
	}
	
	return ValidationResult;
}

EDataValidationResult UInteractableComponent::IsDataValid(TArray<FText>& ValidationErrors)
{
	FDataValidationContext Context;
	EDataValidationResult Result = IsDataValid(Context);
	
	TArray<FText> Warnings, Errors;
	Context.SplitIssues(Warnings, Errors);
	ValidationErrors.Append(Errors);
	
	return Result;
}

TArray<AActor*> UInteractableComponent::GetReferences() const
{
	auto Behaviors = TArray<UInteractionBehavior*>();
	Behaviors.Append(StartBehaviors);
	Behaviors.Append(UpdateBehaviors);
	Behaviors.Append(EndBehaviors);
	
	auto mutableThis = const_cast<UInteractableComponent*>(this);
	
	TArray<AActor*> References;
	for (int j = CachedSpawnedMarkers.Num() - 1; j >= 0; j--)
	{
		if (!CachedSpawnedMarkers[j].IsValid() || CachedSpawnedMarkers[j]->GetOwner() != GetOwner())
		{
			mutableThis->CachedSpawnedMarkers.RemoveAtSwap(j);
		}
		else
		{
			 References.Add(CachedSpawnedMarkers[j].Get());
		}
	}
	
	for (auto Behavior : Behaviors)
	{
		if (!Behavior) continue;
		References.Append(Behavior->GetWorldReferences());
	}

	for (auto Condition : InteractionConditions)
	{
		if (!Condition) continue;
		References.Append(Condition->GetWorldReferences());
	}

	if (InteractionTrigger)
	{
		References.Append(InteractionTrigger->GetWorldReferences());
	}

	return References;
}

bool UInteractableComponent::IsActorReferenced(AActor* ActorQuery) const
{
	return GetReferences().Contains(ActorQuery);
}

#undef LOCTEXT_NAMESPACE
#endif 