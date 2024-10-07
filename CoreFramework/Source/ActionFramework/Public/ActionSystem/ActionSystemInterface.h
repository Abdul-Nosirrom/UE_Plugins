// Copyright 2023 CoC All rights reserved

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "ActionSystemInterface.generated.h"

// This class does not need to be modified.
UINTERFACE(MinimalAPI, NotBlueprintable)
class UActionSystemInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class ACTIONFRAMEWORK_API IActionSystemInterface
{
	GENERATED_BODY()

	// Add interface functions to this class. This is the class that will be inherited to implement this interface.
public:
	UFUNCTION(Category=Actions, BlueprintCallable)
	virtual class UActionSystemComponent* GetActionSystem() const = 0;
};
