// Fill out your copyright notice in the Description page of Project Settings.


#include "Subsystems/InteractionManager.h"

#include "InputBufferSubsystem.h"
#include "InteractionSystemDebug.h"
#include "RadicalCharacter.h"
#include "Components/ActionSystemComponent.h"
#include "Components/InteractableComponent.h"
#include "Engine/World.h"
#include "Helpers/InteractionSystemStatics.h"
#include "Kismet/GameplayStatics.h"
#include "UI/InteractableWidgetBase.h"

DECLARE_CYCLE_STAT(TEXT("[Manager]: Update Non-Input Interactables"), STAT_NonInputUpdate, STATGROUP_InteractionSystem)
DECLARE_CYCLE_STAT(TEXT("[Manager]: Score Input Interactables"), STAT_InputUpdate, STATGROUP_InteractionSystem)

void UInteractionManager::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	InputDelegate.Delegate.BindDynamic(this, &UInteractionManager::InputCallback);
}

void UInteractionManager::Deinitialize()
{
	Super::Deinitialize();

	CleanupInputCallbacks();
	InputDelegate.Delegate.Unbind();
}

void UInteractionManager::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!PlayerCharacter && !InitializeRequirements()) return;

	{
		SCOPE_CYCLE_COUNTER(STAT_InputUpdate)
		
		const auto PrevViable = ActiveInteractable;
		UpdateMostViableInteractable();
		bHasInteractableChanged = (PrevViable != ActiveInteractable) && (PrevViable != nullptr && ActiveInteractable != nullptr);
		UpdateInteractableWidget();
	}

	// Tick non-input interactables
	{
		SCOPE_CYCLE_COUNTER(STAT_NonInputUpdate)
		
		for (int32 IntIdx = AvailableInteractables.Num()-1; IntIdx >= 0; IntIdx--)
		{
			const auto NonInputInteractable = AvailableInteractables[IntIdx];
			if (NonInputInteractable.IsValid())
			{
				AvailableInteractables.RemoveAtSwap(IntIdx);
				continue;
			}

			if (NonInputInteractable->IsInteractionValid())
			{
				NonInputInteractable->InitializeInteraction();
				// NOTE: For now lets remove it after we start it to avoid issues with constant retriggering
				AvailableInteractables.RemoveAtSwap(IntIdx);
			}
		}
	}
}

bool UInteractionManager::InitializeRequirements()
{
	if (auto Player = Cast<ARadicalCharacter>(UGameplayStatics::GetPlayerPawn(GetWorld(), 0)))
	{
		PlayerCharacter = Player;
		return true;
	}
	return false;
}

/*--------------------------------------------------------------------------------------------------------------
* Registration
*--------------------------------------------------------------------------------------------------------------*/

void UInteractionManager::RegisterInteractable(UInteractableComponent* InInteractable)
{
	AvailableInteractables.AddUnique(InInteractable);
}

void UInteractionManager::UnRegisterInteractable(UInteractableComponent* InInteractable)
{
	AvailableInteractables.Remove(InInteractable);
}

void UInteractionManager::RegisterInputInteractable(UInteractableComponent* InInteractable)
{
	AvailableInputInteractables.AddUnique(InInteractable);
}

void UInteractionManager::UnRegisterInputInteractable(UInteractableComponent* InInteractable)
{
	if (InInteractable->InteractionState == EInteractionState::Viable)
	{
		InInteractable->SetInteractionState(EInteractionState::NotStarted);
	}
	if (InInteractable == ActiveInteractable) ActiveInteractable = nullptr;
	AvailableInputInteractables.Remove(InInteractable);
}

/*--------------------------------------------------------------------------------------------------------------
* Ranking
*--------------------------------------------------------------------------------------------------------------*/


void UInteractionManager::UpdateMostViableInteractable()
{
	if (AvailableInputInteractables.IsEmpty()) return;

	UInteractableComponent* NewViableInteractable = nullptr;

	// Check if currently active is no longer valid (due to tag requirements or whatever)
	if (ActiveInteractable && !ActiveInteractable->IsInteractionValid()) ActiveInteractable = nullptr;

	float BestDot = -UE_BIG_NUMBER;
	const FVector CameraFacing = PlayerCharacter->GetControlRotation().Vector();
	for (int i = AvailableInputInteractables.Num() - 1; i >= 0; i--)
	{
		// Check if null, remove it if so
		if (!AvailableInputInteractables[i].IsValid())
		{
			AvailableInputInteractables.RemoveAtSwap(i);
			continue;
		}
		
		// If this interactables conditions fail to interact with, skip it
		if (!AvailableInputInteractables[i]->IsInteractionValid()) continue;

		// Do ranking
		const FVector ToVector = (AvailableInputInteractables[i]->GetComponentLocation() - PlayerCharacter->GetActorLocation()).GetSafeNormal();
		if ((CameraFacing | ToVector) > BestDot)
		{
			NewViableInteractable = AvailableInputInteractables[i].Get();
			BestDot = CameraFacing | ToVector;
		}
	}

	// No viable interactable from our list, exit
	if (!NewViableInteractable || NewViableInteractable == ActiveInteractable) return;

	// Let the old one know its gone
	if (ActiveInteractable)
	{
		// If in progress, it'll auto handle it
		if (ActiveInteractable->InteractionState != EInteractionState::InProgress)
			ActiveInteractable->SetInteractionState(EInteractionState::NotStarted);
		CleanupInputCallbacks();
	}
	
	ActiveInteractable = NewViableInteractable;
	
	// Change its state and bind to its inputs
	ActiveInteractable->SetInteractionState(EInteractionState::Viable);
	SetupInputCallbacks();
}

void UInteractionManager::UpdateInteractableWidget()
{
	// Remove the previous widget if it exists
	if (!ActiveInteractable && ActiveInteractableWidget)
	{
		ActiveInteractableWidget->RemoveFromParent();
		ActiveInteractableWidget = nullptr;
		return;
	}

	// Remove the previous widget if it exists and our interactable has changed
	if (bHasInteractableChanged && ActiveInteractableWidget)
	{
		ActiveInteractableWidget->RemoveFromParent();
		ActiveInteractableWidget = nullptr;
	}

	if (!ActiveInteractable) return;

	// If we're here, that means we have an active interactable, so check if we have a widget for it
	GetOrCreateInteractableWidget();
	

	// Update the widgets location to the interactables location
	FVector2D WidgetLocation;
	UGameplayStatics::GetPlayerController(GetWorld(), 0)->ProjectWorldLocationToScreen(ActiveInteractable->GetComponentLocation(), WidgetLocation);
	ActiveInteractableWidget->SetPositionInViewport(WidgetLocation);
}

void UInteractionManager::GetOrCreateInteractableWidget()
{
	// Override not set, use the default
	TSubclassOf<UInteractableWidgetBase> WidgetClass = UInteractionSystemStatics::GetDefaultInteractableWidget();;
	if (ActiveInteractable->OverrideInteractionPrompt) // Use the provided widget class and add it to our widget map
	{
		WidgetClass = ActiveInteractable->OverrideInteractionPrompt;
		
		if (InteractableWidgets.Contains(ActiveInteractable->OverrideInteractionPrompt)) // May have been already created
		{
			ActiveInteractableWidget = InteractableWidgets[ActiveInteractable->OverrideInteractionPrompt]; // Could still be null so we check below to create it or not
		}
	}

	if (!ActiveInteractableWidget)
	{
		ActiveInteractableWidget = Cast<UInteractableWidgetBase>(CreateWidget(UGameplayStatics::GetPlayerController(GetWorld(), 0), WidgetClass));
		if (InteractableWidgets.Contains(WidgetClass)) InteractableWidgets[WidgetClass] = ActiveInteractableWidget;
		else InteractableWidgets.Add(WidgetClass, ActiveInteractableWidget);
	}
	
	ActiveInteractableWidget->AddToViewport();
}

UInteractableComponent* UInteractionManager::GetActiveInteractable() const
{
	if (ActiveInteractable && ActiveInteractable->InteractionState != EInteractionState::InProgress)
	{
		return ActiveInteractable;
	}
	return nullptr;
}

/*--------------------------------------------------------------------------------------------------------------
* Input Handling
*--------------------------------------------------------------------------------------------------------------*/


void UInteractionManager::InputCallback(const FInputActionValue& Value, float ElapsedTime)
{
	if (!ActiveInteractable) return;
	if (ActiveInteractable->InteractionState != EInteractionState::Viable) return;

	PlayerCharacter->GetInputBuffer()->ConsumeInput(UInteractionSystemStatics::GetDefaultInteractableInput());
	ActiveInteractable->InitializeInteraction();

	// Remove the widget assocaited with this interactable
	if (ActiveInteractableWidget) ActiveInteractableWidget->RemoveFromParent();
}

void UInteractionManager::SetupInputCallbacks()
{
	if (!ActiveInteractable) return;
	if (!PlayerCharacter || !PlayerCharacter->GetInputBuffer()) return;
	PlayerCharacter->GetInputBuffer()->BindAction(InputDelegate, UInteractionSystemStatics::GetDefaultInteractableInput(), TRIGGER_Press, false, 10);
}

void UInteractionManager::CleanupInputCallbacks()
{
	if (!ActiveInteractable) return;
	if (!PlayerCharacter || !PlayerCharacter->GetInputBuffer()) return;
	PlayerCharacter->GetInputBuffer()->UnbindAction(InputDelegate);
}

