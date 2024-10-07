// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "InputBufferSettings.generated.h"

/**
 * 
 */
UCLASS(Config=Game, DefaultConfig)
class COREFRAMEWORK_API UInputBufferSettings : public UDeveloperSettings
{
	GENERATED_BODY()

	UInputBufferSettings();

protected:
	/// @brief  Number of frames that that is buffered
	UPROPERTY(Category=InputBuffer, Config, EditDefaultsOnly, meta=(UIMin=0, ClampMin=0))
	uint8 FullFrameWindow			= 40;
	/// @brief  Number of frames in which button actions are valid in the buffer
	UPROPERTY(Category=InputBuffer, Config, EditDefaultsOnly, meta=(UIMin=0, ClampMin=0))
	uint8 ButtonFrameWindow	= 12;
	/// @brief  Framerate in which the input buffer is updated
	UPROPERTY(Category=InputBuffer, Config, EditDefaultsOnly, meta=(UIMin=0, ClampMin=0, UIMax=120, ClampMax=120))
	float UpdateFrameRate			= 60;

public:

	UFUNCTION(Category=InputBufferSettings, BlueprintPure)
	uint8 GetBufferFullFrameWindow() const { return FullFrameWindow; }
	UFUNCTION(Category=InputBufferSettings, BlueprintPure)
	uint8 GetBufferButtonFrameWindow() const { return ButtonFrameWindow; }
	UFUNCTION(Category=InputBufferSettings, BlueprintPure)
	float GetBufferTickInterval() const { return 1/UpdateFrameRate; }
	
protected:
	
#if WITH_EDITOR
	virtual FText GetSectionText() const override
	{
		return NSLOCTEXT("InputBuffer", "CoreFrameworkSettingsSection", "Input Buffer");
	}

	virtual FText GetSectionDescription() const override
	{
		return NSLOCTEXT("InputBuffer", "CoreFrameworkSettingsSection", "Default values for Input Buffer");
	}

	virtual FName GetContainerName() const override
	{
		return "Project";
	}
#endif
};
