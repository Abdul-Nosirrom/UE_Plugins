// Copyright 2023 CoC All rights reserved

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "AnimNotifyState_ConditionalSectionChange.generated.h"



UCLASS()
class COMBATFRAMEWORK_API UAnimNotifyState_ConditionalSectionChange : public UAnimNotifyState
{
	GENERATED_BODY()
protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName TargetSection = "End";
	UPROPERTY(Instanced)
	class UActionCondition* Condition;

	UAnimNotifyState_ConditionalSectionChange() { bIsNativeBranchingPoint = true; }
	
	virtual void BranchingPointNotifyBegin(FBranchingPointNotifyPayload& BranchingPointPayload) override;
	virtual void BranchingPointNotifyTick(FBranchingPointNotifyPayload& BranchingPointPayload, float FrameDeltaTime) override;
	virtual void BranchingPointNotifyEnd(FBranchingPointNotifyPayload& BranchingPointPayload) override;

#if WITH_EDITOR
	virtual FString GetNotifyName_Implementation() const override;
#endif 
};
