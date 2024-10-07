// Copyright 2023 CoC All rights reserved

#pragma once

#include "CoreMinimal.h"
#include "AttributeModifierTypes.h"
#include "Data/GameplayTagData.h"
#include "EntityEffect.generated.h"

class UAttributeSystemComponent;
class UEntityEffectBehavior;

UENUM()
enum class EEntityEffectDurationType : uint8
{
	/// @brief	This effect applies instantly 
	Instant,
	/// @brief	This effect lasts forever 
	Infinite,
	/// @brief	The duration of this effect will be specified by a magnitude 
	HasDuration
};

UCLASS(ClassGroup=Actions, Category="Gameplay Actions", EditInlineNew, DisplayName="Entity Effect", Blueprintable, PrioritizeCategories="Status Duration EntityEffect", meta = (ShortTooltip = "An EntityEffect modifies attributes and tags."))
class ACTIONFRAMEWORK_API UEntityEffect : public UObject
{
public:
	
	GENERATED_BODY()
	
	bool CanApply(UAttributeSystemComponent* AttributeSystem, const FEntityEffectSpec& EESpec) const;
	void OnExecuted(UAttributeSystemComponent* AttributeSystem, FEntityEffectSpec& EESpec) const;
	void OnApplied(UAttributeSystemComponent* AttributeSystem, FActiveEntityEffect& ActiveEE) const;
	void OnRemoved(UAttributeSystemComponent* AttributeSystem, FActiveEntityEffect& ActiveEE, bool bPrematureRemoval) const;

	template<typename EEBehaviorClass>
	const EEBehaviorClass* FindBehavior() const;
	const UEntityEffectBehavior* FindBehavior(TSubclassOf<UEntityEffectBehavior> ClassToFind) const;
	template<typename EEBehaviorClass>
	EEBehaviorClass& AddBehavior();
	template<typename EEBehaviorClass>
	EEBehaviorClass& FindOrAddBehavior();

	const FGameplayTagContainer& GetEffectTags() const { return EffectTags.CombinedTags; } 

//protected:
#if WITH_EDITOR
	virtual void PostLoad() override;
	virtual void PostCDOCompiled(const FPostCDOCompiledContext& Context) override;
	
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
	
#if WITH_EDITORONLY_DATA
	/// @brief	Allow us to show the Status of the class (valid configurations or invalid configurations) while configuring in the Editor 
	UPROPERTY(Category = Status, VisibleAnywhere, Transient)
	mutable FText EditorStatusText;
#endif 
	
public:
	
	/// @brief	These tags define what this effect is
	UPROPERTY(Category="Definition", EditDefaultsOnly)
	FInheritedTagContainer EffectTags;

	UPROPERTY(Category="Duration", EditDefaultsOnly)
	EEntityEffectDurationType DurationPolicy;

	UPROPERTY(Category="Duration", EditDefaultsOnly, meta=(EditCondition="DurationPolicy==EEntityEffectDurationType::HasDuration", EditConditionHides))
	FEntityEffectModifierMagnitude DurationMagnitude;

	UPROPERTY(Category="Duration|Period", EditDefaultsOnly, BlueprintReadOnly, meta=(EditCondition="DurationPolicy!=EEntityEffectDurationType::Instant", EditConditionHides))
	float Period;

	UPROPERTY(Category="Duration|Period", EditDefaultsOnly, BlueprintReadOnly, meta=(EditCondition="DurationPolicy!=EEntityEffectDurationType::Instant", EditConditionHides))
	bool bExecutePeriodicEffectOnApplication;

	UPROPERTY(Category="EntityEffect", EditDefaultsOnly, BlueprintReadOnly, meta=(TitleProperty=Attribute))
	TArray<FAttributeModifierInfo> Modifiers;

	UPROPERTY(Category="EntityEffect", EditDefaultsOnly, BlueprintReadOnly, Instanced, meta = (DisplayName = "Behaviors", TitleProperty = EditorFriendlyName, ShowOnlyInnerProperties, DisplayPriority = 0))
	TArray<TObjectPtr<UEntityEffectBehavior>> EffectBehaviors;
};

template<typename EEBehaviorClass>
const EEBehaviorClass* UEntityEffect::FindBehavior() const
{
	static_assert(TIsDerivedFrom<EEBehaviorClass, UEntityEffectBehavior>::IsDerived, "EEBehaviorClass must be derived from UEntityEffectBehavior");

	for (const TObjectPtr<UEntityEffectBehavior>& GEComponent : EffectBehaviors)
	{
		if (EEBehaviorClass* CastComponent = Cast<EEBehaviorClass>(GEComponent))
		{
			return CastComponent;
		}
	}

	return nullptr;
}

template<typename EEBehaviorClass>
EEBehaviorClass& UEntityEffect::AddBehavior()
{
	static_assert( TIsDerivedFrom<EEBehaviorClass, UEntityEffectBehavior>::IsDerived, "EEBehaviorClass must be derived from UEntityEffectBehavior");

	TObjectPtr<EEBehaviorClass> Instance = NewObject<EEBehaviorClass>(this, NAME_None, GetMaskedFlags(RF_PropagateToSubObjects) | RF_Transactional);
	EffectBehaviors.Add(Instance);

	check(Instance);
	return *Instance;
}

template<typename EEBehaviorClass>
EEBehaviorClass& UEntityEffect::FindOrAddBehavior()
{
	static_assert(TIsDerivedFrom<EEBehaviorClass, UEntityEffectBehavior>::IsDerived, "EEBehaviorClass must be derived from UEntityEffectBehavior");

	for (const TObjectPtr<UEntityEffectBehavior>& GEComponent : EffectBehaviors)
	{
		if (EEBehaviorClass* CastComponent = Cast<EEBehaviorClass>(GEComponent))
		{
			return *CastComponent;
		}
	}

	return AddBehavior<EEBehaviorClass>();
}