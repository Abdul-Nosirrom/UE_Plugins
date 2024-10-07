// Copyright 2023 CoC All rights reserved


#include "AttributeSystem/Behaviors/ImmunityEntityEffectBehavior.h"

#include "AttributeSystem/EntityEffect.h"
#include "Components/AttributeSystemComponent.h"
#include "Debug/AttributeLog.h"
#include "Misc/DataValidation.h"

#define LOCTEXT_NAMESPACE "ImmunityEntityEffectBehavior"

#if WITH_EDITOR 
EDataValidationResult UImmunityEntityEffectBehavior::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	if (GetOwner()->DurationPolicy == EEntityEffectDurationType::Instant)
	{
		Context.AddError(FText::FormatOrdered(LOCTEXT("ImmunityNotAvailable", "EE is an Instant Effect and incompatible with {0}"), FText::FromString(GetClass()->GetName())));
		Result = EDataValidationResult::Invalid;
	}
	
	return Result;
}
#endif

void UImmunityEntityEffectBehavior::OnEntityEffectApplied(UAttributeSystemComponent* AttributeSystem,
                                                          FActiveEntityEffect& ActiveEffect) const
{
	// No immunity on InstantEffects, so lets check for that
	if (ActiveEffect.Spec.Def && ActiveEffect.Spec.Def->DurationPolicy == EEntityEffectDurationType::Instant) return;

	// Bind to application query
	FEntityEffectApplicationQuery& BoundQuery = AttributeSystem->EntityEffectApplicationQueries.AddDefaulted_GetRef();
	BoundQuery.BindUObject(this, &UImmunityEntityEffectBehavior::QueryEffectBeingApplied);

	ActiveEffect.EventSet.OnEffectRemoved.AddLambda([AttributeSystem, QueryToRemove = BoundQuery.GetHandle()](const FEntityEffectRemovalInfo& RemovalInfo)
	{
		if (ensure(IsValid(AttributeSystem)))
		{
			// Remove our binding
			for (auto It = AttributeSystem->EntityEffectApplicationQueries.CreateIterator(); It; ++It)
			{
				if (It->GetHandle() == QueryToRemove)
				{
					It.RemoveCurrentSwap();
					break;
				}
			}
		}
	});
}

bool UImmunityEntityEffectBehavior::QueryEffectBeingApplied(const UAttributeSystemComponent* AttributeSystem, const FEntityEffectSpec& Spec) const
{
	for (const FEntityEffectQuery& ImmunityQuery : ImmunityQueries)
	{
		if (!ImmunityQuery.IsEmpty() && ImmunityQuery.Matches(Spec))
		{
			AttributeSystem->OnImmunityBlockEntityEffectDelegate.Broadcast(Spec);
			return false;
		}
	}
	return true;
}

#undef LOCTEXT_NAMESPACE