// Copyright 2023 CoC All rights reserved

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "ActionNotifies.generated.h"

namespace ActionNotifies
{
	inline static FName CanCancelNotify = "CanCancel";
}

/// @brief	Sets CanCancel to true when this notify is hit. Note, only works with animation events on an action
UCLASS(DisplayName="Cancel Frame")
class UActionNotify_CanCancel : public UAnimNotify
{
	GENERATED_BODY()

	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
	virtual FString GetNotifyName_Implementation() const override { return "Cancel Frame"; }
};
