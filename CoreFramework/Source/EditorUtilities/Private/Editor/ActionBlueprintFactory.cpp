// 


#include "Editor/ActionBlueprintFactory.h"

#include "BlueprintEditorSettings.h"
#include "SlateOptMacros.h"
#include "ActionSystem/GameplayAction.h"
#include "ActionSystem/ActionBlueprint.h"
#include "Editor/ActionBlueprintSchema.h"
#include "Editor/ActionGraph.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Widgets/SBlueprintClassCreateDialog.h"


#include UE_INLINE_GENERATED_CPP_BY_NAME(ActionBlueprintFactory)

#ifndef LOCTEXT_NAMESPACE
#define LOCTEXT_NAMESPACE "UActionBlueprintFactory"
#endif

UActionBlueprintFactory::UActionBlueprintFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UActionBlueprint::StaticClass();
	ParentClass = UGameplayAction::StaticClass();
}


bool UActionBlueprintFactory::ConfigureProperties()
{
	TSharedRef<SBlueprintClassCreateDialog> Dialog = SNew(SBlueprintClassCreateDialog).BaseClass(UGameplayAction::StaticClass());
	return Dialog->ConfigureProperties(this);
}

UObject* UActionBlueprintFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags,
	UObject* Context, FFeedbackContext* Warn, FName CallingContext)
{
	// Make sure we are trying to factory a gameplay ability blueprint, then create and init one
	check(Class->IsChildOf(UActionBlueprint::StaticClass()));

	// If they selected an interface, force the parent class to be UInterface
	if (BlueprintType == BPTYPE_Interface)
	{
		ParentClass = UInterface::StaticClass();
	}

	if ( ( ParentClass == NULL ) || !FKismetEditorUtilities::CanCreateBlueprintOfClass(ParentClass) || !ParentClass->IsChildOf(UGameplayAction::StaticClass()) )
	{
		FFormatNamedArguments Args;
		Args.Add( TEXT("ClassName"), (ParentClass != NULL) ? FText::FromString( ParentClass->GetName() ) : LOCTEXT("Null", "(null)") );
		FMessageDialog::Open( EAppMsgType::Ok, FText::Format( LOCTEXT("CannotCreateGameplayActionBlueprint", "Cannot create a Gameplay Action Blueprint based on the class '{ClassName}'."), Args ) );
		return NULL;
	}
	else
	{
		UActionBlueprint* NewBP = CastChecked<UActionBlueprint>(FKismetEditorUtilities::CreateBlueprint(ParentClass, InParent, Name, BlueprintType, UActionBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass(), CallingContext));

		if (NewBP)
		{
			UActionBlueprint* AbilityBP = UActionBlueprint::FindRootGameplayAbilityBlueprint(NewBP);
			if (AbilityBP == NULL)
			{
				const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();

				// Only allow a gameplay ability graph if there isn't one in a parent blueprint
				UEdGraph* NewGraph = FBlueprintEditorUtils::CreateNewGraph(NewBP, TEXT("Gameplay Action Graph"), UActionGraph::StaticClass(), UActionBlueprintSchema::StaticClass());
#if WITH_EDITORONLY_DATA
				if (NewBP->UbergraphPages.Num())
				{
					FBlueprintEditorUtils::RemoveGraphs(NewBP, NewBP->UbergraphPages);
				}
#endif
				FBlueprintEditorUtils::AddUbergraphPage(NewBP, NewGraph);
				NewBP->LastEditedDocuments.Add(NewGraph);
				NewGraph->bAllowDeletion = false;

				UBlueprintEditorSettings* Settings = GetMutableDefault<UBlueprintEditorSettings>();
				if(Settings && Settings->bSpawnDefaultBlueprintNodes)
				{
					int32 NodePositionY = 0;
					FKismetEditorUtilities::AddDefaultEventNode(NewBP, NewGraph, FName(TEXT("OnActionActivated")), UGameplayAction::StaticClass(), NodePositionY);
					FKismetEditorUtilities::AddDefaultEventNode(NewBP, NewGraph, FName(TEXT("OnActionTick")), UGameplayAction::StaticClass(), NodePositionY);
					FKismetEditorUtilities::AddDefaultEventNode(NewBP, NewGraph, FName(TEXT("OnActionEnd")), UGameplayAction::StaticClass(), NodePositionY);
				}
			}
		}

		return NewBP;
	}
}

UObject* UActionBlueprintFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags,
	UObject* Context, FFeedbackContext* Warn)
{
	return FactoryCreateNew(Class, InParent, Name, Flags, Context, Warn, NAME_None);
}

