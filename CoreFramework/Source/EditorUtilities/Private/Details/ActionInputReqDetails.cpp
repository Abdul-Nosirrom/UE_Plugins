// Copyright 2023 CoC All rights reserved


#include "Details/ActionInputReqDetails.h"

#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "PropertyCustomizationHelpers.h"
#include "SGameplayTagCombo.h"
#include "SGameplayTagPicker.h"
#include "SGameplayTagWidget.h"
#include "ActionSystem/Triggers/ActionTrigger_InputRequirement.h"
#include "Editor/PropertyEditor/Private/EditConditionParser.h"
#include "Widgets/SBoolPropertyButton.h"

#define InputSlot SHorizontalBox::Slot()

TSharedRef<IPropertyTypeCustomization> FActionInputReqDetails::MakeInstance()
{
	return MakeShareable(new FActionInputReqDetails);
}

void FActionInputReqDetails::CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow,
	IPropertyTypeCustomizationUtils& CustomizationUtils)
{
}

void FActionInputReqDetails::CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle,
	IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	// Get all properties
	auto InputTypeProp = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FActionInputRequirement, InputType));

	auto TriggerEventProp = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FActionInputRequirement, TriggerEvent));
	auto ElapsedTimeProp = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FActionInputRequirement, RequiredElapsedTime));
	auto SequenceOrderProp = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FActionInputRequirement, SequenceOrder));
	auto FirstInputHoldProp = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FActionInputRequirement, bFirstInputIsHold));
	auto ConsiderOrderProp = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FActionInputRequirement, bConsiderOrder));

	auto ButtonActionProp = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FActionInputRequirement, ButtonInput));
	auto SecondButtonActionProp = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FActionInputRequirement, SecondButtonInput));
	auto DirectionalActionProp = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FActionInputRequirement, DirectionalInput));
	
	FText RowSearchName = FText::FromString("Input Requirements");
	
	ChildBuilder.AddCustomRow(RowSearchName).WholeRowContent().HAlign(HAlign_Fill)
	[
		SNew(SHorizontalBox) + InputSlot
		[
			SNew(SProperty, InputTypeProp).ShouldDisplayName(false)
		]
		+ InputSlot
		[
			TriggerEventProp->CreatePropertyValueWidget()
		]
		+ InputSlot
		[
			SNew(SProperty, ElapsedTimeProp)
		]
		+ InputSlot
		[
			SNew(SProperty, SequenceOrderProp).ShouldDisplayName(false)
		]
		+ InputSlot.VAlign(VAlign_Center)
		[
			SNew(SProperty, FirstInputHoldProp)
		]
		+ InputSlot.VAlign(VAlign_Center)
		[
			SNew(SProperty, ConsiderOrderProp)
		]
		+ InputSlot
		[
			SNew(SGameplayTagCombo).PropertyHandle(ButtonActionProp).Filter("Input.Button")
		]
		+ InputSlot
		[
			SNew(SGameplayTagCombo).PropertyHandle(SecondButtonActionProp).Filter("Input.Button")
		]
		+ InputSlot
		[
			SNew(SGameplayTagCombo).PropertyHandle(DirectionalActionProp).Filter("Input.Directional")
		]
	];
}
