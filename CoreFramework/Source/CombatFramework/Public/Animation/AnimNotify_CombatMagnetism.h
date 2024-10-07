// Copyright 2023 CoC All rights reserved

#pragma once

#include "CoreMinimal.h"
#include "Animation/CharacterNotifyState.h"
#include "AnimNotify_CombatMagnetism.generated.h"

/**
 * 
 */
UCLASS()
class COMBATFRAMEWORK_API UAnimNotify_CombatMagnetism : public UCharacterNotifyState
{
	GENERATED_BODY()
protected:

	UPROPERTY(Category=Movement, EditAnywhere)
	FAnimScalarCurve InterpolationCurve;
	UPROPERTY(Category=Movement, EditAnywhere)
	float MinDistance = 75.f;
	UPROPERTY(Category=Movement, EditAnywhere)
	bool bOnlyInterpGroundPlane = true;
	UPROPERTY(Category=Movement, EditAnywhere, meta=(EditCondition=bUseFallback))
	FAnimScalarCurve FallbackSpeedCurve;
	UPROPERTY(Category=Movement, EditAnywhere, meta=(InlineEditConditionToggle))
	bool bUseFallback = true;
	UPROPERTY(Category=Targeting, EditAnywhere)
	class UTargetingPreset* Targeting;

	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float FrameDeltaTime, const FAnimNotifyEventReference& EventReference) override;
	
	UPROPERTY(Transient)
	AActor* TargetedActor;
};
