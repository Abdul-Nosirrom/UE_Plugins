// Copyright 2023 CoC All rights reserved


#include "StaticLibraries/CombatStatics.h"

#include "NiagaraFunctionLibrary.h"
#include "ActionSystem/ActionSystemInterface.h"
#include "AttributeSystem/AttributeSystemInterface.h"
#include "Components/AttributeSystemComponent.h"
#include "Components/CombatActionSystemComponent.h"
#include "Interfaces/DamageableInterface.h"
#include "Subsystems/CombatUtilitiesSubsystem.h"

UCombatActionSystemComponent* UCombatStatics::GetCombatSystemFromActor(const AActor* Actor, bool bLookForComponent)
{
	if (Actor == nullptr)
	{
		return nullptr;
	}

	const IActionSystemInterface* ASI = Cast<IActionSystemInterface>(Actor);
	if (ASI)
	{
		return Cast<UCombatActionSystemComponent>(ASI->GetActionSystem());
	}

	if (bLookForComponent)
	{
		// Fall back to a component search to better support BP-only actors
		return Actor->FindComponentByClass<UCombatActionSystemComponent>();
	}

	return nullptr;
}

bool UCombatStatics::CombatHit(FAttackData AttackData, AActor* Attacker, AActor* Victim)
{
	if (!Attacker || !Victim) return false;
	if (!Victim->Implements<UDamageableInterface>()) return false;

	// Does the attacker allow for them to be hit?
	// NOTE: Bad way of doing this, just doing it for testing
	if (const auto CSC = GetCombatSystemFromActor(Attacker))
	{
		if (!CSC->CanAttackTarget(Victim))
		{
			return false;
		}
	}
	
	if (IDamageableInterface::Execute_GetHit(Victim, AttackData, Attacker))
	{
		if (Attacker->Implements<UDamageableInterface>())
			IDamageableInterface::Execute_OnHitConfirm(Attacker, AttackData, Victim);

		// Set HitPause on self & attacker
		if (AttackData.HitStop > 0.f)
		{
			Victim->GetWorld()->GetSubsystem<UCombatUtilitiesSubsystem>()->SetHitStop(Victim, Attacker, AttackData.HitStop);
		}
	
		// Spawn HitSpark
		{
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(Victim->GetWorld(), AttackData.HitSparkEffect, Victim->GetActorLocation());
		}

		// Apply entity effect
		if (AttackData.EffectToApply)
		{
			if (auto IASC = Cast<IAttributeSystemInterface>(Victim))
			{
				auto ASC = IASC->GetAttributeSystem();
				
				const FEntityEffectContext EffectContext(Attacker, Attacker);
				ASC->ApplyEntityEffectToSelf(AttackData.EffectToApply, EffectContext);
			}
		}

		// Broadcast their hit confirm event
		if (const auto VictimInterface = Cast<IDamageableInterface>(Victim))
		{
			VictimInterface->OnGotHitDelegate.Broadcast(Victim);
		}
		
		return true;
	}

	return false;
}
