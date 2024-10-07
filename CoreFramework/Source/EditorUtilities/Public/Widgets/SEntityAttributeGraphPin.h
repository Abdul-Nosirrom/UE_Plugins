// Copyright 2023 CoC All rights reserved

#pragma once

#include "CoreMinimal.h"
#include "SGraphPin.h"
#include "Widgets/SCompoundWidget.h"

/**
 * 
 */
class EDITORUTILITIES_API SEntityAttributeGraphPin : public SGraphPin
{
public:
	SLATE_BEGIN_ARGS(SEntityAttributeGraphPin) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UEdGraphPin* InGraphPinObj);

	//~ Begin SGraphPin Interface
	virtual TSharedRef<SWidget>	GetDefaultValueWidget() override;
	//~ End SGraphPin Interface

	void OnAttributeChanged(FProperty* SelectedAttribute);

	FProperty* LastSelectedProperty;

private:
	bool GetDefaultValueIsEnabled() const
	{
		return !GraphPinObj->bDefaultValueIsReadOnly;
	}
};

