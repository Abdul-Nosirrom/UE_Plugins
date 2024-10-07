// Copyright 2023 CoC All rights reserved

#pragma once

#include "CoreMinimal.h"
#include "ActionSystem/ActionScript.h"
#include "ActionEvent_OnCancelledBy.generated.h"


/// @brief	Triggers the contained event if the owner action is cancelled by a specific action
UCLASS(DisplayName="On Cancelled By")
class UActionEvent_OnCancelledBy : public UActionEvent
{
	GENERATED_BODY()
public:

	UPROPERTY(EditAnywhere, Instanced)
	UActionEvent* EventToTrigger;
	UPROPERTY(EditAnywhere)
	TObjectPtr<class UGameplayActionData> CancelledBy;

	UActionEvent_OnCancelledBy() { DECLARE_ACTION_EVENT("On Cancelled By", true) }

	virtual void Initialize(UGameplayAction* InOwnerAction) override;
	virtual void ExecuteEvent() override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual FString GetEditorFriendlyName() const override;
#endif 
};

UCLASS(DisplayName="If Previous Action Check")
class UActionEvent_PreviousActionCheck : public UActionEvent
{
	GENERATED_BODY()

public:

	UPROPERTY(EditAnywhere)
	TObjectPtr<UPrimaryActionData> PreviousAction;
	UPROPERTY(EditAnywhere, Instanced)
	UActionEvent* EventToTrigger;
	
	UActionEvent_PreviousActionCheck()
	{ DECLARE_ACTION_EVENT("If Previous Action Check", false) }

	virtual void Initialize(UGameplayAction* InOwnerAction) override;
	virtual void ExecuteEvent() override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual FString GetEditorFriendlyName() const override;
#endif 
};
