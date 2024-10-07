// Copyright 2023 CoC All rights reserved

#pragma once

#include "CoreMinimal.h"
#include "Data/TargetingPreset.h"
#include "Helpers/CombatFrameworkSettings.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "TargetingStatics.generated.h"

/**
 * 
 */
UCLASS()
class COMBATFRAMEWORK_API UTargetingStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	
	/// @brief  Performs a targeting operation based on the settings. Returns true if a target was found, otherwise false.
	UFUNCTION(Category="Targeting", BlueprintCallable, meta=(DefaultToSelf="Instigator"))
	static bool PerformTargeting(AActor* Instigator, FTargetingSettings TargetingSettings, FTargetingResult& OutResult);

	/// @brief  Performs a targeting operation based on the settings. Returns true if a target was found, otherwise false.
	UFUNCTION(Category="Targeting", BlueprintCallable, meta=(DefaultToSelf="Instigator"))
	static bool PerformTargetingFromPreset(AActor* Instigator, UTargetingPreset* TargetingSettings, FTargetingResult& OutResult);

	/// @brief	Returns Targeting Trace Channel
	UFUNCTION(Category="Targeting", BlueprintPure)
	static ECollisionChannel GetTargetingTraceChannel();

	/// @brief	Returns Targeting Profile
	UFUNCTION(Category="Targeting", BlueprintPure)
	static FName GetTargetingCollisionProfile();

protected:
	/// @brief	Returns CombatFramework settings, might be null
	static const UCombatFrameworkSettings* GetCombatFrameworkSettings_Internal()
	{
		return GetDefault<UCombatFrameworkSettings>();
	}
	
};
