// Copyright 2023 CoC All rights reserved


#include "Behaviors/InteractionBehavior_StartAction.h"

#include "Actions/Action_Interactable.h"
#include "Components/ActionSystemComponent.h"
#include "Components/InteractableComponent.h"
#include "Helpers/InteractionSystemStatics.h"

void UInteractionBehavior_StartAction::ExecuteInteraction(EInteractionDomain Domain, float DeltaTime)
{
	if (GetActionSystem())
	{
		if (!GetActionSystem()->TryActivateAbilityByClass(UInteractionSystemStatics::GetDefaultInteractableAction()))
		{
			InteractableOwner->StopInteraction();
		}
		else
		{
			if (auto Action = Cast<UAction_Interactable>(GetActionSystem()->GetRunningAction(UInteractionSystemStatics::GetDefaultInteractableAction())))
			{
				Action->Interactable = InteractableOwner;
			}
		}
	}

	CompleteBehavior();
}

void UInteractionBehavior_StartAction::InteractionEnded()
{
	Super::InteractionEnded();

	if (GetActionSystem())
	{
		GetActionSystem()->CancelAction(UInteractionSystemStatics::GetDefaultInteractableAction());
	}
}
