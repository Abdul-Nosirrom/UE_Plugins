// Copyright 2023 CoC All rights reserved

#pragma once

#include "CoreMinimal.h"
#include "Engine/Blueprint.h"
#include "EntityEffectBlueprint.generated.h"

/**
 * 
 */
UCLASS()
class ACTIONFRAMEWORK_API UEntityEffectBlueprint : public UBlueprint
{
	GENERATED_BODY()

#if WITH_EDITOR
	// UBlueprint interface
	virtual bool SupportedByDefaultBlueprintFactory() const override
	{
		return false;
	}
	// End of UBlueprint interface

	static UEntityEffectBlueprint* FindRootEntityEffectBlueprint(UEntityEffectBlueprint* DerivedBlueprint);
#endif 
};
