// Copyright 2023 CoC All rights reserved

#pragma once

#include "CoreMinimal.h"
#include "EntityAttributeSet.h"
#include "AttributeModifierTypes.generated.h"

/* Forward Declarations */
struct FEntityEffectSpec;
struct FActiveEntityEffect;

/// @brief	Modifier Operations
UENUM(BlueprintType)
enum class EAttributeModifierOp : uint8
{
	Add			= 0,
	Multiply	= 1,
	Divide		= 2,
	Override	= 3,

	Max			UMETA(Hidden)
};

/*--------------------------------------------------------------------------------------------------------------
* Attribute Capture
*--------------------------------------------------------------------------------------------------------------*/

/// @brief	Specifier of whose attributes/data to capture for a given attribute capture (e.g source or target?)
UENUM()
enum class EAttributeCaptureSource : uint8 
{
	/// @brief	Source (caster) of the effect
	Source,
	/// @brief	Target (recipient) of the effect
	Target
};

/// @brief	Attribute capture used by AttributeBased modifier magnitudes
USTRUCT(BlueprintType)
struct ACTIONFRAMEWORK_API FEntityEffectAttributeCapture
{
	GENERATED_BODY()

	UPROPERTY(Category=Capture, EditDefaultsOnly)
	FEntityAttribute AttributeToCapture;

	UPROPERTY(Category=Capture, EditDefaultsOnly)
	EAttributeCaptureSource CaptureSource;

	UPROPERTY(Category=Capture, EditDefaultsOnly)
	bool bSnapshot;
};

/*--------------------------------------------------------------------------------------------------------------
* Modifier Magnitudes
*--------------------------------------------------------------------------------------------------------------*/

UENUM()
enum class EAttributeModifierCalculationPolicy : uint8
{
	/// @brief	Simple float operation
	FloatBased,
	/// @brief	Operation whose magnitude is dependent on an attribute on the source or target
	AttributeBased
};

/// @brief	Enumeration outlining the possible attribute based float calculation policies. 
UENUM()
enum class EAttributeBasedCalculationType : uint8
{
	/// @brief	Use the final evaluated magnitude of the attribute (i.e CurrentValue)
	AttributeMagnitude,
	/// @brief	Use the BaseValue of the attribute
	AttributeBaseValue,
	/// @brief	Use the "bonus" evaluated magnitude of the attribute: Equivalent to (CurrentValue - BaseValue). */
	AttributeBonusMagnitude,
};

/// @brief	Specifies a modifier magnitude that's dependent on some other attribute that may exist on either the Source
///			attribute system or the Target attribute system.
///
///			Calculation = Coefficient * ([AttrVal] + PreMultiplyAdditive) + PostMultiplyAdditive
USTRUCT()
struct FAttributeBasedMagnitude
{
	GENERATED_BODY()

	FAttributeBasedMagnitude()
		: Coefficient(1.f)
		, PreMultiplyAdditiveValue(0.f)
		, PostMultiplyAdditiveValue(0.f)
		, BackingAttribute()
	{}
	
	float CalculateMagnitude(const FEntityEffectSpec& InRelevantSpec) const;
	
	/// @brief  Calculation = Coefficient * ([AttrVal] + PreMultiplyAdditive) + PostMultiplyAdditive
	UPROPERTY(Category=AttributeBased, EditDefaultsOnly)
	float Coefficient;
	/// @brief  Calculation = Coefficient * ([AttrVal] + PreMultiplyAdditive) + PostMultiplyAdditive
	UPROPERTY(Category=AttributeBased, EditDefaultsOnly)
	float PreMultiplyAdditiveValue;
	/// @brief  Calculation = Coefficient * ([AttrVal] + PreMultiplyAdditive) + PostMultiplyAdditive
	UPROPERTY(Category=AttributeBased, EditDefaultsOnly)
	float PostMultiplyAdditiveValue;

	UPROPERTY(Category=AttributeBased, EditDefaultsOnly)
	FEntityEffectAttributeCapture BackingAttribute;

	UPROPERTY(Category=AttributeBased, EditDefaultsOnly)
	EAttributeBasedCalculationType AttributeCalculationType;
	
	/// @brief  Captured base value of the requested attribute
	float AttributeBaseCaptureValue;
	
	/// @brief  Captured current value of the requested attribute
	float AttributeCaptureValue;
};

/// @brief	Represents the magnitude of an Entity Effect Modifier, potentially calculated in numerous different ways. This is the value
///			used in an attribute modifier. E.g, if our attribute modifier specifies [Attr = Attr + X], X is the "ModifierMagnitude" and
///			ModifierInfo specified an "Add" calculation operation
USTRUCT()
struct ACTIONFRAMEWORK_API FEntityEffectModifierMagnitude
{
	friend class UEntityEffect;

	GENERATED_BODY()

public:
	
	/// @brief  Attempts to calculate the magnitude given the provided effect. May fail if information (e.g CapturedAttribute) is missing from the spec
	/// @param  InRelevantSpec EntityEffect to use for magnitude calculation
	/// @param  OutCalculatedMagnitude [OUT] Calculated value of the magnitude. Set to 0.f if failed to calculate
	/// @return True if calculation was successful
	bool AttemptCalculateMagnitude(const FEntityEffectSpec& InRelevantSpec, OUT float& OutCalculatedMagnitude) const;
	
	bool IsNonSnapshot() const;

	bool PerformAttributeCapture(const FEntityEffectSpec& InRelevantSpec, bool bSourceOnly=false);

protected:

	UPROPERTY(Category=Magnitude, EditDefaultsOnly)
	EAttributeModifierCalculationPolicy MagnitudeCalculationPolicy;
	
	UPROPERTY(Category=Magnitude, EditDefaultsOnly, meta=(DisplayName="Magnitude", EditCondition="MagnitudeCalculationPolicy==EAttributeModifierCalculationPolicy::FloatBased", EditConditionHides))
	float FloatBasedMagnitude;

	UPROPERTY(Category=Magnitude, EditDefaultsOnly, meta=(DisplayName="Magnitude", EditCondition="MagnitudeCalculationPolicy==EAttributeModifierCalculationPolicy::AttributeBased", EditConditionHides))
	FAttributeBasedMagnitude AttributeBasedMagnitude;

	
	/// @brief  Dependent on AttributeBased modifiers if we can find its captures or if its backing attribute is valid
	bool bCanCalculate = true;
};


/// @brief	Information struct that specifies:
///			- What attribute is modified
///			- What operation to apply for the modification
///			Does not specify by *what value* to modify, that is calculated by the ModifierMagnitude
USTRUCT(BlueprintType)
struct ACTIONFRAMEWORK_API FAttributeModifierInfo
{
	GENERATED_BODY()
	
	/// @brief  The attribute we modify
	UPROPERTY(Category=AttributeModifier, EditDefaultsOnly)
	FEntityAttribute Attribute;
	
	/// @brief  The numeric operation of this modifier
	UPROPERTY(Category=AttributeModifier, EditDefaultsOnly)
	EAttributeModifierOp ModifierOp = EAttributeModifierOp::Add;
	
	/// @brief  Magnitude of the modifier. Change in attribute is [Attribute = ModifierOp(Attribute, ModifierMagnitude)]
	UPROPERTY(Category=AttributeModifier, EditDefaultsOnly)
	FEntityEffectModifierMagnitude ModifierMagnitude;

	bool operator==(const FAttributeModifierInfo& Other) const;
	bool operator!=(const FAttributeModifierInfo& Other) const;

	FORCEINLINE float GetEvaluatedMagnitude() const { return EvaluatedMagnitude; }

protected:
	friend struct FEntityEffectSpec;
	
	float EvaluatedMagnitude = 0.f;
};


/// @brief	Lives on AttributeSystem. Collects all ModInfos applying to a given attribute, then computes their
///			evaluated result. E.g, health has 5 "Add" ops, 3 "Multiply" ops, 3 "Divide" ops - result is:
///			- AddSum = Sum[AddMods]; MulSum = Sum[MultiplyMods]; DivSum = Sum[DivideMods];
///			- CurrentAttributeValue = (BaseValue + AddSum) * MulSum / DivSum;
///			If has an Overwrite Mod, it'll just apply the first Override mod that 'qualifies'
USTRUCT()
struct ACTIONFRAMEWORK_API FAttributeModifierAggregator
{
	GENERATED_BODY()

	FAttributeModifierAggregator();

	static float ExecModOnBaseValue(float InBaseValue, EAttributeModifierOp ModifierOp, float EvaluatedMagnitude);

	float EvaluateWithBase(float InBaseValue) const;
	
	void AddAggregatorMod(int32 ModIdx, FActiveEntityEffect& ActiveEffect);

	void RemoveAggregatorMod(const FActiveEntityEffect& ActiveEffect);
	
	/// @brief	Aggregator is valid if it has any active modifiers on it. If no modifiers exist on this aggregator, its not "valid"
	bool IsValid()
	{
		bool bValid = false;
		for (const auto& Mod : OpModMap)
		{
			bValid |= !Mod.Value.IsEmpty();
		}

		return bValid;
	}
	
private:

	// TODO: Maybe this can store a pointer to the ModInfo on the ActiveEffect and the ActiveEffect handle, smaller size for it like that
	struct FAggregatorElement
	{
		FAggregatorElement(int32 InModIdx, FActiveEntityEffect* InActiveEffect) : ModIdx(InModIdx), ActiveEffect(InActiveEffect) {}

		int32 ModIdx;
		FActiveEntityEffect* ActiveEffect;

		bool operator==(const FAggregatorElement& Other) const
		{
			return (ModIdx == Other.ModIdx) && (ActiveEffect == Other.ActiveEffect);
		}
	};

	float SumMods(const TArray<FAggregatorElement>& Mods, float Bias) const;
	
	TMap<EAttributeModifierOp, TArray<FAggregatorElement>> OpModMap;

	inline static float ModBiases[] = {0.f, 1.f, 1.f, 0.f};
};

/*--------------------------------------------------------------------------------------------------------------
* Callback/Event Data
*--------------------------------------------------------------------------------------------------------------*/


/// @brief	Description of what happened in an attribute modification. Used for callbacks once we execute an effect for any listeners
USTRUCT(BlueprintType)
struct FAttributeModifierEvaluatedData
{
	GENERATED_BODY()

	FAttributeModifierEvaluatedData()
		: Attribute()
		, ModifierOp(EAttributeModifierOp::Add)
		, Magnitude(0.f)
		, bIsValid(false)
	{}

	FAttributeModifierEvaluatedData(const FEntityAttribute& InAttribute, EAttributeModifierOp InModOp, float InMagnitude)
		: Attribute(InAttribute)
		, ModifierOp(InModOp)
		, Magnitude(InMagnitude)
		, bIsValid(true)
	{}
	
	UPROPERTY()
	FEntityAttribute Attribute;

	UPROPERTY()
	EAttributeModifierOp ModifierOp;

	UPROPERTY()
	float Magnitude;

	UPROPERTY()
	bool bIsValid;
};

/// @brief  Callback data to any requested listeners with information about a given modifier application
struct FEntityEffectModCallbackData
{
	FEntityEffectModCallbackData(const FEntityEffectSpec& InEffectSpec, FAttributeModifierEvaluatedData& InEvaluatedData, UAttributeSystemComponent& InTarget)
		: EffectSpec(InEffectSpec)
		, EvaluatedData(InEvaluatedData)
		, Target(InTarget)
	{
	}
	
	const FEntityEffectSpec&		EffectSpec;		// The spec that the mod came from
	FAttributeModifierEvaluatedData&	EvaluatedData;	// The 'flat'/computed data to be applied to the target

	UAttributeSystemComponent &Target;		// Target we intend to apply to
};