// Copyright 2023 CoC All rights reserved

#pragma once

#include "CoreMinimal.h"
#include "InteractionSystemSettings.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "InteractionSystemStatics.generated.h"



UCLASS()
class INTERACTIONFRAMEWORK_API UInteractionSystemStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	/*--------------------------------------------------------------------------------------------------------------
	* Developer Settings
	*--------------------------------------------------------------------------------------------------------------*/
	
	/**
	* Returns Dialogue System Settings.
	* ❗ Might return null❗
	*/
	static const UInteractionSystemSettings* GetInteractionFrameworkSettings_Internal()
	{
		return GetDefault<UInteractionSystemSettings>();
	}
	
	/**
	* Tries to get default Dialogue Widget from Project Settings.
	* 
	* ❗ Will return null if settings are not accessible❗
	* ❗ Will return null if no Default Widget is selected❗
	*/
	static TSubclassOf<UInteractableWidgetBase>  GetDefaultInteractableWidget()
	{
		if (GetInteractionFrameworkSettings_Internal() == nullptr) return nullptr;
		
		const TSubclassOf<UInteractableWidgetBase> DefaultClass = GetInteractionFrameworkSettings_Internal()->GetDefaultInteractableWidget().LoadSynchronous();
		return DefaultClass;
	}

	static FGameplayTag GetDefaultInteractableInput()
	{
		return GetInteractionFrameworkSettings_Internal()->GetDefaultInteractableInput();
	}

	static UGameplayActionData* GetDefaultInteractableAction()
	{
		if (GetInteractionFrameworkSettings_Internal() == nullptr) return nullptr;

		return GetInteractionFrameworkSettings_Internal()->GetDefaultInteractableAction().LoadSynchronous();
	}
};
