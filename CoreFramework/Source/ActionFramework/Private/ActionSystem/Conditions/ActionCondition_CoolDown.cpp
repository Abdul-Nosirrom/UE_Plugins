// Copyright 2023 CoC All rights reserved


#include "ActionSystem/Conditions/ActionCondition_CoolDown.h"

#include "RadicalCharacter.h"
#include "TimerManager.h"
#include "ActionSystem/GameplayAction.h"

void UActionCondition_CoolDown::Initialize(UGameplayAction* InOwnerAction)
{
	Super::Initialize(InOwnerAction);

	bIsOnCooldown = false;
	OwnerAction->OnGameplayActionEnded.Event.AddUObject(this, &UActionCondition_CoolDown::StartCooldown);
}

void UActionCondition_CoolDown::Cleanup()
{
	Super::Cleanup();
	bIsOnCooldown = false;
	GetWorld()->GetTimerManager().ClearTimer(CooldownTimerHandle);
}

void UActionCondition_CoolDown::StartCooldown()
{
	bIsOnCooldown = true;
	
	OwnerAction->OnCoolDownStarted.Broadcast(CoolDown);

	FTimerDelegate CooldownTimerDelegate;
	
	CooldownTimerDelegate.BindLambda([this]()
	{
		bIsOnCooldown = false;
		OwnerAction->OnCoolDownEnded.Broadcast();
	});

	// Bind on the character, we should do this for timers that aren't part of the actions so they dont get invalidated
	GetCharacterInfo()->CharacterOwner->GetWorld()->GetTimerManager().SetTimer(CooldownTimerHandle, CooldownTimerDelegate, CoolDown, false);
}
