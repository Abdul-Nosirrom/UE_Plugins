// Copyright 2023 CoC All rights reserved


#include "Components/InteractionScript.h"

#include "Components/InteractableComponent.h"
#include "Misc/DataValidation.h"

ARadicalCharacter* UInteractionScript::GetPlayerCharacter() const
{
	return InteractableOwner->PlayerCharacter.Get();
}

UActionSystemComponent* UInteractionScript::GetActionSystem() const
{
	return InteractableOwner->ActionSystemComponent.Get();
}

UWorld* UInteractionScript::GetWorld() const
{
	if (InteractableOwner)
	{
		return InteractableOwner->GetWorld();
	}
	return nullptr;
}

#if WITH_EDITOR 
#define LOCTEXT_NAMESPACE "InteractionScriptValidation"
EDataValidationResult UInteractionScript::IsDataValid(FDataValidationContext& Context)
{
	EDataValidationResult Result = EDataValidationResult::NotValidated;
	
	for (auto WorldRef : GetWorldReferences())
	{
		if (!WorldRef)
		{
			FText CurrentError = FText::FormatOrdered(LOCTEXT("IsNull", "[{0}] One or more of referenced actors is null"), FText::FromString(StaticClass()->GetName()));
			Context.AddError(CurrentError);
			Result =  EDataValidationResult::Invalid;
			break;
		}
	}

	return Result;
}
#undef LOCTEXT_NAMESPACE
#endif