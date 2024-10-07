// Copyright 2023 CoC All rights reserved

#pragma once

#include "CoreMinimal.h"
#include "InteractionTrigger_Overlap.h"
#include "InteractionTrigger_InputRequirement.generated.h"

UCLASS(DisplayName="Requires Input", MinimalAPI)
class UInteractionTrigger_InputRequirement : public UInteractionTrigger_Overlap
{
	GENERATED_BODY()

protected:
	
	virtual void OnBeginOverlap(AActor* OverlappedActor, AActor* OtherActor) override;
	virtual void OnEndOverlap(AActor* OverlappedActor, AActor* OtherActor) override;
};
