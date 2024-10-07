// Copyright 2023 CoC All rights reserved

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Engine/DeveloperSettings.h"
#include "InteractionSystemSettings.generated.h"


class UInputAction;
class UInteractableWidgetBase;
class UGameplayActionData;

UCLASS(Config=Game, DefaultConfig)
class INTERACTIONFRAMEWORK_API UInteractionSystemSettings : public UDeveloperSettings
{
	GENERATED_BODY()

		
	UInteractionSystemSettings();

protected:

	UPROPERTY(Category=Interactables, Config, EditAnywhere, meta=(GameplayTagFilter="Input.Button"))
	FGameplayTag DefaultInteractionInput;

	UPROPERTY(Category=Interactables, Config, EditAnywhere)
	TSoftClassPtr<UInteractableWidgetBase> DefaultInteractableWidgetClass;

	UPROPERTY(Category=Interactables, Config, EditAnywhere)
	TSoftObjectPtr<UGameplayActionData> DefaultInteractableAction;
	
public:
	UFUNCTION(Category=InteractionFramework, BlueprintPure)
	TSoftClassPtr<UInteractableWidgetBase> GetDefaultInteractableWidget() const
	{
		if (DefaultInteractableWidgetClass.IsNull()) return nullptr;
		return DefaultInteractableWidgetClass;
	}

	UFUNCTION(Category=InteractionFramework, BlueprintPure)
	FGameplayTag GetDefaultInteractableInput() const
	{
		return DefaultInteractionInput;
	}

	UFUNCTION(Category=InteractionFramework, BlueprintPure)
	TSoftObjectPtr<UGameplayActionData> GetDefaultInteractableAction() const
	{
		if (DefaultInteractableAction.IsNull()) return nullptr;
		return DefaultInteractableAction;
	}

protected:
	
#if WITH_EDITOR
	virtual FText GetSectionText() const override
	{
		return NSLOCTEXT("InteractionFramework", "CoreFrameworkSettingsSection", "Interaction Framework");
	}

	virtual FText GetSectionDescription() const override
	{
		return NSLOCTEXT("InteractionFramework", "CoreFrameworkSettingsSection", "Default values for Interaction Framework");
	}

	virtual FName GetContainerName() const override
	{
		return "Project";
	}
#endif
};
