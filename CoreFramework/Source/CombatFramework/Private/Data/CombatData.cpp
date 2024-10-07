// Copyright 2023 CoC All rights reserved


#include "Data/CombatData.h"

#include "Kismet/KismetMathLibrary.h"

FVector FAttackData::GetKnockBackVector(AActor* Attacker, AActor* Victim) const
{
	FQuat KnockBackOrientation = FQuat::Identity;
	FVector KnockBackVector = KnockBack;
	
	switch (KnockBackReferenceFrame)
	{
		case EAttackKnockBackType::Frontal:
			KnockBackOrientation = UKismetMathLibrary::MakeRotFromX(Attacker->GetActorForwardVector()).Quaternion();
		break;
		case EAttackKnockBackType::Radial:
			KnockBackOrientation = UKismetMathLibrary::MakeRotFromX((Victim->GetActorLocation() - Attacker->GetActorLocation())).Quaternion();
		break;
		case EAttackKnockBackType::Radial2D:
			KnockBackOrientation = UKismetMathLibrary::MakeRotFromX((Victim->GetActorLocation() - Attacker->GetActorLocation()).GetSafeNormal2D()).Quaternion();
		break;
	}

	return KnockBackOrientation * KnockBackVector;
}

FVector FAttackData::GetAnimationDirection(AActor* Attacker, AActor* Victim) const
{
	return UKismetMathLibrary::FindLookAtRotation(Attacker->GetActorLocation(), Victim->GetActorLocation()).RotateVector(AnimDirection);;
}
