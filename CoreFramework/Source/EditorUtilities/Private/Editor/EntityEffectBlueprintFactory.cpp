// Copyright 2023 CoC All rights reserved


#include "Editor/EntityEffectBlueprintFactory.h"
#include "AttributeSystem/EntityEffect.h"
#include "AttributeSystem/EntityEffectBlueprint.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Widgets/SBlueprintClassCreateDialog.h"


#include UE_INLINE_GENERATED_CPP_BY_NAME(EntityEffectBlueprintFactory)

#define LOCTEXT_NAMESPACE "UEntityEffectBlueprintFactory"

UEntityEffectBlueprintFactory::UEntityEffectBlueprintFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UEntityEffectBlueprint::StaticClass();
	ParentClass = UEntityEffect::StaticClass();
}


bool UEntityEffectBlueprintFactory::ConfigureProperties()
{
	TSharedRef<SBlueprintClassCreateDialog> Dialog = SNew(SBlueprintClassCreateDialog)
														.BaseClass(UEntityEffect::StaticClass());
	return Dialog->ConfigureProperties(this);
}

UObject* UEntityEffectBlueprintFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags,
	UObject* Context, FFeedbackContext* Warn, FName CallingContext)
{
	// Make sure we are trying to factory a gameplay ability blueprint, then create and init one
	check(Class->IsChildOf(UEntityEffectBlueprint::StaticClass()));

	// If they selected an interface, force the parent class to be UInterface
	if (BlueprintType == BPTYPE_Interface)
	{
		ParentClass = UInterface::StaticClass();
	}

	if ( ( ParentClass == NULL ) || !FKismetEditorUtilities::CanCreateBlueprintOfClass(ParentClass) || !ParentClass->IsChildOf(UEntityEffect::StaticClass()) )
	{
		FFormatNamedArguments Args;
		Args.Add( TEXT("ClassName"), (ParentClass != NULL) ? FText::FromString( ParentClass->GetName() ) : LOCTEXT("Null", "(null)") );
		FMessageDialog::Open( EAppMsgType::Ok, FText::Format( LOCTEXT("CannotCreateEntityEffectBlueprint", "Cannot create a Entity Effect Blueprint based on the class '{ClassName}'."), Args ) );
		return NULL;
	}
	else
	{
		UEntityEffectBlueprint* NewBP = CastChecked<UEntityEffectBlueprint>(FKismetEditorUtilities::CreateBlueprint(ParentClass, InParent, Name, BlueprintType, UEntityEffectBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass(), CallingContext));

		return NewBP;
	}
}

UObject* UEntityEffectBlueprintFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags,
	UObject* Context, FFeedbackContext* Warn)
{
	return FactoryCreateNew(Class, InParent, Name, Flags, Context, Warn, NAME_None);
}

#undef LOCTEXT_NAMESPACE
