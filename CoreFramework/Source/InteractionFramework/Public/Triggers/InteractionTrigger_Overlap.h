// Copyright 2023 CoC All rights reserved

#pragma once

#include "CoreMinimal.h"
#include "InteractionTrigger.h"
#include "InteractionTrigger_Overlap.generated.h"

/**
 * 
 */
UCLASS(DisplayName="On Overlap")
class INTERACTIONFRAMEWORK_API UInteractionTrigger_Overlap : public UInteractionTrigger
{
	GENERATED_BODY()

	friend class UInteractableComponent;
	
protected:

	UPROPERTY(EditAnywhere)
	bool bStopOnEndOverlap = false;
	
	virtual void Initialize(UInteractableComponent* OwnerInteractable) override;

	UFUNCTION()
	virtual void OnBeginOverlap(AActor* OverlappedActor, AActor* OtherActor);
	UFUNCTION()
	virtual void OnEndOverlap(AActor* OverlappedActor, AActor* OtherActor);

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) override;
#endif

private:
	bool bTriggeredWithoutRegistering;
};
