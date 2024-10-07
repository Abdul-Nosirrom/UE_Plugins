// Copyright 2023 CoC All rights reserved


#include "AttributeSystem/EntityEffectBehavior.h"

#include "AttributeSystem/EntityEffect.h"
#include "Misc/DataValidation.h"

#define LOCTEXT_NAMESPACE "EntityEffectBehavior"

#if WITH_EDITOR 
EDataValidationResult UEntityEffectBehavior::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = UObject::IsDataValid(Context);

	TSubclassOf<UEntityEffectBehavior> ThisClass = GetClass();

	const UEntityEffect* Owner = GetOwner();
	if (Owner)
	{
		const UEntityEffectBehavior* FirstBehaviorOfThisType = Owner->FindBehavior(ThisClass);
		if (!FirstBehaviorOfThisType)
		{
			Context.AddError(LOCTEXT("NullEffectBehavior", "Component Does Not Exist In Its Owners EEBehaviors Array"));
			Result = EDataValidationResult::Invalid;
		}
		else if (FirstBehaviorOfThisType != this)
		{
			Context.AddError(FText::FormatOrdered(LOCTEXT("DuplicateEffectBehavior", "Two or more types of {0} exist on EE"), FText::FromString(ThisClass->GetName())));
			Result = EDataValidationResult::Invalid;
		}
	}

	// Enough validation here, child classes can invalidate
	if (Result == EDataValidationResult::NotValidated)
	{
		Result = EDataValidationResult::Valid;
	}

	return Result;
}
#endif

#undef LOCTEXT_NAMESPACE