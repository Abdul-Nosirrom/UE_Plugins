// Copyright 2023 CoC All rights reserved

#pragma once

#include "CoreMinimal.h"
#include "InteractionBehavior.h"
#include "Engine/TargetPoint.h"
#include "InteractionBehavior_MovePlayerTo.generated.h"


UCLASS(DisplayName="Move Player To")
class INTERACTIONFRAMEWORK_API UInteractionBehavior_MovePlayerTo : public UInteractionBehavior
{
	GENERATED_BODY()

protected:
	
	UPROPERTY(EditAnywhere)
	ATargetPoint* TargetLocation;
	UPROPERTY(EditAnywhere)
	float AcceptanceRadius = 65.f;
	UPROPERTY(EditAnywhere)
	float Delay;
	
	bool bDoneExecuting;

	virtual void InteractionStarted() override { Super::InteractionStarted(); bDoneExecuting = false; }

	virtual void ExecuteInteraction(EInteractionDomain Domain, float DeltaTime) override;

	virtual void InteractionEnded() override;

	void PerformMove();

	virtual void OnDestinationReached();

#if WITH_EDITOR 
	virtual TArray<AActor*> GetWorldReferences() const override { return {TargetLocation}; }
#endif
	
	FTimerHandle ReachedLocationHandle;
};

UCLASS(DisplayName="Play Animation Move Player To")
class INTERACTIONFRAMEWORK_API UInteractionBehavior_MovePlayerToAndPlayAnimation : public UInteractionBehavior_MovePlayerTo
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere)
	UAnimMontage* Animation;
	UPROPERTY(EditAnywhere)
	float PlayRate = 1.f;
	
	virtual void OnDestinationReached() override;

	FTimerHandle AnimationFinishedHandle;
};
