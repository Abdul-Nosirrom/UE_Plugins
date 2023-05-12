// 

#pragma once

#include "CoreMinimal.h"
#include "Engine/Blueprint.h"
#include "UObject/Object.h"
#include "ActionBlueprint.generated.h"

/**
 * 
 */
UCLASS()
class ACTIONFRAMEWORK_API UActionBlueprint : public UBlueprint
{
	GENERATED_UCLASS_BODY()

#if WITH_EDITOR

	// UBlueprint interface
	virtual bool SupportedByDefaultBlueprintFactory() const override
	{
		return false;
	}
	// End of UBlueprint interface

	/** Returns the most base gameplay ability blueprint for a given blueprint (if it is inherited from another ability blueprint, returning null if only native / non-ability BP classes are it's parent) */
	static UActionBlueprint* FindRootGameplayAbilityBlueprint(UActionBlueprint* DerivedBlueprint);

#endif
};

UCLASS()
class ACTIONFRAMEWORK_API UActionDataBlueprint : public UBlueprint
{
	GENERATED_UCLASS_BODY()

#if WITH_EDITOR

	// UBlueprint interface
	virtual bool SupportedByDefaultBlueprintFactory() const override
	{
		return false;
	}
	// End of UBlueprint interface

	/** Returns the most base gameplay ability blueprint for a given blueprint (if it is inherited from another ability blueprint, returning null if only native / non-ability BP classes are it's parent) */
	static UActionDataBlueprint* FindRootGameplayAbilityBlueprint(UActionDataBlueprint* DerivedBlueprint);

#endif
};
