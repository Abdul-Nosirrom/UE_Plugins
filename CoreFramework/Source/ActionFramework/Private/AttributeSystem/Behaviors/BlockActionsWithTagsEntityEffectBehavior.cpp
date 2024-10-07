// Copyright 2023 CoC All rights reserved


#include "AttributeSystem/Behaviors/BlockActionsWithTagsEntityEffectBehavior.h"

#include "Components/ActionSystemComponent.h"
#include "Components/AttributeSystemComponent.h"

void UBlockActionsWithTagsEntityEffectBehavior::OnEntityEffectApplied(UAttributeSystemComponent* AttributeSystem,
                                                                      FActiveEntityEffect& ActiveEffect) const
{
	if (!AttributeSystem) return;

	if (AttributeSystem->GetActorInfo().ActionSystemComponent.IsValid())
	{
		AttributeSystem->GetActorInfo().ActionSystemComponent->BlockActionsWithTags(InheritableBlockedAbilityTagsContainer);
	}
}

void UBlockActionsWithTagsEntityEffectBehavior::OnEntityEffectRemoved(UAttributeSystemComponent* AttributeSystem,
	FActiveEntityEffect& ActiveEffect, bool bPrematureRemoval) const
{
	if (!AttributeSystem) return;

	if (AttributeSystem->GetActorInfo().ActionSystemComponent.IsValid())
	{
		AttributeSystem->GetActorInfo().ActionSystemComponent->UnBlockActionsWithTags(InheritableBlockedAbilityTagsContainer);
	}
}
