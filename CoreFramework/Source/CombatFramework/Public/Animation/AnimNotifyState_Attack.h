// Copyright 2023 CoC All rights reserved

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "Data/CombatData.h"
#include "AnimNotifyState_Attack.generated.h"

/**
 * 
 */
UCLASS()
class COMBATFRAMEWORK_API UAnimNotifyState_Attack : public UAnimNotifyState
{
	GENERATED_BODY()
protected:
	UAnimNotifyState_Attack()
	{
		bIsNativeBranchingPoint = true;
	}
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ShowOnlyInnerProperties))
	FAttackData AttackData;

	UPROPERTY(Category=Trace, EditAnywhere, BlueprintReadOnly)
	FVector HitBoxPos;
	UPROPERTY(Category=Trace, EditAnywhere, BlueprintReadOnly)
	FVector HitBoxSize = FVector(100.f, 100.f, 100.f);

	virtual void BranchingPointNotifyBegin(FBranchingPointNotifyPayload& BranchingPointPayload) override;
	virtual void BranchingPointNotifyTick(FBranchingPointNotifyPayload& BranchingPointPayload, float FrameDeltaTime) override;
	virtual void BranchingPointNotifyEnd(FBranchingPointNotifyPayload& BranchingPointPayload) override;
};
