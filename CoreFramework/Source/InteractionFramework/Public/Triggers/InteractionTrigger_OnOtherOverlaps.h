// Copyright 2023 CoC All rights reserved

#pragma once

#include "CoreMinimal.h"
#include "InteractionTrigger.h"
#include "InteractionTrigger_OnOtherOverlaps.generated.h"

/// @brief	Requires all overlap volumes to have overlapped with something for the interaction to trigger
UCLASS(DisplayName="On Mutli-Object Overlaps", MinimalAPI)
class UInteractionTrigger_OnOtherOverlaps : public UInteractionTrigger
{
	GENERATED_BODY()

protected:

	UPROPERTY(EditAnywhere)
	TArray<AActor*> OverlapVolumes;

	uint8 NumOverlaps;

	virtual void Initialize(UInteractableComponent* OwnerInteractable) override;
	
	UFUNCTION()
	virtual void OnBeginOverlap(AActor* OverlappedActor, AActor* OtherActor);
	UFUNCTION()
	virtual void OnEndOverlap(AActor* OverlappedActor, AActor* OtherActor);

#if WITH_EDITOR
	virtual TArray<AActor*> GetWorldReferences() const override { return OverlapVolumes; }
#endif

private:

	bool bInitializedWithoutRegistering;
};
