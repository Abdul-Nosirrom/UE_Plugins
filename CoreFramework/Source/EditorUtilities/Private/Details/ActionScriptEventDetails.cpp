// Copyright 2023 CoC All rights reserved


#include "Details/ActionScriptEventDetails.h"

#include "DetailWidgetRow.h"
#include "ActionSystem/GameplayActionTypes.h"
#include "Widgets/SActionEventWidget.h"


TSharedRef<IPropertyTypeCustomization> FActionScriptEventDetails::MakeInstance()
{
	return MakeShareable(new FActionScriptEventDetails());
}

void FActionScriptEventDetails::CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow,
	IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	EventProperty = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FActionEventWrapper, Event));

	PropertyOptions.Empty();
	PropertyOptions.Add(MakeShareable(new FString("None")));

	UClass* TargetActionClass = nullptr;
	TArray<UObject*> OuterObjects;
	PropertyHandle->GetOuterObjects(OuterObjects);
	if (OuterObjects.Num() > 0) TargetActionClass = OuterObjects[0]->GetOuter()->GetClass();
	else TargetActionClass = PropertyHandle->GetOuterBaseClass()->GetOuter()->GetClass();
	const FString& FilterMetaStr = PropertyHandle->GetProperty()->GetMetaData(TEXT("FilterMetaTag"));
	TArray<FProperty*> PropertiesToAdd;
	FActionEventWrapper::GetAllEventProperties(TargetActionClass, PropertiesToAdd, FilterMetaStr);

	for (auto* Property : PropertiesToAdd)
	{
		PropertyOptions.Add(MakeShareable(new FString(FString::Printf(TEXT("%s.%s"), *Property->GetOwnerVariant().GetName(), *Property->GetName()))));
	}
	
	FProperty* PropertyValue = nullptr;
	if (EventProperty.IsValid())
	{
		FProperty* ObjPtr = nullptr;
		EventProperty->GetValue(ObjPtr);
		PropertyValue = ObjPtr;
	}

	HeaderRow.
	NameContent()
	[
		PropertyHandle->CreatePropertyNameWidget()
	]
	.ValueContent()
	.MinDesiredWidth(500)
	.MaxDesiredWidth(4096)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		//.FillWidth(1.0f)
		.HAlign(HAlign_Fill)
		.Padding(0.f, 0.f, 2.f, 0.f)
		[
			SNew(SActionEventWidget)
			.TargetClass(TargetActionClass)
			.OnEventChanged(this, &FActionScriptEventDetails::OnEventChanged)
			.DefaultProperty(PropertyValue)
			.FilterMetaData(FilterMetaStr)
		]
	];
}

void FActionScriptEventDetails::CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle,
	IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
}

TSharedPtr<FString> FActionScriptEventDetails::GetPropertyType() const
{
	if (EventProperty.IsValid())
	{
		FProperty *PropertyValue = nullptr;
		EventProperty->GetValue(PropertyValue);
		if (PropertyValue)
		{
			const FString FullString = PropertyValue->GetOwnerVariant().GetName() + TEXT(".") + PropertyValue->GetName();
			for (int32 i=0; i < PropertyOptions.Num(); ++i)
			{
				if (PropertyOptions[i].IsValid() && PropertyOptions[i].Get()->Equals(FullString))
				{
					return PropertyOptions[i];
				}
			}
		}
	}

	return PropertyOptions[0]; // This should always be none
}

void FActionScriptEventDetails::OnChangeProperty(TSharedPtr<FString> ItemSelected, ESelectInfo::Type SelectInfo)
{
	if (ItemSelected.IsValid() && EventProperty.IsValid())
	{		
		FString FullString = *ItemSelected.Get();
		FString ClassName;
		FString PropertyName;

		FullString.Split( TEXT("."), &ClassName, &PropertyName);

		UClass *FoundClass = UClass::TryFindTypeSlow<UClass>(ClassName);
		if (FoundClass)
		{
			FProperty *Property = FindFProperty<FProperty>(FoundClass, *PropertyName);
			if (Property)
			{
				EventProperty->SetValue(Property);
				
				return;
			}
		}

		UObject* nullObj = nullptr;
		EventProperty->SetValue(nullObj);
	}
}

void FActionScriptEventDetails::OnEventChanged(FProperty* SelectedEVent)
{
	if (EventProperty.IsValid())
	{
		EventProperty->SetValue(SelectedEVent);
	}
}
