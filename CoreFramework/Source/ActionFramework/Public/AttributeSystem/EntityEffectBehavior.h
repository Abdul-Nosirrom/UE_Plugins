// Copyright 2023 CoC All rights reserved

#pragma once

#include "CoreMinimal.h"
#include "EntityEffectBehavior.generated.h"

/* Forward Declarations */
class UEntityEffect;
class UAttributeSystemComponent;
struct FActiveEntityEffect;
struct FEntityEffectSpec;


/// @brief	Define how an attribute effect behaves
UCLASS(Abstract, Const, EditInlineNew, CollapseCategories, Within=EntityEffect)
class ACTIONFRAMEWORK_API UEntityEffectBehavior : public UObject
{
	GENERATED_BODY()

public:
	
	UEntityEffect* GetOwner() const
	{
		return GetTypedOuter<UEntityEffect>();
	}

	/// @brief	If false, the effect will be discard
	virtual bool CanEntityEffectApply(const UAttributeSystemComponent* AttributeSystem, const FEntityEffectSpec& Spec) const { return true; }

	/// @brief	Called when the effect is first added to the container, by returning false here, the effect will start inhibited
	virtual bool OnActiveEntityEffectAdded(const UAttributeSystemComponent* AttributeSystem, FActiveEntityEffect& ActiveEE) const { return true; }

	/// @brief	Called everytime the effect is executed
	virtual void OnEntityEffectExecuted(UAttributeSystemComponent* AttributeSystem, FEntityEffectSpec& Spec) const {}

	/// @brief	Called when
	virtual void OnEntityEffectApplied(UAttributeSystemComponent* AttributeSystem, FActiveEntityEffect& ActiveEffect) const {}

	/// @brief	Called when
	virtual void OnEntityEffectRemoved(UAttributeSystemComponent* AttributeSystem, FActiveEntityEffect& ActiveEffect, bool bPrematureRemoval) const {}

#if WITH_EDITORONLY_DATA
	/// @brief	Friendly name for displaying in the Editor's Gameplay Effect Component Index. We set EditCondition False here so it doesn't show up otherwise. */
	UPROPERTY(VisibleDefaultsOnly, Transient, Category=AlwaysHidden, Meta=(EditCondition=False, EditConditionHides))
	FString EditorFriendlyName;
#endif

#if WITH_EDITOR
	/// @brief  Default validation just ensures there is only one of each behavior on a given effect
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif 
};
