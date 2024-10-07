// Copyright 2023 CoC All rights reserved


#include "Conditions//InteractionCondition_AttributeRequirement.h"

#include "InteractionSystemDebug.h"
#include "Components/AttributeSystemComponent.h"
#include "Components/InteractableComponent.h"
#include "Helpers/ActionFrameworkStatics.h"
#include "Misc/DataValidation.h"

void UInteractionCondition_AttributeRequirement::Initialize(UInteractableComponent* OwnerInteractable)
{
	Super::Initialize(OwnerInteractable);

	AttributeSystem = UActionFrameworkStatics::GetAttributeSystemFromActor(InteractableOwner->GetOwner());

	if (!AttributeSystem.IsValid())
	{
		INTERACTABLE_LOG(Warning, "Interaction Attribute Requirement On Actor With No Attribute System!")
	}
}

bool UInteractionCondition_AttributeRequirement::CanInteract(const UInteractableComponent* OwnerInteractable)
{
	if (!AttributeSystem.IsValid()) return false;

	const float AttributeValue = AttributeSystem->GetAttributeValue(Attribute);
	
	if (ComparisonMethod == EInteractionAttributeComparison::GreaterThan)
	{
		return AttributeValue >= Value;	
	}
	else
	{
		return AttributeValue <= Value;
	}
}

#if WITH_EDITOR

#define LOCTEXT_NAMESPACE "InteractionConditionValidation"

EDataValidationResult UInteractionCondition_AttributeRequirement::IsDataValid(FDataValidationContext& Context)
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	
	if (!UActionFrameworkStatics::GetAttributeSystemFromActor(Cast<UActorComponent>(GetOuter())->GetOwner()))
	{
		FText CurrentError = LOCTEXT("HasASC", "Owner needs to have an attribute system but does not!");
		Context.AddError(CurrentError);
		Result =  EDataValidationResult::Invalid;	
	}

	return Result;
}
#undef LOCTEXT_NAMESPACE

#endif
