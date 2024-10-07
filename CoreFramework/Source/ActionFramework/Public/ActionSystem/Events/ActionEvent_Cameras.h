// Copyright 2023 CoC All rights reserved

#pragma once

#include "CoreMinimal.h"
#include "ActionSystem/ActionScript.h"
#include "ActionEvent_Cameras.generated.h"


UCLASS(DisplayName="Camera Modifier")
class ACTIONFRAMEWORK_API UActionEvent_CameraModifier : public UActionEvent
{
	GENERATED_BODY()
protected:
	UActionEvent_CameraModifier()
	{ DECLARE_ACTION_EVENT("Camera Modifier", true) }

	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<UCameraModifier> CameraModifier;

	UPROPERTY(Transient)
	TWeakObjectPtr<UCameraModifier> CameraModifierInstance;
	
	virtual void Initialize(UGameplayAction* InOwnerAction) override;

	virtual void ExecuteEvent() override;

	virtual void RemoveCameraModifier();
};

UCLASS(DisplayName="Camera Shake")
class UActionEvent_CameraShake : public UActionEvent 
{
	GENERATED_BODY()

protected:
	UActionEvent_CameraShake() { DECLARE_ACTION_EVENT("Camera Shake", false) }

	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<UCameraShakeBase> ShakeClass;
	UPROPERTY(EditDefaultsOnly)
	float Scale;
	
	UPROPERTY(Transient)
	TWeakObjectPtr<UCameraShakeBase> ShakeInstance;

	virtual void Initialize(UGameplayAction* InOwnerAction) override;
	virtual void ExecuteEvent() override;
};
