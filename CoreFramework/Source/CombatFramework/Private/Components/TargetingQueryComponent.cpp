// Copyright 2023 CoC All rights reserved


#include "Components/TargetingQueryComponent.h"

#include "Data/TargetingPreset.h"
#include "StaticLibraries/TargetingStatics.h"

// Sets default values for this component's properties
UTargetingQueryComponent::UTargetingQueryComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = false;

	SphereRadius = 50.f;
}

void UTargetingQueryComponent::BeginPlay()
{
	Super::BeginPlay();
	
	SetCollisionProfileName(UTargetingStatics::GetTargetingCollisionProfile(), true);
}

bool UTargetingQueryComponent::EvaluateTargetingTags(const FTargetingSettings& InTargeting) const
{
	if (!IdentityTags.HasAll(InTargeting.MustHaveAllTags)) return false;
	if (!IdentityTags.HasAny(InTargeting.MustHaveAnyTags)) return false;
	if (IdentityTags.HasAny(InTargeting.MustNotHaveTags)) return false;
	return true;
}

