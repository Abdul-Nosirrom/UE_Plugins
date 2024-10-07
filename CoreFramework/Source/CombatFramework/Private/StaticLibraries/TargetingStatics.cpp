// Copyright 2023 CoC All rights reserved


#include "StaticLibraries/TargetingStatics.h"

#include "Debug/CombatFrameworkLog.h"
#include "Helpers/CombatFrameworkSettings.h"

bool UTargetingStatics::PerformTargeting(AActor* Instigator, FTargetingSettings TargetingSettings, FTargetingResult& OutResult)
{
	if (!Instigator)
	{
		TARGETING_LOG(Error, TEXT("Attempted to perform targeting with null instigator, aborting"));
		return false;
	}

	TargetingSettings.Instigator = Instigator;
	TArray<FOverlapResult> OverlapResults;
	const FCollisionShape Shape = FCollisionShape::MakeSphere(TargetingSettings.GetSphereRadius());
	const FVector CenterPosition = TargetingSettings.GetCenterPosition();
	const FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(TargetingRequest), false, Instigator);

	if (Instigator->GetWorld()->OverlapMultiByProfile(OverlapResults, CenterPosition, FQuat::Identity, GetTargetingCollisionProfile(), Shape, QueryParams))
	{
		return TargetingSettings.ProcessTargets(OverlapResults, OutResult);
	}

	return false;
}

bool UTargetingStatics::PerformTargetingFromPreset(AActor* Instigator, UTargetingPreset* TargetingSettings,
	FTargetingResult& OutResult)
{
	if (TargetingSettings)
	{
		return PerformTargeting(Instigator, TargetingSettings->GetSettings(), OutResult);
	}
	return false;
}

ECollisionChannel UTargetingStatics::GetTargetingTraceChannel()
{
	if (GetCombatFrameworkSettings_Internal() == nullptr)
	{
		TARGETING_LOG(Warning, TEXT("Tried to retrieve targeting trace channel, but developer settings were null!"));
		return ECC_EngineTraceChannel1;
	}

	return GetCombatFrameworkSettings_Internal()->GetTargetingTraceChannel();
}

FName UTargetingStatics::GetTargetingCollisionProfile()
{
	if (GetCombatFrameworkSettings_Internal() == nullptr)
	{
		TARGETING_LOG(Warning, TEXT("Tried to retrieve targeting profile name, but developer settings were null!"));
		return NAME_None;
	}

	return GetCombatFrameworkSettings_Internal()->GetTargetingProfileName();
}
