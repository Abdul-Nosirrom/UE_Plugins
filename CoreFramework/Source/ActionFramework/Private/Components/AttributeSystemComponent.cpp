// Copyright 2023 CoC All rights reserved


#include "Components/AttributeSystemComponent.h"

#include "DrawDebugHelpers.h"
#include "TimerManager.h"
#include "Components/ActionSystemComponent.h"
#include "AttributeSystem/EntityEffectTypes.h"
#include "Debug/AttributeLog.h"

/*--------------------------------------------------------------------------------------------------------------
* CVARS
*--------------------------------------------------------------------------------------------------------------*/

namespace AttributeCVars
{
#if ALLOW_CONSOLE && !NO_LOGGING
	int32 VisualizeAttributes = 0;
	FAutoConsoleVariableRef CVarShowAttributeStates
	(
		TEXT("attributes.VisualizeAttributes"),
		VisualizeAttributes,
		TEXT("Visualize Attributes. 0: Disable, 1: Enable"),
		ECVF_Default
	);
#endif 
}

/*--------------------------------------------------------------------------------------------------------------
* UActorComponent Interface 
*--------------------------------------------------------------------------------------------------------------*/

// Sets default values for this component's properties
UAttributeSystemComponent::UAttributeSystemComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;
}

void UAttributeSystemComponent::BeginPlay()
{
	Super::BeginPlay();

	if (InitStats)
	{
		// Add attribute sets it needs
		for (const auto& SetClass : InitStats->GetRequiredAttributeSets())
		{
			if (auto Set = const_cast<UEntityAttributeSet*>(GetOrCreateAttributeSet(SetClass)))
			{
				Set->InitAttributes(InitStats);
			}
		}
	}
}

void UAttributeSystemComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                              FActorComponentTickFunction* ThisTickFunction)
{
	// TODO: Update all nonsnapshot captures on duration based non-periodic effects
	// Update captures and evaluated magnitudes
	for (auto& ActiveEffect : ActiveEntityEffects)
	{
		if (!ActiveEffect.Spec.HasNonSnapshotMods()) continue;
		
		ActiveEffect.Spec.CaptureAttributes(false);
		ActiveEffect.Spec.CalculateModifierMagnitudes();
	}

	// Recalculate aggregators
	for (auto& Aggr : AttributeAggregatorMap)
	{
		if (Aggr.Value.Get()->IsValid())
			InternalUpdateAttributeValue(Aggr.Key, Aggr.Value->EvaluateWithBase(GetAttributeBaseValue(Aggr.Key)));
	}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (AttributeCVars::VisualizeAttributes > 0)
	{
		VisualizeAttributes();
	}
#endif 
}

/*--------------------------------------------------------------------------------------------------------------
* Attribute Set
*--------------------------------------------------------------------------------------------------------------*/

bool UAttributeSystemComponent::HasAttributeSetForAttribute(FEntityAttribute Attribute) const
{
	return (Attribute.IsValid() && GetAttributeSet(Attribute.GetAttributeSetClass()));
}

void UAttributeSystemComponent::GetAllAttributes(TArray<FEntityAttribute>& OutAttributes) const
{
	for (const UEntityAttributeSet* Set : GetSpawnedAttributeSets())
	{
		if (!Set) continue;

		for (TFieldIterator<FProperty> It(Set->GetClass()); It; ++It)
		{
			if (FEntityAttribute::IsEntityAttributeDataProperty(*It))
			{
				OutAttributes.Push(FEntityAttribute(*It));
			}
		}
	}
}

const UEntityAttributeSet* UAttributeSystemComponent::GetAttributeSet(TSubclassOf<UEntityAttributeSet> AttributeSetClass) const
{
	for (const UEntityAttributeSet* Set : GetSpawnedAttributeSets())
	{
		if (Set && Set->IsA(AttributeSetClass))
		{
			return Set;
		}
	}
	return nullptr;
}

float UAttributeSystemComponent::GetAttributeValue(const FEntityAttribute& Attribute, bool& bFound) const
{
	bFound = false;
	float CurrentValue = 0.f;

	// Null or invalid attribute
	if (!Attribute.IsValid()) return CurrentValue;

	const UEntityAttributeSet* AttributeSet = GetAttributeSet(Attribute.GetAttributeSetClass());
	if (!ensureMsgf(AttributeSet, TEXT("UAttributeSystemComponent::%s: Unable to get attribute set for attribute %s on %s"), __func__, *Attribute.GetName(), *GetPathName()))
	{
		return CurrentValue;
	}

	const FStructProperty* StructProperty = CastField<FStructProperty>(Attribute.GetUProperty());
	check(StructProperty);
	const FEntityAttributeData* DataPtr = StructProperty->ContainerPtrToValuePtr<FEntityAttributeData>(AttributeSet);
	if (DataPtr)
	{
		bFound = true;
		CurrentValue = DataPtr->GetCurrentValue();
	}
	else
	{
		ATTRIBUTE_LOG(Warning, TEXT("Failed to retrieve attribute data"));
	}

	return CurrentValue;
}

float UAttributeSystemComponent::GetAttributeBaseValue(const FEntityAttribute& Attribute) const
{
	float BaseValue = 0.f;

	const UEntityAttributeSet* AttributeSet = GetAttributeSet(Attribute.GetAttributeSetClass());
	if (!ensureMsgf(AttributeSet, TEXT("UAttributeSystemComponent::%s: Unable to get attribute set for attribute %s on %s"), __func__, *Attribute.GetName(), *GetPathName()))
	{
		return BaseValue;
	}

	const FStructProperty* StructProperty = CastField<FStructProperty>(Attribute.GetUProperty());
	check(StructProperty);
	const FEntityAttributeData* DataPtr = StructProperty->ContainerPtrToValuePtr<FEntityAttributeData>(AttributeSet);
	if (DataPtr)
	{
		BaseValue = DataPtr->GetBaseValue();
	}
	else
	{
		ATTRIBUTE_LOG(Warning, TEXT("Failed to retrieve attribute data"));
	}

	return BaseValue;
}

void UAttributeSystemComponent::SetAttributeBaseValue(const FEntityAttribute& Attribute, float NewBaseValue)
{
	const UEntityAttributeSet* AttributeSet = GetAttributeSet(Attribute.GetAttributeSetClass());
	if (!ensureMsgf(AttributeSet, TEXT("UAttributeSystemComponent::SetAttributeBaseValue: Unable to get attribute set for attribute %s"), *Attribute.GetName()))
	{
		return;
	}

	float OldBaseValue = 0.f;

	AttributeSet->PreAttributeBaseChange(Attribute, NewBaseValue);

	// Can't do it directly through the attribute cuz its const, so have to do it manually
	const FStructProperty* StructProperty = CastField<FStructProperty>(Attribute.GetUProperty());
	check(StructProperty);
	FEntityAttributeData* DataPtr = StructProperty->ContainerPtrToValuePtr<FEntityAttributeData>(const_cast<UEntityAttributeSet*>(AttributeSet));
	if (DataPtr)
	{
		OldBaseValue = DataPtr->GetBaseValue();
		DataPtr->SetBaseValue(NewBaseValue);
	}
	else
	{
		ATTRIBUTE_LOG(Warning, TEXT("Failed to retrieve attribute data"));
	}

	// TODO: Notify aggregators about a change in the base value so it can update the current value
	FAttributeModifierAggregator* Aggr = FindAttributeAggregator(Attribute);
	
	if (Aggr)
	{
		InternalUpdateAttributeValue(Attribute, Aggr->EvaluateWithBase(NewBaseValue));
	}
	else
	{
		// No aggregator modifying the CurrentValue of this attribute. Set Current = Base now that Base has changed
		InternalUpdateAttributeValue(Attribute, NewBaseValue);
	}

	AttributeSet->PostAttributeBaseChange(Attribute, OldBaseValue, NewBaseValue);
}

/*--------------------------------------------------------------------------------------------------------------
* Entity Effects
*--------------------------------------------------------------------------------------------------------------*/

FActiveEntityEffectHandle UAttributeSystemComponent::ApplyEntityEffectToTarget(
	TSubclassOf<UEntityEffect> EntityEffect, UAttributeSystemComponent* Target, FEntityEffectContext Context)
{
	if (EntityEffect)
	{
		if (!Context.IsValid())
		{
			Context = MakeEffectContext();
		}

		const FEntityEffectSpec Spec(EntityEffect->GetDefaultObject<UEntityEffect>(), Context);
		return ApplyEntityEffectSpecToTarget(Spec, Target);
	}
	return FActiveEntityEffectHandle();
}

FActiveEntityEffectHandle UAttributeSystemComponent::ApplyEntityEffectToSelf(
	TSubclassOf<UEntityEffect> EntityEffect, FEntityEffectContext Context)
{
	if (EntityEffect)
	{
		if (!Context.IsValid())
		{
			Context = MakeEffectContext();
		}

		const FEntityEffectSpec Spec(EntityEffect->GetDefaultObject<UEntityEffect>(), Context);
		return ApplyEntityEffectSpecToSelf(Spec);
	}
	return FActiveEntityEffectHandle();
}

FActiveEntityEffectHandle UAttributeSystemComponent::ApplyEntityEffectSpecToTarget(const FEntityEffectSpec& EntityEffect,
                                                                              UAttributeSystemComponent* Target)
{
	FActiveEntityEffectHandle ReturnHandle;
	
	if (Target)
	{
		ReturnHandle = Target->ApplyEntityEffectSpecToSelf(EntityEffect);	
	}

	return ReturnHandle;
}

FActiveEntityEffectHandle UAttributeSystemComponent::ApplyEntityEffectSpecToSelf(const FEntityEffectSpec& EntityEffect)
{
	FActiveEntityEffectHandle ReturnHandle(INDEX_NONE);
	
	// Check if there's a registered query that can block the application of this spec
	for (const auto& ApplicationQuery : EntityEffectApplicationQueries)
	{
		const bool bAllowed = ApplicationQuery.Execute(this, EntityEffect);
		if (!bAllowed)
		{
			return ReturnHandle;
		}
	}

	// Ensure that all attributes for this effect are valid
	for (const auto& Mod : EntityEffect.Modifiers)
	{
		if (!Mod.Attribute.IsValid())
		{
			ATTRIBUTE_LOG(Error, TEXT("%s Has a null modifier attribute"), *EntityEffect.Def->GetPathName());
			return ReturnHandle;
		}
	}

	// Check Duration, whether to execute or apply
	TUniquePtr<FEntityEffectSpec> MutableUniqueForScope = nullptr;
	FEntityEffectSpec* MutableSpec = nullptr;
	// BUG: Copying specs is causing issues, not properly copying the modifiers and all that
	// TODO: Capture Everything except source snapshot (Internal does that already, but we need to do it before ExecuteEntityEffect and not in ExecuteEntityEffect so we handle periodic execution in ExecutePeriodic)
	if (EntityEffect.Def->DurationPolicy != EEntityEffectDurationType::Instant)
	{
		FActiveEntityEffect* AppliedEffect = InternalApplyEntityEffectSpec(EntityEffect);
		ReturnHandle = AppliedEffect->Handle;
		MutableSpec = &(AppliedEffect->Spec);
		
		// Log results of applied spec
		ATTRIBUTE_VLOG(GetOwnerActor(), Log, TEXT("Applied %s"), *EntityEffect.Def.GetFName().ToString());

	}
	else
	{
		MutableUniqueForScope = MakeUnique<FEntityEffectSpec>(EntityEffect);
		MutableSpec = MutableUniqueForScope.Get();
		MutableSpec->InitializeTargetAndCaptures(this);
		ExecuteEntityEffect(*MutableSpec);
	}
	
	UAttributeSystemComponent* InstigatorASC = EntityEffect.GetContext().GetInstigatorAttributeSystemComponent();

	// TODO: Namings kinda confusing if sticking to applied vs execute. If applied, we should probably pass the active effect so something can be done about it/modify it with the active struct
	
	// Send ourselves a callback
	OnEntityEffectAppliedToSelf(InstigatorASC, *MutableSpec);
	
	// Send the instigator a callback
	if (InstigatorASC)
	{
		InstigatorASC->OnEntityEffectAppliedToTarget(this, *MutableSpec);
	}
	
	return ReturnHandle;
}

bool UAttributeSystemComponent::RemoveActiveEntityEffect(FActiveEntityEffectHandle EffectHandle)
{
	if (!EffectHandle.IsValid())
	{
		ATTRIBUTE_LOG(Log, TEXT("RemoveActiveEntityEffect called with invalid ActiveEffect: %s"), *EffectHandle.ToString());
		return false;
	}
	
	return InternalRemoveActiveEntityEffect(EffectHandle, true);
}

int32 UAttributeSystemComponent::RemoveActiveEntityEffectsBySourceEffect(TSubclassOf<UEntityEffect> EntityEffect)
{
	FEntityEffectQuery Query;
	Query.EffectDefinition = EntityEffect;
	return RemoveActiveEntityEffects(Query);
}

int32 UAttributeSystemComponent::RemoveActiveEntityEffectsByInstigator(AActor* Instigator)
{
	FEntityEffectQuery Query;
	Query.Instigator = Instigator;
	return RemoveActiveEntityEffects(Query);
}

int32 UAttributeSystemComponent::RemoveActiveEntityEffectsByEffectTags(const FGameplayTagContainer& EffectTags)
{
	return RemoveActiveEntityEffects(FEntityEffectQuery::MakeQuery_MatchAnyEffectTags(EffectTags));
}

int32 UAttributeSystemComponent::RemoveActiveEntityEffects(const FEntityEffectQuery& Query)
{
	int32 NumRemoved = 0;
	
	for (int32 idx = GetNumActiveEntityEffects() - 1; idx >= 0; idx--)
	{
		const FActiveEntityEffect& Effect = ActiveEntityEffects[idx];
		if (Query.Matches(Effect))
		{
			InternalRemoveActiveEntityEffect(Effect.Handle, true);
			++NumRemoved;
		}
	}
	
	return NumRemoved;
}

void UAttributeSystemComponent::ExecModOnAttribute(const FEntityAttribute& Attribute, EAttributeModifierOp ModifierOp, float ModifierMagnitude, const FEntityEffectModCallbackData* ModData)
{
	CurrentModCallbackData = ModData;
	
	float CurrentBase = GetAttributeBaseValue(Attribute);
	float NewBase = FAttributeModifierAggregator::ExecModOnBaseValue(CurrentBase, ModifierOp, ModifierMagnitude);

	SetAttributeBaseValue(Attribute, NewBase);

	if (CurrentModCallbackData)
	{
		CurrentModCallbackData = nullptr;
	}
}

bool UAttributeSystemComponent::CanApplyAttributeModifiers(TSubclassOf<UEntityEffect> EntityEffect,
	const FEntityEffectContext& EffectContext) const
{
	return CanApplyAttributeModifiers(EntityEffect->GetDefaultObject<UEntityEffect>(), EffectContext);
}

bool UAttributeSystemComponent::CanApplyAttributeModifiers(const UEntityEffect* EntityEffect,
                                                           const FEntityEffectContext& EffectContext) const
{
	FEntityEffectSpec Spec(EntityEffect, EffectContext);

	// Setup captures before evaluating
	Spec.InitializeTargetAndCaptures(this);
	Spec.CalculateModifierMagnitudes();

	for (int32 ModIdx = 0; ModIdx < Spec.Modifiers.Num(); ++ModIdx)
	{
		const FAttributeModifierInfo& ModDef = Spec.Modifiers[ModIdx];

		// Only need to check additive operators as thats the only thing that'll get the value < 0
		if (ModDef.ModifierOp == EAttributeModifierOp::Add)
		{
			if (!ModDef.Attribute.IsValid()) continue;

			const UEntityAttributeSet* Set = GetAttributeSet(ModDef.Attribute.GetAttributeSetClass());
			float CurrentValue = ModDef.Attribute.GetAttributeValue(Set);
			float CostValue = ModDef.GetEvaluatedMagnitude();

			if (CurrentValue + CostValue < 0.f)
			{
				return false;
			}
		}
	}

	return true;
}

FEntityEffectSpec UAttributeSystemComponent::MakeOutgoingSpec(TSubclassOf<UEntityEffect> EntityEffectClass,
	FEntityEffectContext Context) const
{
	if (!Context.IsValid())
	{
		Context = MakeEffectContext();
	}

	if (EntityEffectClass)
	{
		UEntityEffect* EntityEffect = EntityEffectClass->GetDefaultObject<UEntityEffect>();
		FEntityEffectSpec NewSpec(EntityEffect, Context);
		return NewSpec;
	}

	ATTRIBUTE_LOG(Warning, TEXT("Attempted to create spec with Null effect class definition"));

	return FEntityEffectSpec(nullptr, Context);
}

FEntityEffectContext UAttributeSystemComponent::MakeEffectContext() const
{
	// By default, use the owner and avatar as the instigator and causer
	FEntityEffectContext Context(GetOwnerActor(), GetOwnerActor());
	return Context;
}


void UAttributeSystemComponent::ExecutePeriodicEffect(FActiveEntityEffectHandle EffectHandle)
{
	FActiveEntityEffect* Effect = GetActiveEntityEffect(EffectHandle);

	if (!Effect) return;

	
	ATTRIBUTE_VLOG(OwnerActor, Log, TEXT("Executed Periodic Effect %s"), *Effect->Spec.Def->GetFName().ToString());


	// Execute as usual as thats what periodic effects do
	ExecuteEntityEffect(Effect->Spec);

	// Invoke delegates for periodic effects being executed, letting both the target (we're attached to) and instigator (whoever applied it) know
	UAttributeSystemComponent* SourceASC = Effect->Spec.GetContext().GetInstigatorAttributeSystemComponent();
	OnPeriodicEntityEffectExecuteOnSelf(SourceASC, Effect->Spec);
	if (SourceASC) // Should we check of Source == Owner ? Otherwise this event might be invoked twice if we apply to self unless applying to self skips storing an instigator
	{
		SourceASC->OnPeriodicEntityEffectExecuteOnTarget(this, Effect->Spec);
	}
}

/*--------------------------------------------------------------------------------------------------------------
* Entity Effects (Internal)
*--------------------------------------------------------------------------------------------------------------*/

void UAttributeSystemComponent::ExecuteEntityEffect(FEntityEffectSpec& EntityEffect)
{
	// Should only ever execute effects that are instant application or periodic. Effects with no period or arent instant should never go here
	FString TrueStr = "True";
	FString FalseStr = "False";
	ATTRIBUTE_LOG(Error, TEXT("Duration = %.2f and Period = %.2f"), EntityEffect.GetDuration(), EntityEffect.GetPeriod());
	ATTRIBUTE_LOG(Error, TEXT("INSTANT = %.2f and NOPERIOD = %.2f"), FEntityEffectConstants::INSTANT_APPLICATION, FEntityEffectConstants::NO_PERIOD);
	ATTRIBUTE_LOG(Error, TEXT("Duration == INSTANT? %s"), (EntityEffect.GetDuration() == FEntityEffectConstants::INSTANT_APPLICATION ? *TrueStr : *FalseStr));
	ATTRIBUTE_LOG(Error, TEXT("Period != NOPERIOD? %s"), (EntityEffect.GetPeriod() != FEntityEffectConstants::NO_PERIOD ? *TrueStr : *FalseStr));

	check((EntityEffect.GetDuration() == FEntityEffectConstants::INSTANT_APPLICATION || EntityEffect.GetPeriod() != FEntityEffectConstants::NO_PERIOD));
	//BUG: Duration == INSTANT is failing, -0.00 == 0.00
	
	ATTRIBUTE_VLOG(GetOwnerActor(), Log, TEXT("Executed %s"), *EntityEffect.Def->GetName());

	// Let modifiers of this effect evaluate first, then we'll apply it
	EntityEffect.CaptureAttributes(false);
	EntityEffect.CalculateModifierMagnitudes();
	// TODO: Should probably not capture here as Periodic is gonna go through here? Capture on creation then capture in ExecutePeriodic for non-snapshots

	/* Modifiers: These will modify the base value of attributes */
	
	for (int32 ModIdx = 0; ModIdx < EntityEffect.Modifiers.Num(); ++ModIdx)
	{
		const FAttributeModifierInfo ModDef = EntityEffect.Modifiers[ModIdx];

		FAttributeModifierEvaluatedData EvalData(ModDef.Attribute, ModDef.ModifierOp, EntityEffect.GetModifierMagnitude(ModIdx));
		InternalExecuteMod(EntityEffect, EvalData);
	}

	// Notify the gameplay effect that its been executed
	EntityEffect.Def->OnExecuted(this, EntityEffect);
}

FActiveEntityEffect* UAttributeSystemComponent::InternalApplyEntityEffectSpec(const FEntityEffectSpec& Spec)
{
	// Check if def exists on spec
	if (!ensureMsgf(Spec.Def, TEXT("Tried to apply EE with no def (context == %s)"), *Spec.GetContext().ToString()))
	{
		return nullptr;
	}

	// Check if we can apply the effect to begin with, behaviors can say 'no' 
	if (!Spec.Def->CanApply(this, Spec)) return nullptr;

	// Create the active EE and generate a new handle for it, effect created directly on our list of active effects
	FActiveEntityEffect* AppliedActiveEE = new(ActiveEntityEffects) FActiveEntityEffect(FActiveEntityEffectHandle::GenerateHandle(), Spec, GetWorld()->GetTimeSeconds());

	// We calculate the modifier magnitudes then apply them later if conditions pass. Stores the evaluation results of the modifiers in the def into a "ModSpec" on the spec thats used later.
	AppliedActiveEE->Spec.InitializeTargetAndCaptures(this);
	AppliedActiveEE->Spec.CalculateModifierMagnitudes();

	// TODO: Is the duration being properly calculated?
	// Calculate the duration of the spec, it may depend on attributes
	float DefCalcDuration = 0.f;
	if (AppliedActiveEE->Spec.AttemptCalculateDurationFromDef(DefCalcDuration))
	{
		AppliedActiveEE->Spec.SetDuration(DefCalcDuration);
	}
	
	const float DurationBaseValue = AppliedActiveEE->Spec.GetDuration();
	// If we have a duration, setup timer for expiring the effect
	if (DurationBaseValue > 0.f)
	{
		FTimerManager& TimerManager = GetWorld()->GetTimerManager();
		FTimerDelegate Delegate = FTimerDelegate::CreateUObject(this, &UAttributeSystemComponent::CheckDurationExpired, AppliedActiveEE->Handle);
		TimerManager.SetTimer(AppliedActiveEE->DurationHandle, Delegate, DurationBaseValue, false);
		if (!ensureMsgf(AppliedActiveEE->DurationHandle.IsValid(), TEXT("Invalid Duration Handle after attempting to set duration for EE %s @ %.2f"),
			*AppliedActiveEE->GetDebugString(), DurationBaseValue))
		{
			TimerManager.SetTimerForNextTick(Delegate);
		} 
	}

	// Check if we should apply periodic execution
	if (AppliedActiveEE->Spec.GetPeriod() > FEntityEffectConstants::NO_PERIOD)
	{
		FTimerManager& TimerManager = GetWorld()->GetTimerManager();
		FTimerDelegate Delegate = FTimerDelegate::CreateUObject(this, &UAttributeSystemComponent::ExecutePeriodicEffect, AppliedActiveEE->Handle);

		if (AppliedActiveEE->Spec.Def->bExecutePeriodicEffectOnApplication)
		{
			TimerManager.SetTimerForNextTick(Delegate);
		}

		TimerManager.SetTimer(AppliedActiveEE->PeriodHandle, Delegate, AppliedActiveEE->Spec.GetPeriod(), true);
	}

	// Add the effects modifiers to the appropriate aggregators
	{
		const UEntityEffect* EffectDef = AppliedActiveEE->Spec.Def;

		if (EffectDef)
		{
			ATTRIBUTE_VLOG(OwnerActor ? OwnerActor : GetOuter(), Log, TEXT("Added: %s"), *GetNameSafe(EffectDef->GetClass()));

			AddActiveEffectModifiers(*AppliedActiveEE);
		}
		else
		{
			ATTRIBUTE_LOG(Error, TEXT("FActiveEntityEffectsContainer added new effect with NULL def!"));
		}
	}

	return AppliedActiveEE;
}

bool UAttributeSystemComponent::InternalExecuteMod(FEntityEffectSpec& Spec,
	FAttributeModifierEvaluatedData& ModEvalData)
{
	bool bExecuted = false;

	UEntityAttributeSet* AttributeSet = const_cast<UEntityAttributeSet*>(GetAttributeSet(ModEvalData.Attribute.GetAttributeSetClass()));

	// Owner doesn't have the attribute that is being modified
	if (!AttributeSet)
	{
		ATTRIBUTE_LOG(Log, TEXT("%s Does not have attribute %s. Skipping Modifier"), *GetPathName(), *ModEvalData.Attribute.GetName());
		return false;
	}

	// Has attribute, continue attempting to execute
	FEntityEffectModCallbackData ExecuteData(Spec, ModEvalData, *this);

	// PreExecute can return false to 'throw out' this modification
	if (AttributeSet->PreEntityEffectExecute(ExecuteData))
	{
		ExecModOnAttribute(ModEvalData.Attribute, ModEvalData.ModifierOp, ModEvalData.Magnitude, &ExecuteData);

		// This should apply gamewide rules, such as clamping Health to MaxHealth or modifying health after DamageTaken execution (general formula), etc...
		AttributeSet->PostEntityEffectExecute(ExecuteData);

		bExecuted = true;
	}

	return bExecuted;
}

bool UAttributeSystemComponent::InternalRemoveActiveEntityEffect(FActiveEntityEffectHandle EffectHandle, bool bPrematureRemoval)
{
	const int32 EffectIdx = ActiveEntityEffects.IndexOfByPredicate([EffectHandle](const FActiveEntityEffect& Element)
	{
		return Element.Handle == EffectHandle;
	});

	if (EffectIdx == INDEX_NONE)
	{
		ATTRIBUTE_LOG(Warning, TEXT("[Handle %s] Tried removing an effect that did not exist on the active list"), *EffectHandle.ToString());
		return false;
	}

	FActiveEntityEffect& EffectToRemove = ActiveEntityEffects[EffectIdx];

	ATTRIBUTE_VLOG(GetOwnerActor(), Log, TEXT("Removed: %s"), *GetNameSafe(EffectToRemove.Spec.Def->GetClass()));
	
	// Setup description struct for removal event broadcast
	FEntityEffectRemovalInfo EffectRemovalInfo;
	EffectRemovalInfo.EffectRemoved = EffectToRemove.Spec.Def;
	EffectRemovalInfo.bPrematureRemoval = bPrematureRemoval;
	EffectRemovalInfo.EffectContext = EffectToRemove.Spec.GetContext();

	// Check timers of this effect and clear them
	if (EffectToRemove.DurationHandle.IsValid())
	{
		GetWorld()->GetTimerManager().ClearTimer(EffectToRemove.DurationHandle);
	}
	if (EffectToRemove.PeriodHandle.IsValid())
	{
		GetWorld()->GetTimerManager().ClearTimer(EffectToRemove.PeriodHandle);
	}

	// Remove its modifiers which will notify the effect
	RemoveActiveEffectModifiers(EffectToRemove);

	// Broadcast removal events
	EffectToRemove.EventSet.OnEffectRemoved.Broadcast(EffectRemovalInfo); // For this specific effect (behaviors can bind to this, but we also call a function on them)
	OnActiveEntityEffectRemovedDelegate.Broadcast(EffectToRemove); // For general listeners uwu
	// Notify behaviors, these could do stuff like remove granted tags, unblocking actions, cancelling actions, etc...
	EffectToRemove.Spec.Def->OnRemoved(this, EffectToRemove, bPrematureRemoval);

	// Remove the effect from the internal array
	ActiveEntityEffects.RemoveAtSwap(EffectIdx);
	

	return true;
}

void UAttributeSystemComponent::InternalUpdateAttributeValue(FEntityAttribute Attribute, float NewValue)
{
	bool bDummy = false;
	float OldValue = GetAttributeValue(Attribute, bDummy);
	
	if (OldValue != NewValue) // NOTE: We do this because we're updating on tick so we always go through here for duration effects, but lets hold off until we're sure a value changed
	{
		Attribute.SetAttributeValue(NewValue, const_cast<UEntityAttributeSet*>(GetAttributeSet(Attribute.GetAttributeSetClass())));

		auto NewDelegate = &GetEntityAttributeValueChangeDelegate(Attribute);
		FOnEntityAttributeChangeData CallbackData;
		CallbackData.Attribute = Attribute;
		CallbackData.OldValue = OldValue;
		CallbackData.NewValue = NewValue;
		CallbackData.GEModData = CurrentModCallbackData;
		NewDelegate->Broadcast(CallbackData);
		OnAttributesValueChanged.Broadcast(CallbackData); // our own binding
	}

	CurrentModCallbackData = nullptr;
}


void UAttributeSystemComponent::AddActiveEffectModifiers(FActiveEntityEffect& ActiveEE)
{
	if (ActiveEE.Spec.Def == nullptr)
	{
		ATTRIBUTE_LOG(Error, TEXT("Null Effect Definition"));
		return;
	}

	// Register this active effects modifiers with each attributes aggregator if its non-periodic
	if (ActiveEE.Spec.GetPeriod() <= FEntityEffectConstants::NO_PERIOD)
	{
		// Loop through all modifiers, check if they're valid, and add them to the aggregator
		for (int32 ModIdx = 0; ModIdx < ActiveEE.Spec.Modifiers.Num(); ++ModIdx)
		{
			const FAttributeModifierInfo& ModInfo = ActiveEE.Spec.Modifiers[ModIdx];

			// Skip over modifiers for attributes that we don't have
			if (!HasAttributeSetForAttribute(ModInfo.Attribute)) continue;
			
			FAttributeModifierAggregator* Aggr = &FindOrCreateAttributeAggregator(ActiveEE.Spec.Modifiers[ModIdx].Attribute);
			if (ensure(Aggr))
			{
				Aggr->AddAggregatorMod(ModIdx, ActiveEE);
			}
		}
	}

	// TODO: Need to evaluate aggregators after adding our mods, wheres the best place to do it? Towards the end?
	for (auto& Aggr : AttributeAggregatorMap)
	{
		if (Aggr.Value.Get()->IsValid())
			InternalUpdateAttributeValue(Aggr.Key, Aggr.Value->EvaluateWithBase(GetAttributeBaseValue(Aggr.Key)));
	}
	
	ActiveEE.Spec.Def->OnApplied(this, ActiveEE);
}

void UAttributeSystemComponent::RemoveActiveEffectModifiers(FActiveEntityEffect& ActiveEE)
{
	// Update attribute aggregators, removing mods from this effect
	if (ActiveEE.Spec.GetPeriod() <= FEntityEffectConstants::NO_PERIOD)
	{
		for (const FAttributeModifierInfo& Mod : ActiveEE.Spec.Modifiers)
		{
			if (Mod.Attribute.IsValid())
			{
				FAttributeModifierAggregator* Aggr = FindAttributeAggregator(Mod.Attribute);
				if (Aggr)
				{
					Aggr->RemoveAggregatorMod(ActiveEE);
					// Update aggregator for attributes
					InternalUpdateAttributeValue(Mod.Attribute, Aggr->EvaluateWithBase(GetAttributeBaseValue(Mod.Attribute)));
				}
			}
		}
	}
}

void UAttributeSystemComponent::CheckDurationExpired(FActiveEntityEffectHandle EffectHandle)
{
	FActiveEntityEffect* EntityEffect = GetActiveEntityEffect(EffectHandle);
	
	if (!EntityEffect) return;

	FTimerManager& TimerManager = GetWorld()->GetTimerManager();
	
	// The duration may have changed since we registered this callback with the timer manager, so make sure if
	// duration was changed externally, we don't destroy the effect now
	float Duration = EntityEffect->GetDuration();
	float CurrentTime = GetWorld()->GetTimeSeconds();
	
	// Effect actually expired, throw it out
	if (Duration > 0.f && (((EntityEffect->StartWorldTime + Duration) < CurrentTime) || FMath::IsNearlyZero(CurrentTime - Duration - EntityEffect->StartWorldTime, KINDA_SMALL_NUMBER)))
	{
		// TODO: For now, we don't support changing an active effects duration, but it'd go here if we do (or we'd do a whole ass thing when handling it where we reset the delegate so we wouldn't need to worry about it
	}

	// Might be close to executing the periodic effect, lets check for that and manually execute it
	if (EntityEffect->PeriodHandle.IsValid() && TimerManager.TimerExists(EntityEffect->PeriodHandle))
	{
		float PeriodTimeRemaining = TimerManager.GetTimerRemaining(EntityEffect->PeriodHandle);
		if (PeriodTimeRemaining <= KINDA_SMALL_NUMBER)
		{
			ExecutePeriodicEffect(EffectHandle);
		}

		TimerManager.ClearTimer(EntityEffect->PeriodHandle);
	}

	// Properly remove the effect now
	InternalRemoveActiveEntityEffect(EffectHandle, false);
}

FAttributeModifierAggregator& UAttributeSystemComponent::FindOrCreateAttributeAggregator(const FEntityAttribute& Attribute)
{
	FAttributeModifierAggregator* Aggr = FindAttributeAggregator(Attribute);
	if (Aggr)
	{
		return *Aggr;
	}

	// No existing aggregator for this attribute, create one
	TSharedPtr<FAttributeModifierAggregator> NewAttributeAggregator = MakeShared<FAttributeModifierAggregator>();

	return *AttributeAggregatorMap.Add(Attribute, NewAttributeAggregator);
}

FAttributeModifierAggregator* UAttributeSystemComponent::FindAttributeAggregator(
	const FEntityAttribute& Attribute) const
{
	if (auto SP = AttributeAggregatorMap.Find(Attribute))
	{
		return SP->Get();
	}
	return nullptr;
}

/*--------------------------------------------------------------------------------------------------------------
* Owner Info
*--------------------------------------------------------------------------------------------------------------*/

void UAttributeSystemComponent::InitActorInfo(AActor* InOwnerActor)
{
	CurrentActorInfo.InitFromActor(InOwnerActor);
	OwnerActor = InOwnerActor;
}

/*--------------------------------------------------------------------------------------------------------------
* Gameplay Tags Interface
*--------------------------------------------------------------------------------------------------------------*/

void UAttributeSystemComponent::GetOwnedGameplayTags(FGameplayTagContainer& TagContainer) const
{
	if (CurrentActorInfo.ActionSystemComponent.IsValid())
	{
		CurrentActorInfo.ActionSystemComponent->GetOwnedGameplayTags(TagContainer);
	}
	else
	{
		TagContainer.AppendTags(AttributeSystemTags.GetExplicitGameplayTags());
	}
}

bool UAttributeSystemComponent::HasMatchingGameplayTag(FGameplayTag TagToCheck) const
{
	if (CurrentActorInfo.ActionSystemComponent.IsValid())
	{
		return CurrentActorInfo.ActionSystemComponent->HasMatchingGameplayTag(TagToCheck);
	}

	return AttributeSystemTags.HasMatchingGameplayTag(TagToCheck);
}

bool UAttributeSystemComponent::HasAllMatchingGameplayTags(const FGameplayTagContainer& TagContainer) const
{
	if (CurrentActorInfo.ActionSystemComponent.IsValid())
	{
		return CurrentActorInfo.ActionSystemComponent->HasAllMatchingGameplayTags(TagContainer);
	}
	
	return AttributeSystemTags.HasAllMatchingGameplayTags(TagContainer);
}

bool UAttributeSystemComponent::HasAnyMatchingGameplayTags(const FGameplayTagContainer& TagContainer) const
{
	if (CurrentActorInfo.ActionSystemComponent.IsValid())
	{
		return CurrentActorInfo.ActionSystemComponent->HasAnyMatchingGameplayTags(TagContainer);
	}
	
	return AttributeSystemTags.HasAnyMatchingGameplayTags(TagContainer);
}

void UAttributeSystemComponent::AddTag(const FGameplayTag& TagToAdd)
{
	if (CurrentActorInfo.ActionSystemComponent.IsValid())
	{
		CurrentActorInfo.ActionSystemComponent->AddTag(TagToAdd);
		return;
	}
	AttributeSystemTags.UpdateTagCount(TagToAdd, 1);
}

void UAttributeSystemComponent::AddTags(const FGameplayTagContainer& TagContainer)
{
	if (CurrentActorInfo.ActionSystemComponent.IsValid())
	{
		CurrentActorInfo.ActionSystemComponent->AddTags(TagContainer);
		return;
	}
	AttributeSystemTags.UpdateTagCount(TagContainer, 1);
}

bool UAttributeSystemComponent::RemoveTag(const FGameplayTag& TagToRemove)
{
	if (CurrentActorInfo.ActionSystemComponent.IsValid())
	{
		return CurrentActorInfo.ActionSystemComponent->RemoveTag(TagToRemove);
	}
	
	return AttributeSystemTags.UpdateTagCount(TagToRemove, -1);
}

void UAttributeSystemComponent::RemoveTags(const FGameplayTagContainer& TagContainer)
{
	if (CurrentActorInfo.ActionSystemComponent.IsValid())
	{
		CurrentActorInfo.ActionSystemComponent->RemoveTags(TagContainer);
		return;
	}
	
	AttributeSystemTags.UpdateTagCount(TagContainer, -1);
}

void UAttributeSystemComponent::AddCountOfTag(const FGameplayTag& TagToAdd, const int Count)
{
	if (CurrentActorInfo.ActionSystemComponent.IsValid())
	{
		CurrentActorInfo.ActionSystemComponent->AddCountOfTag(TagToAdd, Count);
		return;
	}
	
	AttributeSystemTags.UpdateTagCount(TagToAdd, Count);
}

void UAttributeSystemComponent::RemoveCountOfTag(const FGameplayTag& TagToRemove, const int Count)
{
	if (CurrentActorInfo.ActionSystemComponent.IsValid())
	{
		CurrentActorInfo.ActionSystemComponent->RemoveCountOfTag(TagToRemove, Count);
		return;
	}
	
	AttributeSystemTags.UpdateTagCount(TagToRemove, Count);
}

FOnGameplayTagCountChanged& UAttributeSystemComponent::RegisterGameplayTagEvent(FGameplayTag Tag,
	EGameplayTagEventType::Type EventType)
{
	if (CurrentActorInfo.ActionSystemComponent.IsValid())
	{
		return CurrentActorInfo.ActionSystemComponent->RegisterGameplayTagEvent(Tag, EventType);
	}
	
	return AttributeSystemTags.RegisterGameplayTagEvent(Tag, EventType);
}

void UAttributeSystemComponent::UnregisterGameplayTagEvent(FDelegateHandle DelegateHandle, FGameplayTag Tag,
	EGameplayTagEventType::Type EventType)
{
	if (CurrentActorInfo.ActionSystemComponent.IsValid())
	{
		CurrentActorInfo.ActionSystemComponent->UnregisterGameplayTagEvent(DelegateHandle, Tag, EventType);
		return;
	}
	
	AttributeSystemTags.RegisterGameplayTagEvent(Tag, EventType).Remove(DelegateHandle);
}

/*--------------------------------------------------------------------------------------------------------------
* DEBUG
*--------------------------------------------------------------------------------------------------------------*/

void UAttributeSystemComponent::VisualizeAttributes() const
{
	// Some constants
	static FVector HEIGHT_OFFSET = 90.f * FVector::UpVector;
	static FVector ATTRIBUTE_STRING_OFFSET = 10.f * FVector::UpVector;

	if (!OwnerActor) return;

	FVector ActorLoc = OwnerActor->GetActorLocation() + HEIGHT_OFFSET;
	FColor DebugColor = FColor::White;
	
	// Collect Attributes
	TMap<FName, float> AllAttributeValues = TMap<FName, float>();
	for (auto AttributeSet : GetSpawnedAttributeSets())
	{
		AllAttributeValues.Append(AttributeSet->GetAttributeValuePair());
	}

	// Draw them
	for (const auto& Attr : AllAttributeValues)
	{
		FString AttributeText = FString::Printf(TEXT("%s: %.2f"), *Attr.Key.ToString(), Attr.Value);
		DrawDebugString(GetWorld(), ActorLoc, AttributeText, nullptr, DebugColor, 0.f, true);

		ActorLoc += ATTRIBUTE_STRING_OFFSET;
	}
}
