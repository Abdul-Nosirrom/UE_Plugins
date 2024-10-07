// Copyright 2023 CoC All rights reserved


#include "Details/ActionSetEntryDetails.h"

#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "PropertyCustomizationHelpers.h"
#include "ActionSystem/CharacterActionSet.h"
#include "ActionSystem/GameplayAction.h"
#include "ActionSystem/Triggers/ActionTrigger_InputRequirement.h"

#define LOCTEXT_NAMESPACE "ActionSetEntryDetails"

TSharedRef<IPropertyTypeCustomization> FActionSetEntryDetails::MakeInstance()
{
	return MakeShareable(new FActionSetEntryDetails());
}

void FActionSetEntryDetails::CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle,
	FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	auto EnableProp = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FActionSetEntry, bEnabled));
	auto ActionProp = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FActionSetEntry, Action));

	FText EntryTitle = LOCTEXT("NullEntry", "Null");
	
	if (ActionProp->GetPropertyClass())
	{
		EntryTitle = ActionProp->GetPropertyClass()->GetDisplayNameText();
	}

	// Get input requirement handle, search for it as we can't access it like the above
	uint32 NumElements;
	ActionProp->GetNumChildren(NumElements);
	TSharedPtr<IPropertyHandle> InputReqHandle = nullptr;
	FText ActionName = FText::FromString("None");
	for (uint32 Idx = 0; Idx < NumElements; Idx++)
	{
		auto ElemHandle = ActionProp->GetChildHandle(Idx);
		if (ElemHandle->GetProperty()->GetFName() == GET_MEMBER_NAME_CHECKED(UGameplayActionData, Action))
		{
			// Found the action itself, now repeat to search for the trigger
			uint32 NumActionElements;
			ElemHandle->GetNumChildren(NumActionElements);
			for (uint32 AIdx = 0; AIdx < NumActionElements; AIdx++)
			{
				auto AElemHandle = ElemHandle->GetChildHandle(AIdx);
				if (AElemHandle->GetProperty()->GetFName() == GET_MEMBER_NAME_CHECKED(UGameplayAction, ActionTrigger))
				{
					// Found it!
					InputReqHandle = AElemHandle;
				}
			}
		}
	}

	// We've got the handle for the trigger, but still no guarantee that it's actually an input requirement field
	bool bFoundInput = false;
	if (InputReqHandle)
	{
		UObject* InputReqQuery;
		InputReqHandle->GetValue(InputReqQuery);
		if (InputReqQuery && Cast<UActionTrigger_InputRequirement>(InputReqQuery))
		{
			bFoundInput = true;
		}
	}
	
	HeaderRow.NameContent().MaxDesiredWidth(512)
	[
		SNew(SHorizontalBox) + SHorizontalBox::Slot().HAlign(HAlign_Left)
		[
			SNew(SProperty, EnableProp).ShouldDisplayName(false)
		]
	]
	.ValueContent().MinDesiredWidth(500).MaxDesiredWidth(32096)
	[
		SNew(SHorizontalBox) + SHorizontalBox::Slot().HAlign(HAlign_Fill)
		[
			SNew(SProperty, ActionProp)
		]
	];
}

void FActionSetEntryDetails::CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle,
	IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	return;
	auto EnableProp = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FActionSetEntry, bEnabled));
	auto ActionProp = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FActionSetEntry, Action));
	
	UObject* OutActionData;
	TSharedPtr<IPropertyHandle> InputProperty = nullptr;
	if (ActionProp->GetValue(OutActionData) == FPropertyAccess::Result::Success)
	{
		if (OutActionData)
		{
			UGameplayAction* ActionInstance = Cast<UGameplayActionData>(OutActionData)->Action;
			if (ActionInstance && Cast<UActionTrigger_InputRequirement>(ActionInstance->ActionTrigger))
			{
				auto Trigger = Cast<UActionTrigger_InputRequirement>(ActionInstance->ActionTrigger);
				auto TriggerProp = GET_MEMBER_NAME_CHECKED(UActionTrigger_InputRequirement, InputRequirement);
			}
		}
		
		if (auto TestProp = ActionProp->GetChildHandle(GET_MEMBER_NAME_CHECKED(UGameplayActionData, BlockActionsWithTag)))
		{
			OutActionData = nullptr;
		}
		if (auto ActionInstanceProp = ActionProp->GetChildHandle(GET_MEMBER_NAME_CHECKED(UGameplayActionData, Action)))
		{
			if (auto TriggerProp = ActionInstanceProp->GetChildHandle(GET_MEMBER_NAME_CHECKED(UGameplayAction, ActionTrigger)))
			{
				InputProperty = TriggerProp->GetChildHandle(GET_MEMBER_NAME_CHECKED(UActionTrigger_InputRequirement, InputRequirement));
			}
		}
	}

	bool bFailed = !InputProperty.IsValid();

	if (bFailed)
	{
		ChildBuilder.AddCustomRow(LOCTEXT("Action", "Action"))
		[
			SNew(SHorizontalBox) + SHorizontalBox::Slot().HAlign(HAlign_Left).FillWidth(0.2f)
			[
				SNew(SProperty, EnableProp)
			]
			+ SHorizontalBox::Slot().HAlign(HAlign_Fill)
			[
				SNew(SProperty, ActionProp)
			]
		];
	}
	else
	{
		ChildBuilder.AddCustomRow(LOCTEXT("Action", "Action"))
		[
			SNew(SHorizontalBox) + SHorizontalBox::Slot().HAlign(HAlign_Left)
			[
				SNew(SProperty, EnableProp)
			]
			+ SHorizontalBox::Slot().HAlign(HAlign_Fill)
			[
				SNew(SProperty, InputProperty)
			]
			+ SHorizontalBox::Slot().HAlign(HAlign_Fill)
			[
				SNew(SProperty, ActionProp)
			]
		];
	}
}

#undef LOCTEXT_NAMESPACE