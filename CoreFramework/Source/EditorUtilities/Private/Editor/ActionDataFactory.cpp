// Fill out your copyright notice in the Description page of Project Settings.


#include "Editor/ActionDataFactory.h"

#include "SlateOptMacros.h"
#include "ActionSystem/GameplayAction.h"
#include "ClassViewer/Private/UnloadedBlueprintData.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Widgets/Layout/SUniformGridPanel.h"

#define LOCTEXT_NAMESPACE "UActionDataFactory"

// ------------------------------------------------------------------------------
// Dialog to configure creation properties
// ------------------------------------------------------------------------------
BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

class SGameplayActionDataCreateDialog : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SGameplayActionDataCreateDialog){}

	SLATE_END_ARGS()

		/** Constructs this widget with InArgs */
		void Construct(const FArguments& InArgs)
	{
			bOkClicked = false;
			ParentClass = UGameplayActionData::StaticClass();

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
										.OnClicked(this, &SGameplayActionDataCreateDialog::OkClicked)
										.Text(LOCTEXT("CreateGameplayActionBlueprintOk", "OK"))
									]
									+ SUniformGridPanel::Slot(1, 0)
										[
											SNew(SButton)
											.HAlign(HAlign_Center)
											.ContentPadding(FAppStyle::GetMargin("StandardDialog.ContentPadding"))
											.OnClicked(this, &SGameplayActionDataCreateDialog::CancelClicked)
											.Text(LOCTEXT("CreateGameplayActionBlueprintCancel", "Cancel"))
										]
								]
						]
					]
				];

			MakeParentClassPicker();
		}

	/** Sets properties for the supplied GameplayAbilitiesBlueprintFactory */
	bool ConfigureProperties(TWeakObjectPtr<UActionDataFactory> InGameplayActionDataFactory)
	{
		GameplayActionDataFactory = InGameplayActionDataFactory;

		TSharedRef<SWindow> Window = SNew(SWindow)
			.Title(LOCTEXT("CreateGameplayActionDataOptions", "Create Gameplay Action Data PLEASE"))
			.ClientSize(FVector2D(400, 700))
			.SupportsMinimize(false).SupportsMaximize(false)
			[
				AsShared()
			];
		
		PickerWindow = Window;

		GEditor->EditorAddModalWindow(Window);
		GameplayActionDataFactory.Reset();

		return bOkClicked;
	}

private:
	class FGameplayActionDataParentFilter : public IClassViewerFilter
	{
	public:
		/** All children of these classes will be included unless filtered out by another setting. */
		TSet< const UClass* > AllowedChildrenOfClasses;

		FGameplayActionDataParentFilter() {}

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

		TSharedPtr<FGameplayActionDataParentFilter> Filter = MakeShareable(new FGameplayActionDataParentFilter);

		// All child child classes of UGameplayAbility are valid.
		Filter->AllowedChildrenOfClasses.Add(UGameplayActionData::StaticClass());
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
				ClassViewerModule.CreateClassViewer(Options, FOnClassPicked::CreateSP(this, &SGameplayActionDataCreateDialog::OnClassPicked))
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
		if (GameplayActionDataFactory.IsValid())
		{
			GameplayActionDataFactory->ParentClass = ParentClass.Get();
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
	TWeakObjectPtr<UActionDataFactory> GameplayActionDataFactory;

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

UActionDataFactory::UActionDataFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UGameplayActionData::StaticClass();
	ParentClass = UGameplayActionData::StaticClass();
}

bool UActionDataFactory::ConfigureProperties()
{
	TSharedRef<SGameplayActionDataCreateDialog> Dialog = SNew(SGameplayActionDataCreateDialog);
	return Dialog->ConfigureProperties(this);
}

UObject* UActionDataFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn, FName CallingContext)
{
	check(Class->IsChildOf(UGameplayActionData::StaticClass()));

	if ((ParentClass == NULL) || !ParentClass->IsChildOf(UGameplayActionData::StaticClass()))
	{
		FFormatNamedArguments Args;
		Args.Add( TEXT("ClassName"), (ParentClass != NULL) ? FText::FromString( ParentClass->GetName() ) : LOCTEXT("Null", "(null)") );
		FMessageDialog::Open( EAppMsgType::Ok, FText::Format( LOCTEXT("CannotCreateGameplayActionData", "Cannot create a Gameplay Action Data based on the class '{ClassName}'."), Args ) );
		return NULL;
	}
	else
	{
		return NewObject<UGameplayActionData>(InParent,ParentClass,Name,Flags|RF_Transactional,Context);
	}
}


UObject* UActionDataFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	return FactoryCreateNew(Class, InParent, Name, Flags, Context, Warn, NAME_None);
}

#undef LOCTEXT_NAMESPACE