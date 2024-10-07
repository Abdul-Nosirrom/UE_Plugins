// Copyright 2023 CoC All rights reserved

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"


class SEntityAttributeWidget : public SCompoundWidget
{
public:

	/** Called when a tag status is changed */
	DECLARE_DELEGATE_OneParam(FOnEntityAttributeChanged, FProperty*)

	SLATE_BEGIN_ARGS(SEntityAttributeWidget)
	: _FilterMetaData()
	, _DefaultProperty(nullptr)
	{}
	SLATE_ARGUMENT(FString, FilterMetaData)
	SLATE_ARGUMENT(FProperty*, DefaultProperty)
	SLATE_EVENT(FOnEntityAttributeChanged, OnAttributeChanged) // Called when a tag status changes
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);	

private:

	TSharedRef<SWidget> GenerateAttributePicker();

	FText GetSelectedValueAsString() const;

	/** Handles updates when the selected attribute changes */
	void OnAttributePicked(FProperty* InProperty);

	/** Delegate to call when the selected attribute changes */
	FOnEntityAttributeChanged OnAttributeChanged;

	/** The search string being used to filter the attributes */
	FString FilterMetaData;

	/** The currently selected attribute */
	FProperty* SelectedProperty;

	/** Used to display an attribute picker. */
	TSharedPtr<class SComboButton> ComboButton;
};
