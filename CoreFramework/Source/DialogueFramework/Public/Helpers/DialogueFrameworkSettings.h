// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "DialogueFrameworkSettings.generated.h"

class UDialogueWidgetBase;
/**
 * 
 */
UCLASS(Config=Game, DefaultConfig)
class DIALOGUEFRAMEWORK_API UDialogueFrameworkSettings : public UDeveloperSettings
{
	GENERATED_BODY()

	UDialogueFrameworkSettings();

protected:

	UPROPERTY(Category=UI, Config, EditDefaultsOnly, meta=(MustImplement="/Script/DialogueFramework.DialogueWidgetInterface"))
	TSoftClassPtr<UUserWidget> DefaultDialogueWidgetClass;

	
public:
	UFUNCTION(Category=DialogueFramework, BlueprintPure)
	TSoftClassPtr<UUserWidget> GetDefaultDialogueWidget() const
	{
		if (DefaultDialogueWidgetClass.IsNull()) return nullptr;
		return DefaultDialogueWidgetClass;
	}


protected:
	
#if WITH_EDITOR
	virtual FText GetSectionText() const override
	{
		return NSLOCTEXT("DialogueFramework", "CoreFrameworkSettingsSection", "Dialogue Framework");
	}

	virtual FText GetSectionDescription() const override
	{
		return NSLOCTEXT("DialogueFramework", "CoreFrameworkSettingsSection", "Default values for Dialogue Framework");
	}

	virtual FName GetContainerName() const override
	{
		return "Project";
	}
#endif
};
