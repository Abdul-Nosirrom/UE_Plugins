// Copyright 2023 CoC All rights reserved

#pragma once

#include "CoreMinimal.h"
#include "PropertyCustomizationHelpers.h"
#include "Widgets/SCompoundWidget.h"

/**
 * 
 */
class EDITORUTILITIES_API SBoolPropertyButton : public SButton
{
public:
	DECLARE_DELEGATE(FOnPropertyBoolChanged);
	
	SLATE_BEGIN_ARGS(SBoolPropertyButton) {}
		SLATE_ARGUMENT(FText, Text)
	SLATE_END_ARGS()

	/** Constructs this widget with InArgs */
	void Construct(const FArguments& InArgs, TSharedPtr<IPropertyHandle> InPropertyHandle);


	TSharedPtr<IPropertyHandle> PropertyHandle;
	bool bCurrentVal = false;
};
