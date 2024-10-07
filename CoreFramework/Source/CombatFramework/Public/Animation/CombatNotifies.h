// Copyright 2023 CoC All rights reserved

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "CombatNotifies.generated.h"


UCLASS()
class COMBATFRAMEWORK_API UAnimNotify_ClearHitList : public UAnimNotify
{
	GENERATED_BODY()

	UAnimNotify_ClearHitList()
	{
		bIsNativeBranchingPoint = true;
	}

	virtual void BranchingPointNotify(FBranchingPointNotifyPayload& BranchingPointPayload) override;
	
	virtual FString GetNotifyName_Implementation() const override;
};

UCLASS()
class COMBATFRAMEWORK_API UAnimNotify_LaunchCharacter : public UAnimNotify 
{
	GENERATED_BODY()

protected:
	UPROPERTY(Category=Movement, EditAnywhere)
	FVector LaunchVel;
	UPROPERTY(Category=Movement, EditAnywhere)
	bool bPlanarOverride;
	UPROPERTY(Category=Movement, EditAnywhere)
	bool bVerticalOverride;
	
	UAnimNotify_LaunchCharacter()
	{
		bIsNativeBranchingPoint = true;
	}

	virtual void BranchingPointNotify(FBranchingPointNotifyPayload& BranchingPointPayload) override;

	virtual FString GetNotifyName_Implementation() const override
	{
		return "Launch Character";
	}
};
