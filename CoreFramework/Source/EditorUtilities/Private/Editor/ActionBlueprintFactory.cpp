﻿// 


#include "Editor/ActionBlueprintFactory.h"

#include "BlueprintEditorSettings.h"
#include "SlateOptMacros.h"
#include "ActionSystem/GameplayAction.h"
#include "ActionSystem/ActionBlueprint.h"
#include "ClassViewer/Private/UnloadedBlueprintData.h"
#include "Editor/ActionBlueprintSchema.h"
#include "Editor/ActionGraph.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Widgets/Layout/SUniformGridPanel.h"


#include UE_INLINE_GENERATED_CPP_BY_NAME(ActionBlueprintFactory)

#define LOCTEXT_NAMESPACE "UActionBlueprintFactory"


// ------------------------------------------------------------------------------
// Dialog to configure creation properties
// ------------------------------------------------------------------------------
BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

class SGameplayActionBlueprintCreateDialog : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SGameplayActionBlueprintCreateDialog){}

	SLATE_END_ARGS()

		/** Constructs this widget with InArgs */
		void Construct(const FArguments& InArgs)
	{
			bOkClicked = false;
			ParentClass = UGameplayAction::StaticClass();

			ChildSlot
				[
					SNew(SBorder)
					.Visibility(EVisibility::Visible)
					.BorderImage(FAppStyle::GetBrush("Menu.Background"))
					[
						SNew(SBox)
						.Visibility(EVisibility::Visible)
						.WidthOverride(500.0f)
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot()
							.FillHeight(1)
							[
								SNew(SBorder)
								.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
								.Content()
								[
									SAssignNew(ParentClassContainer, SVerticalBox)
								]
							]

							// Ok/Cancel buttons
							+ SVerticalBox::Slot()
								.AutoHeight()
								.HAlign(HAlign_Right)
								.VAlign(VAlign_Bottom)
								.Padding(8)
								[
									SNew(SUniformGridPanel)
									.SlotPadding(FAppStyle::GetMargin("StandardDialog.SlotPadding"))
									.MinDesiredSlotWidth(FAppStyle::GetFloat("StandardDialog.MinDesiredSlotWidth"))
									.MinDesiredSlotHeight(FAppStyle::GetFloat("StandardDialog.MinDesiredSlotHeight"))
									+ SUniformGridPanel::Slot(0, 0)
									[
										SNew(SButton)
										.HAlign(HAlign_Center)
										.ContentPadding(FAppStyle::GetMargin("StandardDialog.ContentPadding"))
										.OnClicked(this, &SGameplayActionBlueprintCreateDialog::OkClicked)
										.Text(LOCTEXT("CreateGameplayActionBlueprintOk", "OK"))
									]
									+ SUniformGridPanel::Slot(1, 0)
										[
											SNew(SButton)
											.HAlign(HAlign_Center)
											.ContentPadding(FAppStyle::GetMargin("StandardDialog.ContentPadding"))
											.OnClicked(this, &SGameplayActionBlueprintCreateDialog::CancelClicked)
											.Text(LOCTEXT("CreateGameplayActionBlueprintCancel", "Cancel"))
										]
								]
						]
					]
				];

			MakeParentClassPicker();
		}

	/** Sets properties for the supplied GameplayAbilitiesBlueprintFactory */
	bool ConfigureProperties(TWeakObjectPtr<UActionBlueprintFactory> InGameplayAbilitiesBlueprintFactory)
	{
		GameplayAbilitiesBlueprintFactory = InGameplayAbilitiesBlueprintFactory;

		TSharedRef<SWindow> Window = SNew(SWindow)
			.Title(LOCTEXT("CreateGameplayActionBlueprintOptions", "Create Gameplay Action Blueprint"))
			.ClientSize(FVector2D(400, 700))
			.SupportsMinimize(false).SupportsMaximize(false)
			[
				AsShared()
			];
		
		PickerWindow = Window;

		GEditor->EditorAddModalWindow(Window);
		GameplayAbilitiesBlueprintFactory.Reset();

		return bOkClicked;
	}

private:
	class FGameplayActionBlueprintParentFilter : public IClassViewerFilter
	{
	public:
		/** All children of these classes will be included unless filtered out by another setting. */
		TSet< const UClass* > AllowedChildrenOfClasses;

		FGameplayActionBlueprintParentFilter() {}

		virtual bool IsClassAllowed(const FClassViewerInitializationOptions& InInitOptions, const UClass* InClass, TSharedRef< FClassViewerFilterFuncs > InFilterFuncs) override
		{
			// If it appears on the allowed child-of classes list (or there is nothing on that list)
			return InFilterFuncs->IfInChildOfClassesSet(AllowedChildrenOfClasses, InClass) != EFilterReturn::Failed;
		}

		virtual bool IsUnloadedClassAllowed(const FClassViewerInitializationOptions& InInitOptions, const TSharedRef< const IUnloadedBlueprintData > InUnloadedClassData, TSharedRef< FClassViewerFilterFuncs > InFilterFuncs) override
		{
			// If it appears on the allowed child-of classes list (or there is nothing on that list)
			return InFilterFuncs->IfInChildOfClassesSet(AllowedChildrenOfClasses, InUnloadedClassData) != EFilterReturn::Failed;
		}
	};

	/** Creates the combo menu for the parent class */
	void MakeParentClassPicker()
	{
		// Load the classviewer module to display a class picker
		FClassViewerModule& ClassViewerModule = FModuleManager::LoadModuleChecked<FClassViewerModule>("ClassViewer");

		// Fill in options
		FClassViewerInitializationOptions Options;
		Options.Mode = EClassViewerMode::ClassPicker;

		// Only allow parenting to base blueprints.
		Options.bIsBlueprintBaseOnly = true;

		TSharedPtr<FGameplayActionBlueprintParentFilter> Filter = MakeShareable(new FGameplayActionBlueprintParentFilter);

		// All child child classes of UGameplayAbility are valid.
		Filter->AllowedChildrenOfClasses.Add(UGameplayAction::StaticClass());
		Options.ClassFilters.Add(Filter.ToSharedRef());

		ParentClassContainer->ClearChildren();
		ParentClassContainer->AddSlot()
			.AutoHeight()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("ParentClass", "Parent Class:"))
				.ShadowOffset(FVector2D(1.0f, 1.0f))
			];

		ParentClassContainer->AddSlot()
			[
				ClassViewerModule.CreateClassViewer(Options, FOnClassPicked::CreateSP(this, &SGameplayActionBlueprintCreateDialog::OnClassPicked))
			];
	}

	/** Handler for when a parent class is selected */
	void OnClassPicked(UClass* ChosenClass)
	{
		ParentClass = ChosenClass;
	}

	/** Handler for when ok is clicked */
	FReply OkClicked()
	{
		if (GameplayAbilitiesBlueprintFactory.IsValid())
		{
			GameplayAbilitiesBlueprintFactory->BlueprintType = BPTYPE_Normal;
			GameplayAbilitiesBlueprintFactory->ParentClass = ParentClass.Get();
		}

		CloseDialog(true);

		return FReply::Handled();
	}

	void CloseDialog(bool bWasPicked = false)
	{
		bOkClicked = bWasPicked;
		if (PickerWindow.IsValid())
		{
			PickerWindow.Pin()->RequestDestroyWindow();
		}
	}

	/** Handler for when cancel is clicked */
	FReply CancelClicked()
	{
		CloseDialog();
		return FReply::Handled();
	}

	FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
	{
		if (InKeyEvent.GetKey() == EKeys::Escape)
		{
			CloseDialog();
			return FReply::Handled();
		}
		return SWidget::OnKeyDown(MyGeometry, InKeyEvent);
	}

private:
	/** The factory for which we are setting up properties */
	TWeakObjectPtr<UActionBlueprintFactory> GameplayAbilitiesBlueprintFactory;

	/** A pointer to the window that is asking the user to select a parent class */
	TWeakPtr<SWindow> PickerWindow;

	/** The container for the Parent Class picker */
	TSharedPtr<SVerticalBox> ParentClassContainer;

	/** The selected class */
	TWeakObjectPtr<UClass> ParentClass;

	/** True if Ok was clicked */
	bool bOkClicked;
};

END_SLATE_FUNCTION_BUILD_OPTIMIZATION


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
	TSharedRef<SGameplayActionBlueprintCreateDialog> Dialog = SNew(SGameplayActionBlueprintCreateDialog);
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

#undef LOCTEXT_NAMESPACE