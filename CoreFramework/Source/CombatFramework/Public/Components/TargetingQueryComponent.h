// Copyright 2023 CoC All rights reserved

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Components/SphereComponent.h"
#include "TargetingQueryComponent.generated.h"


UCLASS(ClassGroup=(Combat), meta=(BlueprintSpawnableComponent, DisplayName="Targeting Query"), hidecategories=(Shape, Object,LOD,Lighting,Transform,Sockets,TextureStreaming))
class COMBATFRAMEWORK_API UTargetingQueryComponent : public USphereComponent
{
	GENERATED_BODY()

	friend struct FTargetingSettings;

public:
	// Sets default values for this component's properties
	UTargetingQueryComponent();

	virtual void BeginPlay() override;
	
	bool EvaluateTargetingTags(const FTargetingSettings& InTargeting) const;

protected:
	UPROPERTY(Category=Targeting, EditAnywhere, meta=(GameplayTagFilter="Targeting"))
	FGameplayTagContainer IdentityTags;
};
