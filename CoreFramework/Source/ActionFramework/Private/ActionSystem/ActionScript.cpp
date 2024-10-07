// Copyright 2023 CoC All rights reserved


#include "ActionSystem/ActionScript.h"
#include "RadicalCharacter.h"
#include "TimerManager.h"
#include "ActionSystem/GameplayAction.h"
#include "Engine/LocalPlayer.h"

void UActionScript::Initialize(UGameplayAction* InOwnerAction)
{
	OwnerAction = InOwnerAction;
	CurrentActorInfo = OwnerAction->CurrentActorInfo;
	PlayerIndex = 0;
	
	if (CurrentActorInfo->CharacterOwner.Get() && CurrentActorInfo->CharacterOwner->IsPlayerControlled())
	{
		APlayerController* Controller = Cast<APlayerController>(CurrentActorInfo->CharacterOwner->GetController());
		if (Controller)
			PlayerIndex = Controller->GetLocalPlayer()->GetLocalPlayerIndex();
	}
	else
	{
		// Set timer and retry
		FTimerDelegate RetryDelegate;
		RetryDelegate.BindLambda([this]
		{
			if (CurrentActorInfo->CharacterOwner.Get() && CurrentActorInfo->CharacterOwner->IsPlayerControlled())
			{
				APlayerController* Controller = Cast<APlayerController>(CurrentActorInfo->CharacterOwner->GetController());
				if (Controller)
					PlayerIndex = Controller->GetLocalPlayer()->GetLocalPlayerIndex();
			}
		});

		GetWorld()->GetTimerManager().SetTimer(RetryTimerHandle, RetryDelegate, 0.5f, false);
	}
}

void UActionScript::Cleanup()
{
	GetWorld()->GetTimerManager().ClearTimer(RetryTimerHandle);
}

void UActionEvent::Initialize(UGameplayAction* InOwnerAction)
{
	Super::Initialize(InOwnerAction);

	// BUG: bIgnoreEvent is being set to true sometimes when it shouldnt be. Guessing this is happening when we're duplicating it, the outer changes on subobjects? Debugger says no tho
	if (Event.IsValid() && Event.GetEvent(OwnerAction.Get()))
		Event.GetEvent(OwnerAction.Get())->Event.AddUObject(this, &UActionEvent::ExecuteEvent);
}
