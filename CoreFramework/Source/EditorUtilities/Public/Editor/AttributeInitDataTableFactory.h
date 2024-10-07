// Copyright 2023 CoC All rights reserved

#pragma once

#include "CoreMinimal.h"
#include "AttributeInitDataTableFactory.generated.h"


UCLASS()
class UAttributeInitDataTableFactory : public UFactory
{
	GENERATED_UCLASS_BODY()

	// UFactory interface
		virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;
	// End of UFactory interface
};
