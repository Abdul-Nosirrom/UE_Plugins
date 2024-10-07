// Copyright 2023 CoC All rights reserved

#pragma once

#include "CoreMinimal.h"
#include "ActionSystem/ActionScript.h"
#include "ActionEvents_Animations.generated.h"

/// Types of animation events:
/// 1.) Basic: Just play an animation
/// 2.) Action Animation: Plays an animation at the start of the action, ends the action when the anim ends. Listens to certain anim notifies.

/// @brief	Simple montage player. Plays the given animation montage at the appropriate event broadcast and thats it
UCLASS(DisplayName="Play Montage")
class ACTIONFRAMEWORK_API UActionEvent_PlayMontage : public UActionEvent
{
	GENERATED_BODY()

public:

	UPROPERTY(EditAnywhere)
	UAnimMontage* Montage;
	UPROPERTY(EditAnywhere)
	float PlayRate = 1.f;
	UPROPERTY(EditAnywhere)
	bool bEndMontageWhenActionEnds = true;
	
	UActionEvent_PlayMontage() { DECLARE_ACTION_EVENT("Play Montage", false); }

	virtual void Initialize(UGameplayAction* InOwnerAction) override;
	virtual void ExecuteEvent() override;
	void OnActionEnd();
};

/// @brief	More advanced montage player. Ties the action to the animation, listening in to various notifies on the action
///			such as 'SetCanCancel' and such. Will end the action when the animation ends.
UCLASS(DisplayName="Action Animation")
class ACTIONFRAMEWORK_API UActionEvent_ActionAnimation : public UActionEvent
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere)
	UAnimMontage* Montage;
	UPROPERTY(EditAnywhere)
	float PlayRate = 1.f;
	
	UActionEvent_ActionAnimation() { DECLARE_ACTION_EVENT("Action Animation", true); }

	virtual void Initialize(UGameplayAction* InOwnerAction) override;
	virtual void ExecuteEvent() override;
	void OnActionEnd();

	UFUNCTION()
	void OnNotifyBeginReceived(FName NotifyName, const FBranchingPointNotifyPayload& Payload);
	UFUNCTION()
	void OnNotifyEndReceived(FName NotifyName, const FBranchingPointNotifyPayload& Payload);
	UFUNCTION()
	void OnMontageEnded(UAnimMontage* EndedMontage, bool bInterrupted);
};