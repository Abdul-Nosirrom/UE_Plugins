// 

#pragma once

#include "CoreMinimal.h"
#include "Factories/BlueprintFactory.h"
#include "ActionBlueprintFactory.generated.h"

/*--------------------------------------------------------------------------------------------------------------
* Gameplay Action
*--------------------------------------------------------------------------------------------------------------*/

UCLASS(HideCategories=Object, MinimalAPI)
class UActionBlueprintFactory : public UBlueprintFactory
{
	GENERATED_UCLASS_BODY()

	//~ Begin UFactory Interface
	virtual bool ConfigureProperties() override;
	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn, FName CallingContext) override;
	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;
	//~ Begin UFactory Interface	
};