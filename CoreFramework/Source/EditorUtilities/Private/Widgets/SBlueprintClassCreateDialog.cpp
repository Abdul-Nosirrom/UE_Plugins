// Copyright 2023 CoC All rights reserved


#include "Widgets/SBlueprintClassCreateDialog.h"

#include "SlateOptMacros.h"
#include "Factories/BlueprintFactory.h"
#include "Widgets/Layout/SUniformGridPanel.h"

#define LOCTEXT_NAMESPACE "SBlueprintClassCreateDialog"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

void SBlueprintClassCreateDialog::Construct(const FArguments& InArgs)
{
	CachedArgs = InArgs;
	bOkClicked = false;
	ParentClass = CachedArgs._BaseClass.Get();

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
								.OnClicked(this, &SBlueprintClassCreateDialog::OkClicked)
								.Text(LOCTEXT("CreateBlueprintOk", "OK"))
							]
							+ SUniformGridPanel::Slot(1, 0)
								[
									SNew(SButton)
									.HAlign(HAlign_Center)
									.ContentPadding(FAppStyle::GetMargin("StandardDialog.ContentPadding"))
									.OnClicked(this, &SBlueprintClassCreateDialog::CancelClicked)
									.Text(LOCTEXT("CreateBlueprintCancel", "Cancel"))
								]
						]
				]
			]
		];

	MakeParentClassPicker();
}

bool SBlueprintClassCreateDialog::ConfigureProperties(TWeakObjectPtr<UBlueprintFactory> InBlueprintFactor)
{
	BlueprintFactory = InBlueprintFactor;
	const FText WindowTitle = FText::FromString(FString::Printf(TEXT("Create %s"), *CachedArgs._BaseClass.Get()->GetName()));

	TSharedRef<SWindow> Window = SNew(SWindow)
		.Title(WindowTitle)
		.ClientSize(FVector2D(400, 700))
		.SupportsMinimize(false).SupportsMaximize(false)
		[
			AsShared()
		];
		
	PickerWindow = Window;

	GEditor->EditorAddModalWindow(Window);
	BlueprintFactory.Reset();

	return bOkClicked;
}

void SBlueprintClassCreateDialog::MakeParentClassPicker()
{
	// Load the classviewer module to display a class picker
	FClassViewerModule& ClassViewerModule = FModuleManager::LoadModuleChecked<FClassViewerModule>("ClassViewer");

	// Fill in options
	FClassViewerInitializationOptions Options;
	Options.Mode = EClassViewerMode::ClassPicker;

	// Only allow parenting to base blueprints.
	Options.bIsBlueprintBaseOnly = true;

	TSharedPtr<FBlueprintParentFilter> Filter = MakeShareable(new FBlueprintParentFilter);

	// All child child classes of target class are valid.
	Filter->AllowedChildrenOfClasses.Add(CachedArgs._BaseClass.Get());
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
			ClassViewerModule.CreateClassViewer(Options, FOnClassPicked::CreateSP(this, &SBlueprintClassCreateDialog::OnClassPicked))
		];
}

void SBlueprintClassCreateDialog::OnClassPicked(UClass* ChosenClass)
{
	ParentClass = ChosenClass;
}

FReply SBlueprintClassCreateDialog::OkClicked()
{
	if (BlueprintFactory.IsValid())
	{
		BlueprintFactory->BlueprintType = BPTYPE_Normal;
		BlueprintFactory->ParentClass = ParentClass.Get();
	}

	CloseDialog(true);

	return FReply::Handled();
}

void SBlueprintClassCreateDialog::CloseDialog(bool bWasPicked)
{
	bOkClicked = bWasPicked;
	if (PickerWindow.IsValid())
	{
		PickerWindow.Pin()->RequestDestroyWindow();
	}
}

FReply SBlueprintClassCreateDialog::CancelClicked()
{
	CloseDialog();
	return FReply::Handled();
}

FReply SBlueprintClassCreateDialog::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		CloseDialog();
		return FReply::Handled();
	}
	return SWidget::OnKeyDown(MyGeometry, InKeyEvent);
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION

#undef LOCTEXT_NAMESPACE