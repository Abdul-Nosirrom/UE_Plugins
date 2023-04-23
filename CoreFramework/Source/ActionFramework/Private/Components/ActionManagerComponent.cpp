// Fill out your copyright notice in the Description page of Project Settings.


#include "Components/ActionManagerComponent.h"

#include "RadicalCharacter.h"
#include "RadicalMovementComponent.h"
#include "ActionSystem/GameplayAction.h"
#include "Actors/LevelPrimitiveActor.h"
#include "Actors/RadicalPlayerCharacter.h"
#include "Engine/AssetManager.h"
#include "Engine/Canvas.h"

DECLARE_CYCLE_STAT(TEXT("Tick Actions"), STAT_TickStateMachine, STATGROUP_ActionManagerComp)
DECLARE_CYCLE_STAT(TEXT("Activate Action"), STAT_ActivateAction, STATGROUP_ActionManagerComp)
DECLARE_CYCLE_STAT(TEXT("Calculate Action Velocity"), STAT_CalcVelocity, STATGROUP_ActionManagerComp)
DECLARE_CYCLE_STAT(TEXT("Update Action Rotation"), STAT_UpdateRot, STATGROUP_ActionManagerComp)

// Sets default values for this component's properties
UActionManagerComponent::UActionManagerComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	A_Instance = nullptr;
}

void UActionManagerComponent::InitializeComponent()
{
	Super::InitializeComponent();
}


// Called when the game starts
void UActionManagerComponent::BeginPlay()
{
	Super::BeginPlay();

	/* Create Instance Of SM Class And Assign It */
	if (StateMachineClass && !A_Instance)
	{
		A_Instance = NewObject<UGameplayStateMachine_Base>(GetOuter(), StateMachineClass, NAME_None, RF_NoFlags);
		A_Instance->RadicalOwner = CharacterOwner;
		A_Instance->Component = this;
	}
	
	/* Initialize the state machine and bind to events */
	if (!A_Instance->IsInitialized())
	{
		A_Instance->InitializeActions(CharacterOwner, this);
	}
	
	A_Instance->SetRegisterTick(A_Instance->CanEverTick());
	
	A_Instance->Start();
	
	/* Bind to movement component events */
	CharacterOwner = Cast<ARadicalPlayerCharacter>(GetOuter());
	CharacterOwner->GetCharacterMovement()->CalculateVelocityDelegate.BindDynamic(this, &UActionManagerComponent::CalculateVelocity);
	CharacterOwner->GetCharacterMovement()->UpdateRotationDelegate.BindDynamic(this, &UActionManagerComponent::UpdateRotation);
	CharacterOwner->GetCharacterMovement()->PostProcessRMVelocityDelegate.BindDynamic(this, &UActionManagerComponent::PostProcessRMVelocity);
	CharacterOwner->GetCharacterMovement()->PostProcessRMRotationDelegate.BindDynamic(this, &UActionManagerComponent::PostProcessRMRotation);

	/* Initialize Actor Info*/
	ActionActorInfo.InitFromCharacter(CharacterOwner, this);
}


void UActionManagerComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	SCOPED_NAMED_EVENT(UActionManagerComponent_TickComponent, FColor::Yellow)
	SCOPE_CYCLE_COUNTER(STAT_TickStateMachine)
	
	/* Tick the active actions */
	if (RunningActions.Num() > 0)
	{
		for (int i = RunningActions.Num() - 1; i >= 0; i--)
		{
			RunningActions[i]->OnActionTick(DeltaTime);
		}
	}

	
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}


/*--------------------------------------------------------------------------------------------------------------
* Level Primitives Interface
*--------------------------------------------------------------------------------------------------------------*/

void UActionManagerComponent::RegisterLevelPrimitive(ALevelPrimitiveActor* LP)
{
	if (ActiveLevelPrimitives.Contains(LP->GetTag()))
	{
		ActiveLevelPrimitives[LP->GetTag()] = LP;
	}
	else
	{
		ActiveLevelPrimitives.Add(LP->GetTag(), LP);
	}

	if (!GrantedTags.HasTag(LP->GetTag()))
		AddTag(LP->GetTag()); 
}

bool UActionManagerComponent::UnRegisterLevelPrimitive(ALevelPrimitiveActor* LP)
{
	if (ActiveLevelPrimitives.Contains(LP->GetTag()))
	{
		ActiveLevelPrimitives[LP->GetTag()] = nullptr;
	}
	
	return RemoveTag(LP->GetTag());
}

ALevelPrimitiveActor* UActionManagerComponent::GetActiveLevelPrimitive(FGameplayTag LPTag) const
{
	return ActiveLevelPrimitives.Contains(LPTag) ? ActiveLevelPrimitives[LPTag] : nullptr;
}



/*--------------------------------------------------------------------------------------------------------------
* Actions Interface
*--------------------------------------------------------------------------------------------------------------*/

UGameplayAction* UActionManagerComponent::FindActionInstanceFromClass(TSubclassOf<UGameplayAction> InActionClass)
{
	if (ActivatableActions.Contains(InActionClass))
	{
		return ActivatableActions[InActionClass];
	}
	return nullptr;
}

UGameplayAction* UActionManagerComponent::GiveAction(TSubclassOf<UGameplayAction> InActionClass)
{
	if (!IsValid(InActionClass))
	{
		// Log error, class not valid
		return nullptr;
	}

	if (const auto ActionInstance = FindActionInstanceFromClass(InActionClass))
	{
		// We already have this action class granted, abort (?)
		return ActionInstance;
	}

	UGameplayAction* InstancedAction = ActivatableActions.Contains(InActionClass) ? ActivatableActions[InActionClass] : ActivatableActions.Add(InActionClass, CreateNewInstanceOfAbility(InActionClass));
	
	// Can also notify an action if it has been granted
	InstancedAction->OnActionGranted(&ActionActorInfo);
	
	return InstancedAction;
}

UGameplayAction* UActionManagerComponent::GiveActionAndActivateOnce(TSubclassOf<UGameplayAction> InActionClass)
{
	if (!InActionClass) return nullptr; // Not a valid class

	auto Instance = GiveAction(InActionClass);

	if (InternalTryActivateAction(Instance))
	{
		// Succesfully activated, be sure to delete ater ends
		return Instance;
	}

	return nullptr;
}

bool UActionManagerComponent::RemoveAction(TSubclassOf<UGameplayAction> InActionClass)
{
	auto Action = FindActionInstanceFromClass(InActionClass);

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

	Action->OnActionRemoved(&ActionActorInfo); // Call on CDO maybe

	return true;
}


UGameplayAction* UActionManagerComponent::CreateNewInstanceOfAbility(TSubclassOf<UGameplayAction> InActionClass) const
{
	check(InActionClass);

	AActor* Owner = GetOwner();
	check(Owner);

	UAssetManager* Manager = UAssetManager::GetIfValid();

	if (Manager)
	{
		//Manager->load
	}
	
	UGameplayAction* ActionInstance = NewObject<UGameplayAction>(Owner, InActionClass);
	check(ActionInstance);
	
	return ActionInstance;
}

bool UActionManagerComponent::TryActivateAbilityByClass(TSubclassOf<UGameplayAction> InActionToActivate)
{
	SCOPE_CYCLE_COUNTER(STAT_ActivateAction)
	
	UGameplayAction* Action = FindActionInstanceFromClass(InActionToActivate);

	if (!Action)
	{
		if (!InActionToActivate.GetDefaultObject()->bGrantOnActivation)
		{
			// Log, no available action of this class (unless bGrant on activate, if so we create it here)
			return false;
		}
		Action = GiveAction(InActionToActivate);
	}

	return InternalTryActivateAction(Action);
}

bool UActionManagerComponent::InternalTryActivateAction(UGameplayAction* Action)
{
	// Make a FGameplayActionActorInfo struct that's held by us here. This also lets us use this independently I think

	if (!Action)
	{
		// Log Error
		return false;
	}

	// Check action condition
	if (!Action->CanActivateAction())
	{
		return false;
	}

	// If action is already active, check to see if we have to restart it
	if (Action->IsActive()) // Action is already active, we restart it
	{
		if (Action->bRetriggerAbility)
			Action->EndAction(true);
		else
		{
			// Log that action is already active
			return false;
		}
	}

	// Setup activation info for the actionspec
	Action->ActivateAction();

	return true;
}



void UActionManagerComponent::NotifyActionActivated(UGameplayAction* Action)
{
	check(Action);
	
	// Action is already registered as active
	if (RunningActions.Contains(Action)) return;

	RunningActions.Add(Action);

	if (Action->bRespondToMovementEvents)
	{
		RunningCalcVelocityActions.Add(Action);
	}

	if (Action->bRespondToRotationEvents)
	{
		RunningUpdateRotationActions.Add(Action);
	}
}

void UActionManagerComponent::NotifyActionEnded(UGameplayAction* Action)
{
	check(Action);

	// Action was not registered, we have nothing to remove
	if (!RunningActions.Contains(Action)) return;

	RunningActions.Remove(Action);
	
	if (Action->bRespondToMovementEvents)
	{
		RunningCalcVelocityActions.Remove(Action);
	}

	if (Action->bRespondToRotationEvents)
	{
		RunningUpdateRotationActions.Remove(Action);
	}
}

void UActionManagerComponent::CalculateVelocity(URadicalMovementComponent* MovementComponent, float DeltaTime)
{
	SCOPE_CYCLE_COUNTER(STAT_CalcVelocity)

	if (RunningCalcVelocityActions.Num() > 0)
	{
		for (const auto Action : RunningCalcVelocityActions)
		{
			Action->CalcVelocity(MovementComponent, DeltaTime);
		}
	}
	else
	{
		MovementComponent->GetMovementData()->CalculateVelocity(MovementComponent, DeltaTime);
	}
}

void UActionManagerComponent::UpdateRotation(URadicalMovementComponent* MovementComponent, float DeltaTime)
{
	SCOPE_CYCLE_COUNTER(STAT_UpdateRot)

	if (RunningUpdateRotationActions.Num() > 0)
	{
		for (const auto Action : RunningUpdateRotationActions)
		{
			Action->UpdateRotation(MovementComponent, DeltaTime);
		}
	}
	else
	{
		MovementComponent->GetMovementData()->UpdateRotation(MovementComponent, DeltaTime);
	}
}

void UActionManagerComponent::PostProcessRMVelocity(URadicalMovementComponent* MovementComponent, FVector& Velocity,
	float DeltaTime)
{
	SCOPE_CYCLE_COUNTER(STAT_CalcVelocity)

if (RunningCalcVelocityActions.Num() > 0)
{
	for (const auto Action : RunningCalcVelocityActions)
	{
		Action->PostProcessRMVelocity_Implementation(Velocity, DeltaTime);
	}
}
}

void UActionManagerComponent::PostProcessRMRotation(URadicalMovementComponent* MovementComponent, FQuat& Rotation,
	float DeltaTime)
{
	SCOPE_CYCLE_COUNTER(STAT_UpdateRot)

	if (RunningUpdateRotationActions.Num() > 0)
	{
		for (const auto Action : RunningUpdateRotationActions)
		{
			Action->PostProcessRMRotation(Rotation, DeltaTime);
		}
	}
}


/*--------------------------------------------------------------------------------------------------------------
* Debug
*--------------------------------------------------------------------------------------------------------------*/

void UActionManagerComponent::DisplayDebug(UCanvas* Canvas, const FDebugDisplayInfo& DebugDisplay, float& YL,
										   float& YPos)
{
	auto DisplayDebugManager = Canvas->DisplayDebugManager;
	
	DisplayDebugManager.SetDrawColor(FColor::Red);
	DisplayDebugManager.SetFont(GEngine->GetLargeFont());
	DisplayDebugManager.DrawString("[ ACTIONS ]");
	DisplayDebugManager.SetFont(GEngine->GetMediumFont());
	DisplayDebugManager.SetDrawColor(FColor::Yellow);

	float CachedYPos = DisplayDebugManager.GetYPos();
	
	FString T = "Granted Actions: ";
	int Len = T.Len();
	DisplayDebugManager.DrawString(T);

	/* Draw Activatable Actions*/
	DisplayDebugManager.SetDrawColor(FColor::White);
	float XOffset = 0.75f * DisplayDebugManager.GetMaxCharHeight() * Len;
	DisplayDebugManager.SetYPos(CachedYPos);
	T = "";
	for (auto Action : ActivatableActions)
	{
		T += Action.Value->GetName();
		T += ", ";
	}
	DisplayDebugManager.DrawString(T, XOffset);

	/* Draw Running Actions */
	CachedYPos = DisplayDebugManager.GetYPos();
	T = "Running Actions: ";
	Len = T.Len();
	XOffset = 0.75f * DisplayDebugManager.GetMaxCharHeight() * Len;
	DisplayDebugManager.SetDrawColor(FColor::Yellow);
	DisplayDebugManager.DrawString(T);
	T = "";
	DisplayDebugManager.SetDrawColor(FColor::White);
	for (auto Action : RunningActions)
	{
		DisplayDebugManager.SetYPos(CachedYPos);
		DisplayDebugManager.SetDrawColor(Action->CanBeCanceled() ? FColor::Green : FColor::Red);
		DisplayDebugManager.DrawString(Action->GetName() + ", ", XOffset);
		
		Len += Action->GetName().Len();
		XOffset = 0.75f * DisplayDebugManager.GetMaxCharHeight() * Len;
	}

	//float XOffset = 0.75f * DisplayDebugManager.GetMaxCharHeight() * Len;

	/*
	DisplayDebugManager.SetDrawColor(FColor::White);
	T = A_Instance->GetSingleActiveState()->GetNodeName();
	DisplayDebugManager.DrawString(T, XOffset);
	T = A_Instance->GetSingleNestedActiveState()->GetNodeName();
	DisplayDebugManager.DrawString(T, XOffset);
	*/
	
	/* Level primitives */
		DisplayDebugManager.SetYPos(DisplayDebugManager.GetYPos() + 50.f);
		DisplayDebugManager.SetDrawColor(FColor::Red);
    	DisplayDebugManager.SetFont(GEngine->GetLargeFont());
    	DisplayDebugManager.DrawString("[ PRIMITIVES ]");
    	DisplayDebugManager.SetFont(GEngine->GetMediumFont());
    	DisplayDebugManager.SetDrawColor(FColor::Yellow);
	
	for (auto Prims : ActiveLevelPrimitives)
	{
		YPos = DisplayDebugManager.GetYPos();
		T = Prims.Key.ToString();
		XOffset = 0.75 * DisplayDebugManager.GetMaxCharHeight() * T.Len();
		DisplayDebugManager.SetDrawColor(FColor::Yellow);
		DisplayDebugManager.DrawString(T);

		DisplayDebugManager.SetYPos(YPos);
		DisplayDebugManager.SetDrawColor(Prims.Value ? FColor::White : FColor::Red);
		T = Prims.Value ? Prims.Value->GetName() : "Null";
		DisplayDebugManager.DrawString(T, XOffset);
	}
}
