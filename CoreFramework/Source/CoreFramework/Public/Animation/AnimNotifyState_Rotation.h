// Copyright 2023 CoC All rights reserved

#pragma once

#include "CoreMinimal.h"
#include "CharacterNotifyState.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "AnimNotifyState_Rotation.generated.h"

/**
 * 
 */
UCLASS()
class COREFRAMEWORK_API UAnimNotifyState_Rotation : public UCharacterNotifyState
{
	GENERATED_BODY()
protected:

	UPROPERTY(Category=Physics, EditAnywhere, BlueprintReadOnly)
	FRuntimeFloatCurve YawCurve;
	
	virtual void BranchingPointNotifyBegin(FBranchingPointNotifyPayload& BranchingPointPayload) override;
	virtual void BranchingPointNotifyEnd(FBranchingPointNotifyPayload& BranchingPointPayload) override;
	
	UFUNCTION()
	void UpdateRotation(class URadicalMovementComponent* MovementComponent, float DeltaTime);
	
};
