// Copyright 2023 CoC All rights reserved


#include "Editor/AttributeInitDataTableFactory.h"
#include "AttributeSystem/EntityAttributeSet.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AttributeInitDataTableFactory)


UAttributeInitDataTableFactory::UAttributeInitDataTableFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SupportedClass = UAttributeInitDataTable::StaticClass();
	bCreateNew = true;
	bEditAfterNew = true;
}

UObject* UAttributeInitDataTableFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name,
	EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	check(Class && Class->IsChildOf(UAttributeInitDataTable::StaticClass()));
	return NewObject<UAttributeInitDataTable>(InParent, Class, Name, Flags);
}
