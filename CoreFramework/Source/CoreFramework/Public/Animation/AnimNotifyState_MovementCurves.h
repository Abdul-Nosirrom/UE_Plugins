// Copyright 2023 CoC All rights reserved

#pragma once

#include "CoreMinimal.h"
#include "CharacterNotifyState.h"
#include "Helpers/PhysicsUtilities.h"
#include "AnimNotifyState_MovementCurves.generated.h"

UCLASS()
class COREFRAMEWORK_API UAnimNotifyState_MovementCurves : public UCharacterNotifyState
{
	GENERATED_BODY()
protected:
#if WITH_EDITORONLY_DATA
	UPROPERTY(VisibleAnywhere)
	FString EditorStatusText;
#endif
	
	UPROPERTY(EditAnywhere, meta=(ShowOnlyInnerProperties))
	FCurveMovementParams Params;
	
	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float FrameDeltaTime, const FAnimNotifyEventReference& EventReference) override {}
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override; 

	UFUNCTION()
	void OnMontageBlendedOut(UAnimMontage* Montage, bool bInterrupted);

#if WITH_EDITOR
	// Check for NANs if position curve
	virtual void PostLoad() override {Super::PostLoad(); EditorStatusText = Params.ValidateCurve(Duration);}
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override { Super::PostEditChangeProperty(PropertyChangedEvent); EditorStatusText = Params.ValidateCurve(Duration);}
#endif 
};
