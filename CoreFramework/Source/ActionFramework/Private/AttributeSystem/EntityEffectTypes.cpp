// Copyright 2023 CoC All rights reserved


#include "AttributeSystem/EntityEffectTypes.h"
#include "AttributeSystem/EntityEffect.h"
#include "Debug/AttributeLog.h"
#include "Helpers/ActionFrameworkStatics.h"

const float FEntityEffectConstants::INFINITE_DURATION = -1.f;
const float FEntityEffectConstants::INSTANT_APPLICATION = 0.f;
const float FEntityEffectConstants::NO_PERIOD = 0.f;
const float FEntityEffectConstants::UPDATE_FREQUENCY = 0.1f;

/*--------------------------------------------------------------------------------------------------------------
* Context
*--------------------------------------------------------------------------------------------------------------*/

void FEntityEffectContext::AddInstigator(AActor* InInstigator, AActor* InEffectCauser)
{
	Instigator = InInstigator;
	SetEffectCauser(InEffectCauser);
	InstigatorAttributeSystemComponent = UActionFrameworkStatics::GetAttributeSystemFromActor(InInstigator);
}

UAttributeSystemComponent* FEntityEffectContext::GetInstigatorAttributeSystemComponent() const
{
	return InstigatorAttributeSystemComponent.Get();
}

UAttributeSystemComponent* FEntityEffectContext::GetTargetAttributeSystemComponent() const
{
	return TargetAttributeSystemComponent.Get();
}

AActor* FEntityEffectContext::GetInstigatorActor() const
{
	return Instigator.Get();
}

AActor* FEntityEffectContext::GetEffectCauser() const
{
	return EffectCauser.Get();
}

void FEntityEffectContext::SetEffectCauser(AActor* InEffectCauser)
{
	EffectCauser = InEffectCauser;
}

void FEntityEffectContext::SetTarget(UAttributeSystemComponent* InTargetASC)
{
	TargetAttributeSystemComponent = InTargetASC; 
}

FString FEntityEffectContext::ToString() const
{
	const AActor* InstigatorPtr = Instigator.Get();
	return (InstigatorPtr ? InstigatorPtr->GetName() : FString(TEXT("NONE")));
}

/*--------------------------------------------------------------------------------------------------------------
* Entity Effect Spec
*--------------------------------------------------------------------------------------------------------------*/

FEntityEffectSpec::FEntityEffectSpec(const FEntityEffectSpec& Other)
{
	*this = Other;
}

void FEntityEffectSpec::Initialize(const UEntityEffect* InDef, const FEntityEffectContext& InEffectContext)
{
	Def = InDef;
	check(Def);

	Modifiers = Def->Modifiers; // Do this first as we need to push captures into our copy of modifiers
	SetContext(InEffectContext);

	// TODO: Double check on this
	Period = Def->Period;
	AttemptCalculateDurationFromDef(Duration);
	
	// Check if we have any non-snapshot shit
	bHasNonSnapshotAttributes = false;
	NonSnapshotModsIdx.Empty();
	for (int32 ModIdx = 0; ModIdx < Modifiers.Num(); ModIdx++)
	{
		const auto& Mod = Modifiers[ModIdx];
		if (Mod.ModifierMagnitude.IsNonSnapshot())
		{
			bHasNonSnapshotAttributes = true;
			NonSnapshotModsIdx.Add(ModIdx);
		}
	}
}

void FEntityEffectSpec::InitializeFromSpec(const UEntityEffect* InDef, const FEntityEffectSpec& OriginalSpec)
{
	Initialize(InDef, OriginalSpec.GetContext());
}

void FEntityEffectSpec::CaptureAttributes(bool bInitialCapture)
{
	// Source snapshot is taken during construction
	if (bInitialCapture)
	{
		InvalidModCapturesIdx.Empty();
		for (auto& Mod : Modifiers)
		{
			// False if float based mod, or 
			InvalidModCapturesIdx.Add(Mod.ModifierMagnitude.PerformAttributeCapture(*this));
		}
	}
	else
	{
		for (const auto ModIdx : NonSnapshotModsIdx)
		{
			if (!InvalidModCapturesIdx[ModIdx]) continue;
			
			auto& Mod = Modifiers[ModIdx];
			Mod.ModifierMagnitude.PerformAttributeCapture(*this);
			Mod.ModifierMagnitude.AttemptCalculateMagnitude(*this, Modifiers[ModIdx].EvaluatedMagnitude);
		}
	}
}

void FEntityEffectSpec::RecaptureSourceSnapshots()
{
	for (auto& Mod : Modifiers)
	{
		if (!Mod.ModifierMagnitude.IsNonSnapshot()) continue;

		// This is called when target is not yet set, so TargetASC is null and we'll only actually capture source snapshots
		if (Mod.ModifierMagnitude.PerformAttributeCapture(*this, true))
			Mod.ModifierMagnitude.AttemptCalculateMagnitude(*this, Mod.EvaluatedMagnitude);
	}
}

bool FEntityEffectSpec::AttemptCalculateDurationFromDef(float& OutDefDuration) const
{
	check(Def);

	bool bCalculatedDuration = true;

	if (Def->DurationPolicy == EEntityEffectDurationType::Infinite)
	{
		OutDefDuration = FEntityEffectConstants::INFINITE_DURATION;
	}
	else if (Def->DurationPolicy == EEntityEffectDurationType::Instant)
	{
		OutDefDuration = FEntityEffectConstants::INSTANT_APPLICATION;
	}
	else
	{
		// Inplace capture
		auto DurationMod = Def->DurationMagnitude;
		DurationMod.PerformAttributeCapture(*this);
		bCalculatedDuration = DurationMod.AttemptCalculateMagnitude(*this, OutDefDuration);
	}
	
	return bCalculatedDuration;
}

void FEntityEffectSpec::SetDuration(float NewDuration)
{
	Duration = NewDuration;
}

float FEntityEffectSpec::GetDuration() const
{
	return Duration;
}

float FEntityEffectSpec::GetPeriod() const
{
	return Period;
}

void FEntityEffectSpec::SetContext(FEntityEffectContext NewEffectContext)
{
	EffectContext = NewEffectContext;
	RecaptureSourceSnapshots(); // We recapture source snapshots whenever we change contexts
}

void FEntityEffectSpec::InitializeTargetAndCaptures(const UAttributeSystemComponent* InTargetASC)
{
	EffectContext.SetTarget(const_cast<UAttributeSystemComponent*>(InTargetASC));
	CaptureAttributes(true);
}

void FEntityEffectSpec::GetEffectTags(FGameplayTagContainer& OutContainer) const
{
	if (Def)
		OutContainer.AppendTags(Def->GetEffectTags());
}

void FEntityEffectSpec::CalculateModifierMagnitudes()
{
	for (int32 ModIdx = 0; ModIdx < Modifiers.Num(); ++ModIdx)
	{
		CalculateModifierMagnitude(ModIdx);
	}
}

void FEntityEffectSpec::CalculateModifierMagnitude(int32 ModIdx)
{
	FAttributeModifierInfo& ModDef = Modifiers[ModIdx];

	if (!ModDef.ModifierMagnitude.AttemptCalculateMagnitude(*this, ModDef.EvaluatedMagnitude))
	{
		ATTRIBUTE_LOG(Warning, TEXT("Modifier On Spec: %s was asked to CalculateMagnitude and failed. Falling back to 0"), *GetNameSafe(Def));
	}
}

float FEntityEffectSpec::GetModifierMagnitude(int32 ModIdx) const
{
	if (Modifiers.IsValidIndex(ModIdx))
	{
		return Modifiers[ModIdx].GetEvaluatedMagnitude();
	}
	return 0.f;
}

void FEntityEffectSpec::PrintAll() const
{
	ATTRIBUTE_LOG(Log, TEXT("Def: %s"), *Def->GetName());
	ATTRIBUTE_LOG(Log, TEXT("Duration: %.2f"), GetDuration());
	ATTRIBUTE_LOG(Log, TEXT("Period: %.2f"), GetPeriod());
	ATTRIBUTE_LOG(Log, TEXT("Modifiers:"));
}

/*--------------------------------------------------------------------------------------------------------------
* Active Effect
*--------------------------------------------------------------------------------------------------------------*/


void FActiveEntityEffect::PrintAll() const
{
	ATTRIBUTE_LOG(Log, TEXT("Handle: %s"), *Handle.ToString())
	ATTRIBUTE_LOG(Log, TEXT("Start World Time: %.2f"), StartWorldTime);
	Spec.PrintAll();
}

FString FActiveEntityEffect::GetDebugString() const
{
	return FString::Printf(TEXT("Def: %s"), *GetNameSafe(Spec.Def));
}

/*--------------------------------------------------------------------------------------------------------------
* Query
*--------------------------------------------------------------------------------------------------------------*/

bool FEntityEffectQuery::Matches(const FActiveEntityEffect& Effect) const
{
	// Anything in the ignore handle list results in an immediate non-match
	if (IgnoreHandles.Contains(Effect.Handle))
	{
		return false;
	}
	
	// since all of these query conditions must be met to be considered a match, failing
	// any one of them means we can return false
	return Matches(Effect.Spec);
}

bool FEntityEffectQuery::Matches(const FEntityEffectSpec& Spec) const
{
	if (Spec.Def == nullptr)
	{
		return false;
	}

	if (EffectTagQuery.IsEmpty() == false)
	{
		// Combine tags from the definition and the spec into one container to match queries that may span both
		// static to avoid memory allocations every time we do a query
		check(IsInGameThread());
		static FGameplayTagContainer GETags;
		GETags.Reset();

		Spec.GetEffectTags(GETags);

		if (EffectTagQuery.Matches(GETags) == false)
		{
			return false;
		}
	}


	// if we are looking for ModifyingAttribute go over each of the Spec Modifiers and check the Attributes
	if (ModifyingAttribute.IsValid())
	{
		bool bEffectModifiesThisAttribute = false;

		for (int32 ModIdx = 0; ModIdx < Spec.Modifiers.Num(); ++ModIdx)
		{
			const FAttributeModifierInfo& ModDef = Spec.Modifiers[ModIdx];

			if (ModDef.Attribute == ModifyingAttribute)
			{
				bEffectModifiesThisAttribute = true;
				break;
			}
		}
		if (bEffectModifiesThisAttribute == false)
		{
			// effect doesn't modify the attribute we are looking for, no match
			return false;
		}
	}

	// check definition
	if (EffectDefinition != nullptr)
	{
		if (Spec.Def != EffectDefinition.GetDefaultObject())
		{
			return false;
		}
	}

	// Check instigator
	if (Instigator != nullptr)
	{
		if (Spec.GetContext().GetInstigatorActor() != Instigator)
		{
			return false;
		}
	}

	return true;
}

bool FEntityEffectQuery::IsEmpty() const
{
	return 
	(
		EffectTagQuery.IsEmpty() &&
		!ModifyingAttribute.IsValid() &&
		!Instigator &&
		!EffectSource &&
		!EffectDefinition
	);
}