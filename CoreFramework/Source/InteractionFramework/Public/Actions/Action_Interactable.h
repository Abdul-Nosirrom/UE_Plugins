// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ActionSystem/GameplayAction.h"
#include "Components/InteractableComponent.h"
#include "Action_Interactable.generated.h"

class UInteractableComponent;


UCLASS(Blueprintable, BlueprintType, ClassGroup=ActionManager, meta=(DisplayName="Interaction Action"))
class INTERACTIONFRAMEWORK_API UAction_Interactable : public UPrimaryAction
{
	GENERATED_BODY()
public:
	UPROPERTY(Transient)
	mutable UInteractableComponent* Interactable;

protected:
	
	SETUP_PRIMARY_ACTION(TAG_Interaction_Base, false, true, true);
	
	
	UFUNCTION()
	virtual void OnInteractionEnded() { EndAction(); } 
};

