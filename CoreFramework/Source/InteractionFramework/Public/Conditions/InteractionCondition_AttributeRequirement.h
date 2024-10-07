// Copyright 2023 CoC All rights reserved

#pragma once

#include "CoreMinimal.h"
#include "AttributeSystem/EntityAttributeSet.h"
#include "Conditions/InteractionCondition.h"
#include "InteractionCondition_AttributeRequirement.generated.h"


UENUM(BlueprintType)
enum class EInteractionAttributeComparison : uint8
{
	GreaterThan	UMETA(DisplayName=">="),
	LessThan	UMETA(DisplayName="<=")
};
	

UCLASS(DisplayName="Attribute Requirement", MinimalAPI)
class UInteractionCondition_AttributeRequirement : public UInteractionCondition
{
	GENERATED_BODY()
protected:
	
	UPROPERTY(EditAnywhere)
	FEntityAttribute Attribute;
	UPROPERTY(EditAnywhere, DisplayName="")
	EInteractionAttributeComparison ComparisonMethod;
	UPROPERTY(EditAnywhere)
	float Value;
	
	TWeakObjectPtr<class UAttributeSystemComponent> AttributeSystem;

	virtual void Initialize(UInteractableComponent* OwnerInteractable) override;
	virtual bool CanInteract(const UInteractableComponent* OwnerInteractable) override;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) override;
#endif 
};
