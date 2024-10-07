// Copyright 2023 CoC All rights reserved


#include "AttributeSystem/AttributeModifierTypes.h"
#include "Components/AttributeSystemComponent.h"
#include "Debug/AttributeLog.h"

/*--------------------------------------------------------------------------------------------------------------
* Modifier Magnitude
*--------------------------------------------------------------------------------------------------------------*/


float FAttributeBasedMagnitude::CalculateMagnitude(const FEntityEffectSpec& InRelevantSpec) const
{
	float AttrValue = 0.f;
	
	switch (AttributeCalculationType)
	{
		case EAttributeBasedCalculationType::AttributeMagnitude:
			AttrValue = AttributeCaptureValue;
			break;
		case EAttributeBasedCalculationType::AttributeBaseValue:
			AttrValue = AttributeBaseCaptureValue;
			break;
		case EAttributeBasedCalculationType::AttributeBonusMagnitude:
			AttrValue = AttributeCaptureValue - AttributeBaseCaptureValue;
			break;
	}
	
	return (Coefficient * (AttrValue + PreMultiplyAdditiveValue) + PostMultiplyAdditiveValue);
}


bool FEntityEffectModifierMagnitude::AttemptCalculateMagnitude(const FEntityEffectSpec& InRelevantSpec,
                                                               float& OutCalculatedMagnitude) const
{
	// Check if we can calculate
	OutCalculatedMagnitude = 0.f;
	if (!bCanCalculate) return false; // We cache this during initialization, mostly about if we have the correct attributes captured

	// Actually calculate based on calculation type
	if (MagnitudeCalculationPolicy == EAttributeModifierCalculationPolicy::FloatBased)
		OutCalculatedMagnitude = FloatBasedMagnitude;
	else
		OutCalculatedMagnitude = AttributeBasedMagnitude.CalculateMagnitude(InRelevantSpec);

	return true;
}

bool FEntityEffectModifierMagnitude::IsNonSnapshot() const
{
	if (MagnitudeCalculationPolicy == EAttributeModifierCalculationPolicy::FloatBased) return false;
		
	const auto CaptureInfo = AttributeBasedMagnitude.BackingAttribute;
	return !CaptureInfo.bSnapshot && CaptureInfo.AttributeToCapture.IsValid();
}

bool FEntityEffectModifierMagnitude::PerformAttributeCapture(const FEntityEffectSpec& InRelevantSpec, bool bSourceOnly)
{
	if (MagnitudeCalculationPolicy == EAttributeModifierCalculationPolicy::FloatBased) return false;
	
	bCanCalculate = false;
	
	const auto CaptureDef = AttributeBasedMagnitude.BackingAttribute;
	if (CaptureDef.CaptureSource == EAttributeCaptureSource::Target && bSourceOnly) return false;
	if (!CaptureDef.AttributeToCapture.IsValid()) return false;

	
	const auto ASCToCapture = CaptureDef.CaptureSource == EAttributeCaptureSource::Source ?
		InRelevantSpec.GetContext().GetInstigatorAttributeSystemComponent() : InRelevantSpec.GetContext().GetTargetAttributeSystemComponent();
	
	if (!ASCToCapture) return false;

	AttributeBasedMagnitude.AttributeCaptureValue = ASCToCapture->GetAttributeValue(CaptureDef.AttributeToCapture, bCanCalculate);
	AttributeBasedMagnitude.AttributeBaseCaptureValue = ASCToCapture->GetAttributeBaseValue(CaptureDef.AttributeToCapture);

	return true;
}

/*--------------------------------------------------------------------------------------------------------------
* Aggregators
*--------------------------------------------------------------------------------------------------------------*/

FAttributeModifierAggregator::FAttributeModifierAggregator()
{
	OpModMap.Add(EAttributeModifierOp::Add, TArray<FAggregatorElement>());
	OpModMap.Add(EAttributeModifierOp::Multiply, TArray<FAggregatorElement>());
	OpModMap.Add(EAttributeModifierOp::Divide, TArray<FAggregatorElement>());
	OpModMap.Add(EAttributeModifierOp::Override, TArray<FAggregatorElement>());
}

float FAttributeModifierAggregator::ExecModOnBaseValue(float BaseValue, EAttributeModifierOp ModifierOp,
                                                       float EvaluatedMagnitude)
{
	switch (ModifierOp)
	{
		case EAttributeModifierOp::Add:
			BaseValue += EvaluatedMagnitude;
			break;
		case EAttributeModifierOp::Multiply:
			BaseValue *= EvaluatedMagnitude;
			break;
		case EAttributeModifierOp::Divide:
			if (!FMath::IsNearlyZero(EvaluatedMagnitude))
			{
				BaseValue /= EvaluatedMagnitude;
			}
			break;
		case EAttributeModifierOp::Override:
			BaseValue = EvaluatedMagnitude;
			break;
	}

	return BaseValue;
}

float FAttributeModifierAggregator::EvaluateWithBase(float InlineBaseValue) const
{
	for (const FAggregatorElement& Mod : OpModMap[EAttributeModifierOp::Override])
	{
		return Mod.ActiveEffect->Spec.GetModifierMagnitude(Mod.ModIdx);
	}

	float Add = SumMods(OpModMap[EAttributeModifierOp::Add], ModBiases[uint8(EAttributeModifierOp::Add)]);
	float Multiply = SumMods(OpModMap[EAttributeModifierOp::Multiply], ModBiases[uint8(EAttributeModifierOp::Multiply)]);
	float Divide = SumMods(OpModMap[EAttributeModifierOp::Divide], ModBiases[uint8(EAttributeModifierOp::Divide)]);

	if (FMath::IsNearlyZero(Divide))
	{
		ATTRIBUTE_LOG(Warning, TEXT("Division summation was 0.f in aggregator evaluation"));
		Divide = 0.f;
	}

	return ((InlineBaseValue + Add) * Multiply) / Divide;
}

void FAttributeModifierAggregator::AddAggregatorMod(int32 ModIdx, FActiveEntityEffect& ActiveEffect)
{
	// TODO: Make this a getter so we don't directly access that list from here
	const auto ModifierOp = ActiveEffect.Spec.Modifiers[ModIdx].ModifierOp;

	FAggregatorElement AggrElement = FAggregatorElement(ModIdx, &ActiveEffect);

	OpModMap[ModifierOp].AddUnique(AggrElement);
}

void FAttributeModifierAggregator::RemoveAggregatorMod(const FActiveEntityEffect& ActiveEffect)
{
	for(auto& ModOp : OpModMap)
	{
		int32 RemoveIdx = ModOp.Value.IndexOfByPredicate([&ActiveEffect](const FAggregatorElement& Element)
		{
			return ActiveEffect == *Element.ActiveEffect;
		});

		if (RemoveIdx != INDEX_NONE)
		{
			ModOp.Value.RemoveAtSwap(RemoveIdx);
		}
	}
}

float FAttributeModifierAggregator::SumMods(const TArray<FAggregatorElement>& Mods, float Bias) const
{
	float Sum = Bias;

	for (const auto& Mod : Mods)
	{
		Sum += (Mod.ActiveEffect->Spec.GetModifierMagnitude(Mod.ModIdx) - Bias);
	}

	return Sum;
}
