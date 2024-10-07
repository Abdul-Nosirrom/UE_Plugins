// Copyright 2023 CoC All rights reserved

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "CombatFrameworkSettings.generated.h"

/**
 * 
 */
UCLASS(Config=Game, DefaultConfig)
class COMBATFRAMEWORK_API UCombatFrameworkSettings : public UDeveloperSettings
{
	GENERATED_BODY()
			
	UCombatFrameworkSettings()
	{
		CategoryName = TEXT("Core Framework");
		SectionName = TEXT("Combat Framework");
	}

protected:

	UPROPERTY(Category=Targeting, Config, EditDefaultsOnly)//, meta=(MustImplement="/Script/DialogueFramework/DialogueWidgetInterface"))
	TEnumAsByte<ECollisionChannel> TargetingTraceChannel;

	UPROPERTY(Category=Targeting, Config, EditDefaultsOnly)//, meta=(MustImplement="/Script/DialogueFramework/DialogueWidgetInterface"))
	FName TargetingProfileName;

	
public:
	UFUNCTION(Category=CombatFramework, BlueprintPure)
	ECollisionChannel GetTargetingTraceChannel() const
	{
		return TargetingTraceChannel;
	}

	UFUNCTION(Category=InteractionFramework, BlueprintPure)
	FName GetTargetingProfileName() const
	{
		return TargetingProfileName;
	}


protected:
	
#if WITH_EDITOR
	virtual FText GetSectionText() const override
	{
		return NSLOCTEXT("CombatFramework", "CoreFrameworkSettingsSection", "Combat Framework");
	}

	virtual FText GetSectionDescription() const override
	{
		return NSLOCTEXT("CombatFramework", "CoreFrameworkSettingsSection", "Default values for Combat Framework");
	}

	virtual FName GetContainerName() const override
	{
		return "Project";
	}
#endif
};
