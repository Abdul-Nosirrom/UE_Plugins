// Copyright 2023 CoC All rights reserved


#include "AttributeSystem/Behaviors/RemoveOtherEntityEffectsBehavior.h"

#include "Components/AttributeSystemComponent.h"

void URemoveOtherEntityEffectsBehavior::PerformRemove(UAttributeSystemComponent* AttributeSystemComponent) const
{
	FEntityEffectQuery FindOwnerQuery;
	FindOwnerQuery.EffectDefinition = GetOwner() ? GetOwner()->GetClass() : nullptr;

	// We need to keep track to ensure we never remove ourselves
	TArray<FActiveEntityEffectHandle> ActiveEEHandles = AttributeSystemComponent->GetActiveEntityEffects(FindOwnerQuery);

	for (const FEntityEffectQuery& RemoveQuery : RemoveEntityEffectQueries)
	{
		if (!RemoveQuery.IsEmpty())
		{
			// If we have an ActiveGEHandle, make sure we never remove ourselves.
			// If we don't, there's no need to make a copy.
			if (ActiveEEHandles.IsEmpty())
			{
				// Faster path: No copy needed
				AttributeSystemComponent->RemoveActiveEntityEffects(RemoveQuery);
			}
			else
			{
				FEntityEffectQuery MutableRemoveQuery = RemoveQuery;
				MutableRemoveQuery.IgnoreHandles = MoveTemp(ActiveEEHandles);

				AttributeSystemComponent->RemoveActiveEntityEffects(MutableRemoveQuery);
			}
		}
	}
}
