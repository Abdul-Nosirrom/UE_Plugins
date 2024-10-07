// Copyright 2023 CoC All rights reserved


#include "ActionSystem/Conditions/ActionCondition_CostEffect.h"

#include "ActionSystem/GameplayAction.h"
#include "AttributeSystem/EntityEffectTypes.h"
#include "Components/AttributeSystemComponent.h"

void UActionCondition_CostEffect::Initialize(UGameplayAction* InOwnerAction)
{
	Super::Initialize(InOwnerAction);

	// Bind to OnStarted to trigger the effect
	OwnerAction->OnGameplayActionStarted.Event.AddUObject(this, &UActionCondition_CostEffect::ApplyEffect);
	OwnerAction->OnGameplayActionEnded.Event.AddUObject(this, &UActionCondition_CostEffect::OnOwnerActionEnded);

	// Create our effect spec
	if (CostEffect && GetCharacterInfo()->AttributeSystemComponent.IsValid())
	{
		EffectContext = FEntityEffectContext(GetCharacterInfo()->OwnerActor.Get(), GetCharacterInfo()->OwnerActor.Get());
		CostSpec = FEntityEffectSpec(CostEffect->GetDefaultObject<UEntityEffect>(), EffectContext);
	}
}

void UActionCondition_CostEffect::Cleanup()
{
	Super::Cleanup();
	GetWorld()->GetTimerManager().ClearTimer(CostEffectTimerHandle);
}

bool UActionCondition_CostEffect::DoesConditionPass()
{
	if (CostSpec.Def && GetCharacterInfo()->AttributeSystemComponent.IsValid())
	{
		if (GetCharacterInfo()->AttributeSystemComponent->CanApplyAttributeModifiers(CostSpec.Def, EffectContext))
		{
			return true;
		}
	}
	
	return false;
}

void UActionCondition_CostEffect::ApplyEffect()
{
	// Apply our cost effect if we have one
	if (CostEffect && GetCharacterInfo()->AttributeSystemComponent.IsValid())
	{
		ActiveEffectHandle = GetCharacterInfo()->AttributeSystemComponent->ApplyEntityEffectSpecToSelf(CostSpec);

		// If the effect is periodic / applied multiple times, bind to it and optionally cancel out of the action if we use up all the resource
		if (CostSpec.Def->DurationPolicy != EEntityEffectDurationType::Instant && CostSpec.Def->Period > 0.f)
		{
			// Clear existing handle if it exists
			CostEffectTimerHandle.Invalidate();
			
			CostEffectTimerDelegate.BindUObject(this, &UActionCondition_CostEffect::CheckPeriodicApplication);
			GetWorld()->GetTimerManager().SetTimer(CostEffectTimerHandle, CostEffectTimerDelegate, CostSpec.Def->Period, true);
		}
	}
}

void UActionCondition_CostEffect::CheckPeriodicApplication()
{
	if (CostSpec.Def && GetCharacterInfo()->AttributeSystemComponent.IsValid())
	{
		if (!GetCharacterInfo()->AttributeSystemComponent->CanApplyAttributeModifiers(CostSpec.Def, EffectContext))
		{
			GetCharacterInfo()->AttributeSystemComponent->RemoveActiveEntityEffect(ActiveEffectHandle);
			CostEffectTimerHandle.Invalidate();
		}
	}
}

void UActionCondition_CostEffect::OnOwnerActionEnded()
{
	// Stop timer
	CostEffectTimerHandle.Invalidate();

	// Maybe remove effect
	if (bStopDurationEffectOnActionEnd && CostSpec.Def->DurationPolicy != EEntityEffectDurationType::Instant)
	{
		GetCharacterInfo()->AttributeSystemComponent->RemoveActiveEntityEffect(ActiveEffectHandle);
	}
}
