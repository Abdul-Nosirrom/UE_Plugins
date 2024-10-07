// Copyright 2023 CoC All rights reserved

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "InteractionScript.generated.h"



UCLASS(Blueprintable, EditInlineNew, DefaultToInstanced, Abstract)
class INTERACTIONFRAMEWORK_API UInteractionScript : public UObject
{
	GENERATED_BODY()

	friend class UInteractableComponent;
	
protected:
	
	UPROPERTY()
	UInteractableComponent* InteractableOwner;
	
	virtual void Initialize(UInteractableComponent* OwnerInteractable) { InteractableOwner = OwnerInteractable; }

	class ARadicalCharacter* GetPlayerCharacter() const;
	class UActionSystemComponent* GetActionSystem() const;

	virtual UWorld* GetWorld() const override;

#if WITH_EDITOR
	virtual TArray<AActor*> GetWorldReferences() const { return TArray<AActor*>(); }
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) override;
#endif 
};
