// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "InputBufferSettings.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CoreFrameworkStatics.generated.h"

/**
 * 
 */
UCLASS()
class COREFRAMEWORK_API UCoreFrameworkStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	* Returns Input Buffer Settings
	* ❗ Might return null❗
	*/
	static const UInputBufferSettings* GetInputBufferSettings_Internal()
	{
		return GetDefault<UInputBufferSettings>();
	}

	static uint8 GetInputBufferFullFrameWindow()
	{
		if (GetInputBufferSettings_Internal() == nullptr) return 0;
		
		return GetInputBufferSettings_Internal()->GetBufferFullFrameWindow();
	}
	static uint8 GetInputBufferButtonFrameWindow()
	{
		if (GetInputBufferSettings_Internal() == nullptr) return 0;
		
		return GetInputBufferSettings_Internal()->GetBufferButtonFrameWindow();
	}
	static float GetInputBufferTickInterval()
	{
		if (GetInputBufferSettings_Internal() == nullptr) return 1;
		
		return GetInputBufferSettings_Internal()->GetBufferTickInterval();
	}
};
