// Fill out your copyright notice in the Description page of Project Settings.


#include "Editor/ActionDataBlueprintFactory.h"

#include "BlueprintEditorSettings.h"
#include "SlateOptMacros.h"
#include "ActionSystem/GameplayAction.h"
#include "ActionSystem/ActionBlueprint.h"
#include "ClassViewer/Private/UnloadedBlueprintData.h"
#include "Editor/ActionGraph.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Widgets/Layout/SUniformGridPanel.h"


#define LOCTEXT_NAMESPACE "UActionDataBlueprintFactory"


UActionDataBlueprintFactory::UActionDataBlueprintFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UActionDataBlueprint::StaticClass();
	ParentClass = UGameplayActionData::StaticClass();
}

UObject* UActionDataBlueprintFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags,
	UObject* Context, FFeedbackContext* Warn, FName CallingContext)
{
	// Make sure we are trying to factory a gameplay ability blueprint, then create and init one
	check(Class->IsChildOf(UActionDataBlueprint::StaticClass()));

	// If they selected an interface, force the parent class to be UInterface
	if (BlueprintType == BPTYPE_Interface)
	{
		ParentClass = UInterface::StaticClass();
	}

	if ( ( ParentClass == NULL ) || !FKismetEditorUtilities::CanCreateBlueprintOfClass(ParentClass) || !ParentClass->IsChildOf(UGameplayActionData::StaticClass()) )
	{
		FFormatNamedArguments Args;
		Args.Add( TEXT("ClassName"), (ParentClass != NULL) ? FText::FromString( ParentClass->GetName() ) : LOCTEXT("Null", "(null)") );
		FMessageDialog::Open( EAppMsgType::Ok, FText::Format( LOCTEXT("CannotCreateGameplayActionBlueprint", "Cannot create a Gameplay Action Data Blueprint based on the class '{ClassName}'."), Args ) );
		return NULL;
	}
	else
	{
		UActionDataBlueprint* NewBP = CastChecked<UActionDataBlueprint>(FKismetEditorUtilities::CreateBlueprint(ParentClass, InParent, Name, BlueprintType, UActionDataBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass(), CallingContext));
		return NewBP;
	}
}

UObject* UActionDataBlueprintFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags,
	UObject* Context, FFeedbackContext* Warn)
{
	return FactoryCreateNew(Class, InParent, Name, Flags, Context, Warn, NAME_None);
}

#undef LOCTEXT_NAMESPACE