// Copyright 2023 CoC All rights reserved


#include "Subsystems/CombatUtilitiesSubsystem.h"

void UCombatUtilitiesSubsystem::SetHitStop(AActor* Attacker, AActor* Victim, float Duration)
{
	if (!Attacker || !Victim) return;

	Attacker->CustomTimeDilation = 0.f;
	Victim->CustomTimeDilation = 0.f;
	FTimerHandle Handle;
	FTimerDelegate Delegate;
	Delegate.BindLambda([Attacker, Victim]()
	{
		Attacker->CustomTimeDilation = 1.f;
		if (Victim)
			Victim->CustomTimeDilation = 1.f;
	});
	GetWorld()->GetTimerManager().SetTimer(Handle, Delegate, Duration, false);
}
