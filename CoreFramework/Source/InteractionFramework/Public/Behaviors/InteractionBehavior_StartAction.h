// Copyright 2023 CoC All rights reserved

#pragma once

#include "CoreMinimal.h"
#include "InteractionBehavior.h"
#include "InteractionBehavior_StartAction.generated.h"

/**
 * 
 */
UCLASS(DisplayName="Start Action", MinimalAPI)
class UInteractionBehavior_StartAction : public UInteractionBehavior
{
	GENERATED_BODY()
protected:

	virtual void ExecuteInteraction(EInteractionDomain Domain, float DeltaTime) override;

	virtual void InteractionEnded() override;
};
