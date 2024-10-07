// Fill out your copyright notice in the Description page of Project Settings.


#include "Components/ActionSystemComponent.h"

#include "DrawDebugHelpers.h"
#include "Debug/ActionSystemLog.h"
#include "RadicalCharacter.h"
#include "RadicalMovementComponent.h"
#include "ActionSystem/GameplayAction.h"
#include "Components/LevelPrimitiveComponent.h"
#include "Engine/Canvas.h"

namespace ActionSystemCVars
{
#if ALLOW_CONSOLE && !NO_LOGGING
	int32 DisplayActions = 0;
	FAutoConsoleVariableRef CVarShowActionsState
	(
		TEXT("actions.Display"),
		DisplayActions,
		TEXT("Display Actions. 0: Disable, 1: Enable"),
		ECVF_Default
	);
#endif 
}

DECLARE_CYCLE_STAT(TEXT("Tick Actions"), STAT_TickActionSystem, STATGROUP_ActionSystem)
DECLARE_CYCLE_STAT(TEXT("Activate Action"), STAT_ActivateAction, STATGROUP_ActionSystem)
DECLARE_CYCLE_STAT(TEXT("Calculate Action Velocity"), STAT_CalcVelocity, STATGROUP_ActionSystem)
DECLARE_CYCLE_STAT(TEXT("Update Action Rotation"), STAT_UpdateRot, STATGROUP_ActionSystem)

// Sets default values for this component's properties
UActionSystemComponent::UActionSystemComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UActionSystemComponent::InitializeComponent()
{
	Super::InitializeComponent();
}


// Called when the game starts
void UActionSystemComponent::BeginPlay()
{
	Super::BeginPlay();

	/* Bind to movement component events */
	CharacterOwner = Cast<ARadicalCharacter>(GetOwner());
	CharacterOwner->GetCharacterMovement()->RequestPriorityVelocityBinding().BindUObject(this, &UActionSystemComponent::CalculateVelocity);
	CharacterOwner->GetCharacterMovement()->RequestPriorityRotationBinding().BindUObject(this, &UActionSystemComponent::UpdateRotation);

	/* Initialize Actor Info*/
	ActionActorInfo.InitFromCharacter(CharacterOwner, this);

	
	/* Create actions part of our character action set */
	InitializeCharacterActionSet();

	TimeLastPrimaryActivated = 0.f;
}

void UActionSystemComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	SCOPED_NAMED_EVENT(UActionSystemComponent_TickComponent, FColor::Yellow)
	SCOPE_CYCLE_COUNTER(STAT_TickActionSystem)
	
	/* Tick the active actions */
	if (PrimaryActionRunning && PrimaryActionRunning->bActionTicks) PrimaryActionRunning->OnActionTick(DeltaTime);
	
	if (RunningAdditiveActions.Num() > 0)
	{
		for (int i = RunningAdditiveActions.Num() - 1; i >= 0; i--)
		{
			if (RunningAdditiveActions[i]->bActionTicks)
				RunningAdditiveActions[i]->OnActionTick(DeltaTime);
		}
	}

	/* Tick Condition Evaluation For Possible Actions In Our ActionSet or Followups*/
	UpdateActionSelection();
	
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

#if ALLOW_CONSOLE && !NO_LOGGING
	if (ActionSystemCVars::DisplayActions > 0)
	{
		FString PrimaryActionName = "Invalid";
		FColor DisplayColor = FColor::Red;
		float TimeActive = 0.f;
		if (PrimaryActionRunning)
		{
			PrimaryActionName = PrimaryActionRunning->GetName();
			if (PrimaryActionRunning->CanBeCanceled()) DisplayColor = FColor::Green;
			TimeActive = PrimaryActionRunning->GetTimeInAction();
		}

		FVector DrawLoc = CharacterOwner->GetActorLocation() + 90.f * FVector::UpVector;
		DrawDebugString(GetWorld(), DrawLoc, FString::Printf(TEXT("[%.2f] PRIMARY: %s"), TimeActive, *PrimaryActionName), 0, DisplayColor, 0.f, true);
	}
#endif 
}

void UActionSystemComponent::OnUnregister()
{
	Super::OnUnregister();

	// NOTE: MarkAsGarbage scope causes crash when using the TakeRecorder to record gameplay for sequencer and opening up the recorded sequence.

	// Cancel and clear running actions. Ending them will auto remove them from the lists
	CancelAllActions();
	
	// Mark running actions as garbage so they can be deleted
	for (auto AvailableAction : ActivatableActions)
	{
		// Let actions perform custom cleanups
		AvailableAction.Value->Cleanup();
		// Unbind input events
		AvailableAction.Value->CleanupActionTrigger();
		// Mark it as garbage
		AvailableAction.Value->MarkAsGarbage();
	}

	ActivatableActions.Empty();
}


/*--------------------------------------------------------------------------------------------------------------
* Level Primitives Interface
*--------------------------------------------------------------------------------------------------------------*/

void UActionSystemComponent::RegisterLevelPrimitive(ULevelPrimitiveComponent* LP, FGameplayTag PrimitiveTag)
{
	if (ActiveLevelPrimitives.Contains(PrimitiveTag))
	{
		ActiveLevelPrimitives[PrimitiveTag] = LP;
	}
	else
	{
		ActiveLevelPrimitives.Add(PrimitiveTag, LP);
	}

	if (!GrantedTags.HasMatchingGameplayTag(PrimitiveTag))
		AddTag(PrimitiveTag);

	OnLevelPrimitiveRegisteredDelegate.Broadcast(PrimitiveTag);
}

bool UActionSystemComponent::UnRegisterLevelPrimitive(ULevelPrimitiveComponent* LP, FGameplayTag PrimitiveTag)
{
	if (ActiveLevelPrimitives.Contains(PrimitiveTag))
	{
		ActiveLevelPrimitives[PrimitiveTag] = nullptr;
	}

	OnLevelPrimitiveUnRegisteredDelegate.Broadcast(PrimitiveTag);
	
	return RemoveTag(PrimitiveTag);
}

ULevelPrimitiveComponent* UActionSystemComponent::GetActiveLevelPrimitive(FGameplayTag LPTag) const
{
	return ActiveLevelPrimitives.Contains(LPTag) ? ActiveLevelPrimitives[LPTag] : nullptr;
}

/*--------------------------------------------------------------------------------------------------------------
* Actions Interface
*--------------------------------------------------------------------------------------------------------------*/

UGameplayAction* UActionSystemComponent::FindActionInstanceFromClass(UGameplayActionData* InActionData)
{
	if (ActivatableActions.Contains(InActionData))
	{
		return ActivatableActions[InActionData];
	}
	return nullptr;
}

UGameplayAction* UActionSystemComponent::GiveAction(UGameplayActionData* InActionData)
{
	if (!IsValid(InActionData) || !IsValid(InActionData->Action))
	{
		// Log error, class not valid
		ACTIONSYSTEM_VLOG(CharacterOwner, Error, "Attempted granting an invalid/null action")
		return nullptr;
	}

	if (const auto ActionInstance = FindActionInstanceFromClass(InActionData))
	{
		// We already have this action class granted, abort (?)
		return ActionInstance;
	}

	UGameplayAction* InstancedAction = ActivatableActions.Contains(InActionData) ? ActivatableActions[InActionData] : ActivatableActions.Add(InActionData, CreateNewInstanceOfAbility(InActionData));
	
	// Can also notify an action if it has been granted
	InstancedAction->OnActionGranted(&ActionActorInfo);
	OnActionGrantedDelegate.Broadcast(InActionData);
	
	return InstancedAction;
}

UGameplayAction* UActionSystemComponent::GiveActionAndActivateOnce(UGameplayActionData* InActionData)
{
	if (!InActionData) return nullptr; // Not a valid data asset
	
	auto Instance = GiveAction(InActionData);

	if (InternalTryActivateAction(Instance))
	{
		// Succesfully activated, be sure to delete after ends
		return Instance;
	}

	return nullptr;
}

bool UActionSystemComponent::RemoveAction(UGameplayActionData* InActionData)
{
	auto Action = FindActionInstanceFromClass(InActionData);

	if (!Action)
	{
		return false; // Failed to remove action. It was never granted in the first place
	}

	if (Action->IsActive())
	{
		// Mark action as pending kill, this will then delete it via
		Action->EndAction(true);
	}

	Action->MarkAsGarbage(); // This should be done after EndAction has finished if its Async

	Action->OnActionRemoved(&ActionActorInfo);
	
	return true;
}


UGameplayAction* UActionSystemComponent::CreateNewInstanceOfAbility(UGameplayActionData* InActionData) const
{
	check(InActionData);

	AActor* Owner = GetOwner();
	check(Owner);
	
	UGameplayAction* ActionInstance = DuplicateObject(InActionData->Action, Owner, InActionData->GetFName());
	check(ActionInstance);

	// Set ActionData on initial creation only
	ActionInstance->ActionData = InActionData;
	// NOTE: We could just place this in Granted since that's called at the same time as on creation, wait no but granted doesnt take in the data we need nvm
	return ActionInstance;
}

bool UActionSystemComponent::TryActivateAbilityByClass(UGameplayActionData* InActionData, bool bQueryOnly)
{
	SCOPE_CYCLE_COUNTER(STAT_ActivateAction)
	
	UGameplayAction* Action = FindActionInstanceFromClass(InActionData);

	if (!Action)
	{
		if (!InActionData->bGrantOnActivation)
		{
			ACTIONSYSTEM_VLOG(CharacterOwner, Warning, "[%s] Tried activating action that's not been granted yet and is not set to auto-grant on activation", *InActionData->GetName());
			return false;
		}
		Action = GiveAction(InActionData);
	}
	
	return InternalTryActivateAction(Action, bQueryOnly);
}

bool UActionSystemComponent::InternalTryActivateAction(UGameplayAction* Action, bool bQueryOnly)
{
	if (!Action)
	{
		ACTIONSYSTEM_VLOG(CharacterOwner, Warning, "(ACTIVATION FAILED) Action attempting to activate was null")
		return false;
	}

	// Is action explicitly blocked
	if (BlockedActions.Contains(Action->GetActionData()))
	{
		ACTIONSYSTEM_VLOG(CharacterOwner.Get(), Log, "(ACTIVATION FAILED) %s Is speficially blocked", *Action->GetActionData()->GetName());
		return false;
	}

	// Is the action thats trying to activate a primary action? If so can we cancel out of our current primary action?
	if (Action->ActionCategory == EActionCategory::Primary && PrimaryActionRunning)
	{
		if (!PrimaryActionRunning->CanBeCanceled())
		{
			ACTIONSYSTEM_VLOG(CharacterOwner, Log, "(ACTIVATION FAILED) Tried activating primary action [%s], but current primary action can't be cancelled yet [%s]", *Action->GetActionData()->GetName(), *PrimaryActionRunning->GetActionData()->GetName())
			return false;
		}
	}

	// Check action condition. This will check if we can retrigger as well in case the action is currently active
	if (!Action->CanActivateAction())
	{
		return false;
	}
	
	// Activate, and if current action was cancelled, set up info about the cancellation
	if (!bQueryOnly) 
	{
		// We're now guaranteed to start this action, so if its primary, lets be sure to end the current running primary
		if (Action->ActionCategory == EActionCategory::Primary && PrimaryActionRunning)
		{
			PrimaryActionRunning->ActionThatCancelledUs = Action->GetActionData();
			PrimaryActionRunning->EndAction(true); 
		}

		// Everything confirmed, activate the action. If its a retrigger, the action will end the current iteration itself and restart
		Action->ActivateAction();
	}
	
	return true;
}

void UActionSystemComponent::CancelAction(UGameplayActionData* Action)
{
	if (PrimaryActionRunning && PrimaryActionRunning->GetActionData() == Action) PrimaryActionRunning->EndAction(true);
	
	for (auto RunningActionIndex = RunningAdditiveActions.Num()-1; RunningActionIndex >= 0; RunningActionIndex--)
	{
		auto RunningAction = RunningAdditiveActions[RunningActionIndex];
		if (RunningAction->GetActionData() == Action)
		{
			RunningAction->EndAction(true);
			break;
		}
	}
}

void UActionSystemComponent::CancelActionsWithTags(const FGameplayTagContainer& CancelTags, const UGameplayActionData* Ignore)
{
	if (PrimaryActionRunning && PrimaryActionRunning->GetActionData()->ActionTags.HasAny(CancelTags)) PrimaryActionRunning->EndAction(true);

	for (auto RunningActionIndex = RunningAdditiveActions.Num()-1; RunningActionIndex >= 0; RunningActionIndex--)
	{
		auto RunningAction = RunningAdditiveActions[RunningActionIndex];
		if (RunningAction->GetActionData() == Ignore || !RunningAction->GetActionData()->ActionTags.HasAny(CancelTags))
		{
			continue;
		}
		RunningAction->EndAction(true);
	}
}

void UActionSystemComponent::CancelActionsWithTagRequirement(const FGameplayTagContainer& CancelTags)
{
	if (PrimaryActionRunning && PrimaryActionRunning->GetActionData()->OwnerRequiredTags.HasAny(CancelTags)) PrimaryActionRunning->EndAction(true);

	for (auto RunningActionIndex = RunningAdditiveActions.Num()-1; RunningActionIndex >= 0; RunningActionIndex--)
	{
		auto RunningAction = RunningAdditiveActions[RunningActionIndex];
		if (!RunningAction->GetActionData()->OwnerRequiredTags.HasAny(CancelTags))
		{
			continue;
		}
		RunningAction->EndAction(true);
	}
}

void UActionSystemComponent::CancelAllActions(const UGameplayActionData* Ignore)
{
	// Primary action will cancel all running additive actions when it ends
	if (PrimaryActionRunning && PrimaryActionRunning->GetActionData() != Ignore) PrimaryActionRunning->EndAction(true);
	
	for (auto RunningActionIndex = RunningAdditiveActions.Num()-1; RunningActionIndex >= 0; RunningActionIndex--)
	{
		auto RunningAction = RunningAdditiveActions[RunningActionIndex];
		if (RunningAction->GetActionData() == Ignore)
		{
			continue;
		}
		RunningAction->EndAction(true);
	}
}

void UActionSystemComponent::ApplyActionBlockAndCancelTags(const UGameplayActionData* InAction, bool bEnabledBlockTags, bool bExecuteCancelTags)
{
	if (bEnabledBlockTags)
	{
		BlockActionsWithTags(InAction->BlockActionsWithTag);
	}
	else
	{
		UnBlockActionsWithTags(InAction->BlockActionsWithTag);
	}

	if (bExecuteCancelTags)
	{
		CancelActionsWithTags(InAction->CancelActionsWithTag, InAction);
	}
	
}


void UActionSystemComponent::NotifyActionActivated(UGameplayAction* Action)
{
	check(Action);
	
	// Action is already registered as active NOTE: More to do here, e.g checking if the primary action is active and ending it if so
	if (Action->ActionCategory == EActionCategory::Primary)
	{
		TimeLastPrimaryActivated = GetWorld()->GetTimeSeconds();
		PrimaryActionRunning = Cast<UPrimaryAction>(Action);
	}
	else
	{
		if (RunningAdditiveActions.Contains(Action)) return;
		RunningAdditiveActions.Add(Cast<UAdditiveAction>(Action));
	}


	AddTags(Action->GetActionData()->ActionTags);
	ApplyActionBlockAndCancelTags(Action->GetActionData(), true, true);
	OnActionActivatedDelegate.Broadcast(Action->GetActionData());
}

void UActionSystemComponent::NotifyActionEnded(UGameplayAction* Action)
{
	check(Action);
	
	// Action was not registered, we have nothing to remove
	if (Action->ActionCategory == EActionCategory::Primary && Action == PrimaryActionRunning)
	{
		PreviousPrimaryAction = PrimaryActionRunning;
		PrimaryActionRunning = nullptr;
	}
	else
	{
		if (!RunningAdditiveActions.Contains(Action)) return; // we already got rid of it
		RunningAdditiveActions.Remove(Cast<UAdditiveAction>(Action));
	}

	RemoveTags(Action->GetActionData()->ActionTags);
	ApplyActionBlockAndCancelTags(Action->GetActionData(), false, false);
	OnActionEndedDelegate.Broadcast(Action->GetActionData());

	// Only cache prev actions info/tags if it was a primary action
	if (Action->ActionCategory == EActionCategory::Primary)
		PrevAction = Action->GetActionData()->ActionTags;
}

void UActionSystemComponent::CalculateVelocity(URadicalMovementComponent* MovementComponent, float DeltaTime)
{
	SCOPE_CYCLE_COUNTER(STAT_CalcVelocity)

	if (PrimaryActionRunning && PrimaryActionRunning->bRespondToMovementEvents)
	{
		PrimaryActionRunning->CalcVelocity(MovementComponent, DeltaTime);
	}
	else
	{
		MovementComponent->GetMovementData()->CalculateVelocity(MovementComponent, DeltaTime);
	}
}

void UActionSystemComponent::UpdateRotation(URadicalMovementComponent* MovementComponent, float DeltaTime)
{
	SCOPE_CYCLE_COUNTER(STAT_UpdateRot)

	if (PrimaryActionRunning && PrimaryActionRunning->bRespondToRotationEvents)
	{
		PrimaryActionRunning->UpdateRotation(MovementComponent, DeltaTime);
	}
	else
	{
		MovementComponent->GetMovementData()->UpdateRotation(MovementComponent, DeltaTime);
	}
}

/*--------------------------------------------------------------------------------------------------------------
* Debug
*--------------------------------------------------------------------------------------------------------------*/

#define DisplayDebugString(Text, XOffset, Font, Color) DisplayDebugManager.SetDrawColor(Color); DisplayDebugManager.SetFont(Font); DisplayDebugManager.DrawString(Text, XOffset); 

void UActionSystemComponent::DisplayDebug(UCanvas* Canvas, const FDebugDisplayInfo& DebugDisplay, float& YL,
										   float& YPos)
{
#if WITH_EDITOR
	auto DisplayDebugManager = Canvas->DisplayDebugManager;

	// Primary Actions Display
	FString PrimaryActionName = PrimaryActionRunning ? PrimaryActionRunning->GetName() : "NONE";
	float RowYPos = DisplayDebugManager.GetYPos();
	DisplayDebugString("[ PRIMARY ACTION ]", 0.f, GEngine->GetLargeFont(), FColor::Red)
	DisplayDebugManager.SetYPos(RowYPos);
	DisplayDebugString(PrimaryActionName, 140.f, GEngine->GetMediumFont(), FColor::White);

	// Additive Actions Display
	DisplayDebugString("________________________________", 0.f, GEngine->GetLargeFont(), FColor::Black);
	DisplayDebugString("[ ADDITIVE ACTIONS ]", 0.f, GEngine->GetLargeFont(), FColor::Red);
	FString AdditiveActionNames = "";
	for (auto Action : RunningAdditiveActions)
	{
		if (!Action) continue;
		AdditiveActionNames.Append(Action->GetName());
		AdditiveActionNames.Append(", ");
	}
	DisplayDebugString(AdditiveActionNames, 30.f, GEngine->GetMediumFont(), FColor::White);

	// Registered Actions (Awaiting Activation)
	DisplayDebugString("________________________________", 0.f, GEngine->GetLargeFont(), FColor::Black);
	DisplayDebugString("[ AWAITING ACTIVATION ]", 0.f, GEngine->GetLargeFont(), FColor::Red);
	FString AwaitingActivationNames = "";
	for (auto Action : ActionsAwaitingActivation)
	{
		if (!Action) continue;
		AwaitingActivationNames.Append(Action->GetName());
		AwaitingActivationNames.Append(", ");
	}
	DisplayDebugString(AwaitingActivationNames, 30.f, GEngine->GetMediumFont(), FColor::White);
	
	// Seperator
	DisplayDebugString("________________________________", 0.f, GEngine->GetLargeFont(), FColor::Black);
	
	// All Tags Display
	auto Tags = GrantedTags.GetExplicitGameplayTags().GetGameplayTagArray();
	FString TagStrings = "";
	uint32 rowCount = 0;
	for (auto Tag : Tags)
	{
		TagStrings.Append(Tag.ToString());
		TagStrings.Append(", ");
		rowCount++;
		if (rowCount % 6 == 0) TagStrings.Append("\n");
	}
	DisplayDebugString("[ GRANTED TAGS ]", 0.f, GEngine->GetLargeFont(), FColor::Red);
	DisplayDebugString(TagStrings, 30.f, GEngine->GetMediumFont(), FColor::White);
	
	// Block Tags Display
	Tags = BlockedTags.GetExplicitGameplayTags().GetGameplayTagArray();
	TagStrings = "";
	rowCount = 0;
	for (auto Tag : Tags)
	{
		TagStrings.Append(Tag.ToString());
		TagStrings.Append(", ");
		rowCount++;
		if (rowCount % 6 == 0) TagStrings.Append("\n");
	}
	DisplayDebugString("[ BLOCK TAGS ]", 0.f, GEngine->GetLargeFont(), FColor::Red);
	DisplayDebugString(TagStrings, 30.f, GEngine->GetMediumFont(), FColor::White);

	// Seperator
	DisplayDebugString("________________________________", 0.f, GEngine->GetLargeFont(), FColor::Black);

	// Specific blocked actions
	TagStrings = "";
	rowCount = 0;
	for (auto BlockedAction : BlockedActions)
	{
		if (!BlockedAction) continue;
		TagStrings.Append(BlockedAction->GetName());
		TagStrings.Append(", ");
		rowCount++;
		if (rowCount % 6 == 0) TagStrings.Append("\n");
	}
	DisplayDebugString("[ EXPLICIT BLOCKED ACTIONS ]", 0.f, GEngine->GetLargeFont(), FColor::Red);
	DisplayDebugString(TagStrings, 30.f, GEngine->GetMediumFont(), FColor::White);

	// Seperator
	DisplayDebugString("________________________________", 0.f, GEngine->GetLargeFont(), FColor::Black);

	// Primitive Display
	
#endif
}
