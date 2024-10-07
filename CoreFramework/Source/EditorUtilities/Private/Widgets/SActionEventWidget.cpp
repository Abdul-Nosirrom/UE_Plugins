// Copyright 2023 CoC All rights reserved


#include "Widgets/SActionEventWidget.h"

#include "SlateOptMacros.h"
#include "ActionSystem/GameplayAction.h"
#include "ActionSystem/GameplayActionTypes.h"
#include "Misc/TextFilter.h"
#include "Widgets/Input/SSearchBox.h"

DECLARE_DELEGATE_OneParam(FOnActionEventPicked, FProperty*);


BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

/*--------------------------------------------------------------------------------------------------------------
* Internal List Dropdown Widget 
*--------------------------------------------------------------------------------------------------------------*/

struct FActionEventViewerNode
{
public:
	FActionEventViewerNode(FProperty* InEvent, FString InEventName)
	{
		Event = InEvent;
		EventName = MakeShareable(new FString(InEventName));
	}

	/** The displayed name for this node. */
	TSharedPtr<FString> EventName;

	FProperty* Event;
};

/** The item used for visualizing the attribute in the list. */
class SActionEventItem : public SComboRow< TSharedPtr<FActionEventViewerNode> >
{
public:

	SLATE_BEGIN_ARGS(SActionEventItem)
		: _TextColor(FLinearColor(1.0f, 1.0f, 1.0f, 1.0f))
	{}

	/** The color text this item will use. */
	SLATE_ARGUMENT(FSlateColor, TextColor)
	/** The node this item is associated with. */
	SLATE_ARGUMENT(TSharedPtr<FActionEventViewerNode>, AssociatedNode)

	SLATE_END_ARGS()

	/**
	* Construct the widget
	*
	* @param InArgs   A declaration from which to construct the widget
	*/
	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView)
	{
		AssociatedNode = InArgs._AssociatedNode;

		this->ChildSlot
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					.Padding(0.0f, 3.0f, 6.0f, 3.0f)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(FText::FromString(*AssociatedNode->EventName.Get()))
						.ColorAndOpacity(this, &SActionEventItem::GetTextColor)
						.IsEnabled(true)
					]
			];

		TextColor = InArgs._TextColor;

		STableRow< TSharedPtr<FActionEventViewerNode> >::ConstructInternal(
			STableRow::FArguments()
			.ShowSelection(true),
			InOwnerTableView
			);
	}

	/** Returns the text color for the item based on if it is selected or not. */
	FSlateColor GetTextColor() const
	{
		const TSharedPtr< ITypedTableView< TSharedPtr<FActionEventViewerNode> > > OwnerWidget = OwnerTablePtr.Pin();
		const TSharedPtr<FActionEventViewerNode>* MyItem = OwnerWidget->Private_ItemFromWidget(this);
		const bool bIsSelected = OwnerWidget->Private_IsItemSelected(*MyItem);

		if (bIsSelected)
		{
			return FSlateColor::UseForeground();
		}

		return TextColor;
	}

private:

	/** The text color for this item. */
	FSlateColor TextColor;

	/** The Attribute Viewer Node this item is associated with. */
	TSharedPtr< FActionEventViewerNode > AssociatedNode;
};

class SActionEventListWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SActionEventListWidget)
	{
	}

	SLATE_ARGUMENT(FString, FilterMetaData)
	SLATE_ARGUMENT(UClass*, TargetClass)
	SLATE_ARGUMENT(FOnActionEventPicked, OnActionEventPickedDelegate)

	SLATE_END_ARGS()

	/**
	* Construct the widget
	*
	* @param	InArgs			A declaration from which to construct the widget
	*/
	void Construct(const FArguments& InArgs);

	virtual ~SActionEventListWidget();

private:
	
	/** Creates the row widget when called by Slate when an item appears on the list. */
	TSharedRef< ITableRow > OnGenerateRowForEventViewer(TSharedPtr<FActionEventViewerNode> Item, const TSharedRef< STableViewBase >& OwnerTable);

	/** Called by Slate when an item is selected from the tree/list. */
	void OnEventSelectionChanged(TSharedPtr<FActionEventViewerNode> Item, ESelectInfo::Type SelectInfo);

	/** Updates the list of items in the dropdown menu */
	TSharedPtr<FActionEventViewerNode> UpdatePropertyOptions();

	/** Delegate to be called when an attribute is picked from the list */
	FOnActionEventPicked OnActionEventPicked;
	
	/** Class to look for events on */
	UClass* TargetClass;

	/** Holds the Slate List widget which holds the attributes for the Attribute Viewer. */
	TSharedPtr<SListView<TSharedPtr< FActionEventViewerNode > >> EventList;

	/** Array of items that can be selected in the dropdown menu */
	TArray<TSharedPtr<FActionEventViewerNode>> PropertyOptions;
	
	/** Filter for meta data */
	FString FilterMetaData;
};

SActionEventListWidget::~SActionEventListWidget()
{
	if (OnActionEventPicked.IsBound())
	{
		OnActionEventPicked.Unbind();
	}
}

void SActionEventListWidget::Construct(const FArguments& InArgs)
{
	
	struct Local
	{
		static void AttributeToStringArray(const FProperty& Property, OUT TArray< FString >& StringArray)
		{
			StringArray.Add(FString::Printf(TEXT("%s"), *Property.GetName()));
		}
	};

	FilterMetaData = InArgs._FilterMetaData;
	TargetClass = InArgs._TargetClass;
	OnActionEventPicked = InArgs._OnActionEventPickedDelegate;
	
	UpdatePropertyOptions();

	TSharedPtr< SWidget > ClassViewerContent;

	SAssignNew(ClassViewerContent, SVerticalBox)
	
	+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		[
			SAssignNew(EventList, SListView<TSharedPtr< FActionEventViewerNode > >)
			.Visibility(EVisibility::Visible)
			.SelectionMode(ESelectionMode::Single)
			.ListItemsSource(&PropertyOptions)

 			// Generates the actual widget for a tree item
			.OnGenerateRow(this, &SActionEventListWidget::OnGenerateRowForEventViewer)

 			// Find out when the user selects something in the tree
			.OnSelectionChanged(this, &SActionEventListWidget::OnEventSelectionChanged)
		];


	ChildSlot
		[
			ClassViewerContent.ToSharedRef()
		];
}

TSharedRef< ITableRow > SActionEventListWidget::OnGenerateRowForEventViewer(TSharedPtr<FActionEventViewerNode> Item, const TSharedRef< STableViewBase >& OwnerTable)
{
	TSharedRef< SActionEventItem > ReturnRow = SNew(SActionEventItem, OwnerTable)
		.TextColor(FLinearColor(1.0f, 1.0f, 1.0f, 1.f))
		.AssociatedNode(Item);

	return ReturnRow;
}

TSharedPtr<FActionEventViewerNode> SActionEventListWidget::UpdatePropertyOptions()
{
	PropertyOptions.Empty();
	TSharedPtr<FActionEventViewerNode> InitiallySelected = MakeShareable(new FActionEventViewerNode(nullptr, "None"));

	PropertyOptions.Add(InitiallySelected);

	// Gather all UAttribute classes
	TArray<FProperty*> EventProps;
	FActionEventWrapper::GetAllEventProperties(TargetClass, EventProps, FilterMetaData);

	for (auto EventProp : EventProps)
	{
		TSharedPtr<FActionEventViewerNode> SelectableProperty = MakeShareable(new FActionEventViewerNode(EventProp, FString::Printf(TEXT("%s"), *EventProp->GetName())));
		PropertyOptions.Add(SelectableProperty);
	}

	return InitiallySelected;
}

void SActionEventListWidget::OnEventSelectionChanged(TSharedPtr<FActionEventViewerNode> Item, ESelectInfo::Type SelectInfo)
{
	OnActionEventPicked.ExecuteIfBound(Item->Event);
}


/*--------------------------------------------------------------------------------------------------------------
* Main Widget
*--------------------------------------------------------------------------------------------------------------*/


void SActionEventWidget::Construct(const FArguments& InArgs)
{
	FilterMetaData = InArgs._FilterMetaData;
	OnAttributeChanged = InArgs._OnEventChanged;
	TargetClass = InArgs._TargetClass;
	SelectedProperty = InArgs._DefaultProperty;

	// set up the combo button
	SAssignNew(ComboButton, SComboButton)
		.OnGetMenuContent(this, &SActionEventWidget::GenerateEventPicker)
		.ContentPadding(FMargin(2.0f, 2.0f))
		.ToolTipText(this, &SActionEventWidget::GetSelectedValueAsString)
		.ButtonContent()
		[
			SNew(STextBlock)
			.Text(this, &SActionEventWidget::GetSelectedValueAsString)
		];

	ChildSlot
	[
		ComboButton.ToSharedRef()
	];
}

void SActionEventWidget::OnEventPicked(FProperty* InProperty)
{
	if (OnAttributeChanged.IsBound())
	{
		OnAttributeChanged.Execute(InProperty);
	}

	// Update the selected item for displaying
	SelectedProperty = InProperty;

	// close the list
	ComboButton->SetIsOpen(false);
}

TSharedRef<SWidget> SActionEventWidget::GenerateEventPicker()
{
	FOnEventSelectionChanged OnPicked(FOnEventSelectionChanged::CreateRaw(this, &SActionEventWidget::OnEventPicked));

	return SNew(SBox)
		.WidthOverride(280)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.MaxHeight(500)
			[
				SNew(SActionEventListWidget)
				.TargetClass(TargetClass)
				.OnActionEventPickedDelegate(OnPicked)
				.FilterMetaData(FilterMetaData)
			]
		];
}

FText SActionEventWidget::GetSelectedValueAsString() const
{
	if (SelectedProperty)
	{
		FString PropertyString = FString::Printf(TEXT("%s"), *SelectedProperty->GetName());
		return FText::FromString(PropertyString);
	}

	return FText::FromString(TEXT("None"));
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION
