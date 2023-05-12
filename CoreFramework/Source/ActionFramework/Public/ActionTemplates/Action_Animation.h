// 

#pragma once

#include "CoreMinimal.h"
#include "ActionSystem/GameplayAction.h"
#include "Interfaces/ActionStateAnimation.h"
#include "UObject/Object.h"
#include "Action_Animation.generated.h"

/// @brief Archetype for an action which plays an animation montage such that the action ends when the animation montage ends
UCLASS(Abstract, Blueprintable, BlueprintType, ClassGroup = ActionManager, meta = (DisplayName = "Animation Action Template", ShortTooltip="Action which plays an animation and exits when the animation ends"))
class ACTIONFRAMEWORK_API UAction_Animation : public UGameplayAction, public IActionStateAnimation
{
	GENERATED_BODY()

protected:
	UPROPERTY(Category=Animation, EditDefaultsOnly, BlueprintReadOnly)
	UAnimMontage* MontageToPlay;
	UPROPERTY(Category=Animation, EditDefaultsOnly, AdvancedDisplay, meta=(ClampMin=0, ClampMax=0, UIMin=0, UIMax=0))
	float PlayRate = 1;
	UPROPERTY(Category=Animation, EditDefaultsOnly, AdvancedDisplay, meta=(ClampMin=0, ClampMax=0, UIMin=0, UIMax=0))
	float StartingPosition = 0;
	UPROPERTY(Category=Animation, EditDefaultsOnly, AdvancedDisplay)
	FName StartingSection = NAME_None;
	
	// BEGIN UGameplayAction Interface
	virtual void OnActionActivated_Implementation() override;
	// END UGameplayAction Interface

	// BEGIN IActionStateAnimation Interface
	virtual void OnMontageEnded(UAnimMontage* Montage, bool bInterrupted) override { EndAction();}
	// END IActionStateAnimation Interface
};
