// Copyright 2023 CoC All rights reserved

#pragma once

#include "CoreMinimal.h"
#include "AttributeSystem/EntityEffectBehavior.h"
#include "AttributeSystem/EntityEffectTypes.h"
#include "RemoveOtherEntityEffectsBehavior.generated.h"

/**
 * 
 */
UCLASS()
class ACTIONFRAMEWORK_API URemoveOtherEntityEffectsBehavior : public UEntityEffectBehavior
{
	GENERATED_BODY()

	URemoveOtherEntityEffectsBehavior()
	{
#if WITH_EDITORONLY_DATA
		EditorFriendlyName = TEXT("Remove Other Entity Effects");
#endif 
	}

	virtual void OnEntityEffectExecuted(UAttributeSystemComponent* AttributeSystem, FEntityEffectSpec& Spec) const override
	{
		PerformRemove(AttributeSystem);
	}
	virtual void OnEntityEffectApplied(UAttributeSystemComponent* AttributeSystem, FActiveEntityEffect& ActiveEffect) const override
	{
		PerformRemove(AttributeSystem);
	}

	void PerformRemove(UAttributeSystemComponent* AttributeSystemComponent) const;
	
public:
	UPROPERTY(Category=None, EditDefaultsOnly)
	TArray<FEntityEffectQuery> RemoveEntityEffectQueries;
};
