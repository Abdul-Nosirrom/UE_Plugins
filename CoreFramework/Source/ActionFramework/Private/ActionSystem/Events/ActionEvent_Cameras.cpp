// Copyright 2023 CoC All rights reserved


#include "ActionSystem/Events/ActionEvent_Cameras.h"

#include "ActionSystem/GameplayAction.h"
#include "Camera/CameraModifier.h"
#include "Camera/CameraShakeBase.h"

void UActionEvent_CameraModifier::Initialize(UGameplayAction* InOwnerAction)
{
	Super::Initialize(InOwnerAction);

	OwnerAction->OnGameplayActionStarted.Event.AddUObject(this, &UActionEvent_CameraModifier::ExecuteEvent);
	OwnerAction->OnGameplayActionEnded.Event.AddUObject(this, &UActionEvent_CameraModifier::RemoveCameraModifier);
}

void UActionEvent_CameraModifier::ExecuteEvent()
{
	if (CameraModifierInstance.IsValid())
	{
		CameraModifierInstance->EnableModifier();
	}
	else
	{
		// NOTE: I know it auto enabled when you first add it but why not lol
		CameraModifierInstance = UGameplayStatics::GetPlayerCameraManager(this, GetPlayerIndex())->AddNewCameraModifier(CameraModifier);
		if (CameraModifierInstance.IsValid()) CameraModifierInstance->EnableModifier();
	}
}

void UActionEvent_CameraModifier::RemoveCameraModifier()
{
	if (CameraModifierInstance.IsValid())
	{
		CameraModifierInstance->DisableModifier();
	}
}

void UActionEvent_CameraShake::Initialize(UGameplayAction* InOwnerAction)
{
	Super::Initialize(InOwnerAction);

	// For camera shakes w/ infinite duration (duration set to <= 0), make sure that we remove it on action end
	if (ShakeClass && ShakeClass->GetDefaultObject<UCameraShakeBase>()->GetCameraShakeDuration().IsInfinite())
	{
		OwnerAction->OnGameplayActionEnded.Event.AddLambda([this]
		{
			if (ShakeInstance.IsValid())
			{
				if (auto PCM = UGameplayStatics::GetPlayerCameraManager(this, GetPlayerIndex()))
					PCM->StopCameraShake(ShakeInstance.Get(), false);
			}
		});
	}
}

void UActionEvent_CameraShake::ExecuteEvent()
{
	ShakeInstance = UGameplayStatics::GetPlayerCameraManager(this, GetPlayerIndex())->StartCameraShake(ShakeClass, Scale);
}
