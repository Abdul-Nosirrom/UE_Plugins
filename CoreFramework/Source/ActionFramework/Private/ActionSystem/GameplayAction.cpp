// 


#include "ActionSystem/GameplayAction.h"

#include "AF_PCH.h"
#include "RadicalMovementComponent.h"
#include "TimerManager.h"
#include "ActionSystem/GameplayActionTypes.h"
#include "Components/ActionManagerComponent.h"

DECLARE_CYCLE_STAT(TEXT("Activating Action Internal"), STAT_ActivateActionInternal, STATGROUP_ActionManagerComp)
DECLARE_CYCLE_STAT(TEXT("End Action Internal"), STAT_EndActionInternal, STATGROUP_ActionManagerComp)

UWorld* UGameplayAction::GetWorld() const
{
	//Return null if the called from the CDO, or if the outer is being destroyed
	if (!HasAnyFlags(RF_ClassDefaultObject) &&  !GetOuter()->HasAnyFlags(RF_BeginDestroyed) && !GetOuter()->IsUnreachable())
	{
		//Try to get the world from the owning actor if we have one
		AActor* Outer = GetTypedOuter<AActor>();
		if (Outer != nullptr)
		{
			return Outer->GetWorld();
		}
	}
	//Else return null - the latent action will fail to initialize
	return nullptr;
}

void UGameplayAction::OnActionGranted(const FActionActorInfo* ActorInfo)
{
	// Set actor and spec info
	CurrentActorInfo = ActorInfo;
	bIsCancelable = true;
	K2_OnActionGranted();
}

void UGameplayAction::OnActionRemoved(const FActionActorInfo* ActorInfo)
{
	K2_OnActionRemoved();
}

bool UGameplayAction::CanActivateAction()
{
	// Ensure we hae a valid ActionmanagerComponent reference
	if (!CurrentActorInfo->ActionManagerComponent.IsValid())
	{
		return false;
	}

	// TODO: Possible cooldown and "ammo" checks here

	// Ensure valid tag requirements satisfied
	if (!DoesSatisfyTagRequirements())
	{
		return false;
	}

	// Ensure spec on ActionManager is valid (maybe not)

	//bool bHasBlueprintActivateCondition = false; // TODO: But as of right now, condition is NativeEvent

	return EnterCondition();
}

bool UGameplayAction::DoesSatisfyTagRequirements() const
{
	return true;
}

void UGameplayAction::ActivateAction()
{
	SCOPE_CYCLE_COUNTER(STAT_ActivateActionInternal);
	
	if (!CurrentActorInfo->ActionManagerComponent.IsValid())
	{
		return; // No valid owner
	}

	TimeActivated = GetWorld()->GetTimeSeconds();
	bIsActive = true;
	bIsCancelable = true;

	// Set current info (Not necessary in our use case I believe)
	
	// Grant tags to owner
	CurrentActorInfo->ActionManagerComponent->AddTags(ActionTags);

	//if (OnActionEndedDelegate) // Pass this in
	//{
	//	OnActionEnded.Add(*OnActionEndedDelegate); // we clear the bindings of this when we end so no need to manually unbind
	//}

	CurrentActorInfo->ActionManagerComponent->NotifyActionActivated(this);
	// TODO: more tag stuff

	OnActionActivated();
}

void UGameplayAction::EndAction(bool bWasCancelled)
{
	SCOPE_CYCLE_COUNTER(STAT_EndActionInternal);
	
	if (IsActive())
	{
		// Stop timers or latent actions
		UWorld* MyWorld = GetWorld();
		if (MyWorld)
		{
			MyWorld->GetLatentActionManager().RemoveActionsForObject(this);
			MyWorld->GetTimerManager().ClearAllTimersForObject(this);
		}

		// Execute events then unbind it since we're no longer active
		OnGameplayActionEnded.Broadcast(this, bWasCancelled);
		OnGameplayActionEnded.Clear();
		
		// Overridden action behavior
		OnActionEnd();

		// Notify action manager
		if (CurrentActorInfo->ActionManagerComponent.IsValid())
		{
			// Remove tags
			CurrentActorInfo->ActionManagerComponent->NotifyActionEnded(this);
		}

		bIsActive = false;
	}
}

void UGameplayAction::CancelAction()
{
	if (CanBeCanceled())
	{
		EndAction(true);
	}
}

