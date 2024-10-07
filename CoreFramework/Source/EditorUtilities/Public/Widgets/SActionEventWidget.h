// Copyright 2023 CoC All rights reserved

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"


class SActionEventWidget : public SCompoundWidget
{
public:

	/* Called when an event selection changes*/
	DECLARE_DELEGATE_OneParam(FOnEventSelectionChanged, FProperty*)
	
	SLATE_BEGIN_ARGS(SActionEventWidget)
		: _FilterMetaData(), _DefaultProperty(nullptr), _TargetClass(nullptr)
	{}
		SLATE_ARGUMENT(FString, FilterMetaData)
		SLATE_ARGUMENT(FProperty*, DefaultProperty)
		SLATE_ARGUMENT(UClass*, TargetClass)
		SLATE_EVENT(FOnEventSelectionChanged, OnEventChanged)
	SLATE_END_ARGS()

	/** Constructs this widget with InArgs */
	void Construct(const FArguments& InArgs);

private:

	TSharedRef<SWidget> GenerateEventPicker();

	FText GetSelectedValueAsString() const;

	/** Handles updates when the selected attribute changes */
	void OnEventPicked(FProperty* InProperty);

	/** Delegate to call when the selected attribute changes */
	FOnEventSelectionChanged OnAttributeChanged;

	/** The search string being used to filter the attributes */
	FString FilterMetaData;

	/** The currently selected attribute */
	FProperty* SelectedProperty;

	/** Class which we look for events on */
	UClass* TargetClass;

	/** Used to display an attribute picker. */
	TSharedPtr<class SComboButton> ComboButton;
};
