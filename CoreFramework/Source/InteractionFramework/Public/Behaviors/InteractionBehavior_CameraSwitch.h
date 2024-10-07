// Copyright 2023 CoC All rights reserved

#pragma once

#include "CoreMinimal.h"
#include "InteractionBehavior.h"
#include "InteractionBehavior_CameraSwitch.generated.h"


UCLASS(DisplayName="Camera Switch", MinimalAPI)
class UInteractionBehavior_CameraSwitch : public UInteractionBehavior
{
	GENERATED_BODY()
protected:
	
	/// @brief	Delay prior to switching cameras, in seconds after the behavior is first triggered
	UPROPERTY(EditAnywhere, meta=(ClampMin = 0, UIMin = 0, Units=s))
	float SwitchDelay = 0.f;
	/// @brief	If non-zero, we switch to the camera, hold for this amount of seconds, then switch back. Otherwise, switch back automatically happens on interactin end
	UPROPERTY(EditAnywhere, meta=(ClampMin = 0, UIMin = 0, Units=s))
	float HoldTime = 0.f;
	
	UPROPERTY(EditAnywhere)
	AActor* CameraActor;
	UPROPERTY(EditAnywhere)
	FViewTargetTransitionParams BlendInParams;
	UPROPERTY(EditAnywhere)
	FViewTargetTransitionParams BlendOutParams;

	virtual void ExecuteInteraction(EInteractionDomain Domain, float DeltaTime) override;
	void SwapCamera(bool bToInteractionCamera);
	virtual void InteractionEnded() override;

	virtual void InteractionStarted() override { Super::InteractionStarted(); bDoneExecuting = false; }
	
	bool bDoneExecuting;

#if WITH_EDITOR
	virtual TArray<AActor*> GetWorldReferences() const override;
	virtual EDataValidationResult IsBehaviorValid(EInteractionDomain InteractionDomain, FDataValidationContext& Context) override;
#endif 
};
