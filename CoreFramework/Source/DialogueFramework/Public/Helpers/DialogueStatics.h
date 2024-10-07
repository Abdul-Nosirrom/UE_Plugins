// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Helpers/DialogueFrameworkSettings.h"
#include "DialogueStatics.generated.h"

/**
 * 
 */
UCLASS()
class DIALOGUEFRAMEWORK_API UDialogueStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	* Returns Dialogue System Settings.
	* ❗ Might return null❗
	*/
	static const UDialogueFrameworkSettings* GetDialogueSystemSettings_Internal()
	{
		return GetDefault<UDialogueFrameworkSettings>();
	}

	/**
	* Tries to get default Dialogue Widget from Project Settings.
	* 
	* ❗ Will return null if settings are not accessible❗
	* ❗ Will return null if no Default Widget is selected❗
	*/
	static TSubclassOf<UUserWidget>  GetDefaultDialogueWidget()
	{
		if (GetDialogueSystemSettings_Internal() == nullptr) return nullptr;
		
		const TSubclassOf<UUserWidget> DefaultClass = GetDialogueSystemSettings_Internal()->GetDefaultDialogueWidget().LoadSynchronous();
		return DefaultClass;
	}
};
