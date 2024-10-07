// Copyright 2023 CoC All rights reserved

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "Curves/CurveVector.h"
#include "Curves/CurveFloat.h"
#include "CharacterNotifyState.generated.h"

USTRUCT()
struct COREFRAMEWORK_API FAnimPhysicsInfo
{
	GENERATED_BODY()

	FAnimPhysicsInfo() {}
	
	FAnimPhysicsInfo(class ARadicalCharacter* Owner, float InStartTime, float InDuration);
	
	TWeakObjectPtr<ARadicalCharacter> Character;
	FTransform InitialTransform;
	float Duration;
	float StartTime;
	float NormalizedTime;
};

USTRUCT(BlueprintType)
struct COREFRAMEWORK_API FAnimScalarCurve
{
	GENERATED_BODY()

	float Sample(float InTime)
	{
		return Curve.GetRichCurve()->Eval(InTime) * Scale;
	}

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FRuntimeFloatCurve Curve;
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float Scale = 1.f;
};

USTRUCT(BlueprintType)
struct COREFRAMEWORK_API FAnimVectorCurve
{
	GENERATED_BODY()

	FVector Sample(float InTime) const
	{
		return Curve.GetValue(InTime) * Scale;
	}
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FRuntimeVectorCurve Curve;
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FVector Scale = FVector(1.f, 1.f, 1.f);
};

UCLASS(Abstract, NotBlueprintable)
class COREFRAMEWORK_API UCharacterNotifyState : public UAnimNotifyState
{
	GENERATED_BODY()
protected:

	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float FrameDeltaTime, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;

	UPROPERTY(Transient)
	TMap<USkeletalMeshComponent*, FAnimPhysicsInfo> PhysicsInfo;

	float Duration;

#if WITH_EDITOR
	virtual void OnAnimNotifyCreatedInEditor(FAnimNotifyEvent& ContainingAnimNotifyEvent) override
	{
		Super::OnAnimNotifyCreatedInEditor(ContainingAnimNotifyEvent);
		Duration = ContainingAnimNotifyEvent.GetDuration();
	}

	virtual void PostLoad() override { Super::PostLoad(); bIsNativeBranchingPoint = false; }

	virtual bool ShouldFireInEditor() override { return false; }
#endif;
};

// Interp Notify State. Has a fallback velocity curve, otherwise has an interpolation curve. Does a targeting request
// on notify begin, if a target is found, we do an interpolation to that target based on the curve otherwise we use the fallback
// velocity curve. Also we don't do translate position instead get the deltas and set our velocities to that so if we cancel out
// we carry over that momentum