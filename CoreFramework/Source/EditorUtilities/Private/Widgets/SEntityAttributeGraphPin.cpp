// Copyright 2023 CoC All rights reserved


#include "Widgets/SEntityAttributeGraphPin.h"

#include "SlateOptMacros.h"
#include "AttributeSystem/EntityAttributeSet.h"
#include "Widgets/SEntityAttributeWidget.h"

#define LOCTEXT_NAMESPACE "K2Node"

void SEntityAttributeGraphPin::Construct( const FArguments& InArgs, UEdGraphPin* InGraphPinObj )
{
	SGraphPin::Construct( SGraphPin::FArguments(), InGraphPinObj );
	LastSelectedProperty = nullptr;

}

TSharedRef<SWidget>	SEntityAttributeGraphPin::GetDefaultValueWidget()
{	
	FEntityAttribute DefaultAttribute;

	// Parse out current default value
	FString DefaultString = GraphPinObj->GetDefaultAsString();
	if (!DefaultString.IsEmpty())
	{
		UScriptStruct* PinLiteralStructType = FEntityAttribute::StaticStruct();
		PinLiteralStructType->ImportText(*DefaultString, &DefaultAttribute, nullptr, EPropertyPortFlags::PPF_SerializedAsImportText, GError, PinLiteralStructType->GetName(), true);
	}

	// Create widget
	return SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SEntityAttributeWidget)
			.OnAttributeChanged(this, &SEntityAttributeGraphPin::OnAttributeChanged)
			.DefaultProperty(DefaultAttribute.GetUProperty())
			.Visibility(this, &SGraphPin::GetDefaultValueVisibility)
			.IsEnabled(this, &SEntityAttributeGraphPin::GetDefaultValueIsEnabled)
		];
}

void SEntityAttributeGraphPin::OnAttributeChanged(FProperty* SelectedAttribute)
{
	FString FinalValue;
	FEntityAttribute NewAttributeStruct;
	NewAttributeStruct.SetUProperty(SelectedAttribute);

	FEntityAttribute::StaticStruct()->ExportText(FinalValue, &NewAttributeStruct, &NewAttributeStruct, nullptr, EPropertyPortFlags::PPF_SerializedAsImportText, nullptr);

	if (FinalValue != GraphPinObj->GetDefaultAsString())
	{
		const FScopedTransaction Transaction(NSLOCTEXT("GraphEditor", "ChangePinValue", "Change Pin Value"));
		GraphPinObj->Modify();
		GraphPinObj->GetSchema()->TrySetDefaultValue(*GraphPinObj, FinalValue);
	}

	LastSelectedProperty = SelectedAttribute;
}

#undef LOCTEXT_NAMESPACE


