// Copyright 2023 CoC All rights reserved


#include "Details/EntityAttributeDetails.h"

#include "DetailWidgetRow.h"
#include "AttributeSystem/EntityAttributeSet.h"
#include "Widgets/SEntityAttributeWidget.h"

#define LOCTEXT_NAMESPACE "AttributeDetailsCustomization"

TSharedRef<IPropertyTypeCustomization> FEntityAttributePropertyDetails::MakeInstance()
{
	return MakeShareable(new FEntityAttributePropertyDetails());
}

void FEntityAttributePropertyDetails::CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle,
	FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	MyProperty = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FEntityAttribute,Attribute));
	OwnerProperty = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FEntityAttribute,AttributeOwner));
	NameProperty = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FEntityAttribute,AttributeName));

	PropertyOptions.Empty();
	PropertyOptions.Add(MakeShareable(new FString("None")));

	const FString& FilterMetaStr = PropertyHandle->GetProperty()->GetMetaData(TEXT("FilterMetaTag"));

	TArray<FProperty*> PropertiesToAdd;
	FEntityAttribute::GetAllAttributeProperties(PropertiesToAdd, FilterMetaStr);

	for ( auto* Property : PropertiesToAdd )
	{
		PropertyOptions.Add(MakeShareable(new FString(FString::Printf(TEXT("%s.%s"), *Property->GetOwnerVariant().GetName(), *Property->GetName()))));
	}

	FProperty* PropertyValue = nullptr;
	if (MyProperty.IsValid())
	{
		FProperty *ObjPtr = nullptr;
		MyProperty->GetValue(ObjPtr);
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
				SNew(SEntityAttributeWidget)
				.OnAttributeChanged(this, &FEntityAttributePropertyDetails::OnAttributeChanged)
				.DefaultProperty(PropertyValue)
				.FilterMetaData(FilterMetaStr)
			]
		];
}

void FEntityAttributePropertyDetails::CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle,
	IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils)
{}

TSharedPtr<FString> FEntityAttributePropertyDetails::GetPropertyType() const
{
	if (MyProperty.IsValid())
	{
		FProperty *PropertyValue = nullptr;
		MyProperty->GetValue(PropertyValue);
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

void FEntityAttributePropertyDetails::OnChangeProperty(TSharedPtr<FString> ItemSelected, ESelectInfo::Type SelectInfo)
{
	if (ItemSelected.IsValid() && MyProperty.IsValid())
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
				MyProperty->SetValue(Property);
				
				return;
			}
		}

		UObject* nullObj = nullptr;
		MyProperty->SetValue(nullObj);
	}
}

void FEntityAttributePropertyDetails::OnAttributeChanged(FProperty* SelectedAttribute)
{
	if (MyProperty.IsValid())
	{
		MyProperty->SetValue(SelectedAttribute);

		// When we set the attribute we should also set the owner and name info
		if (OwnerProperty.IsValid())
		{
			OwnerProperty->SetValue(SelectedAttribute ? SelectedAttribute->GetOwnerStruct() : nullptr);
		}

		if (NameProperty.IsValid())
		{
			FString AttributeName;
			if (SelectedAttribute)
			{
				SelectedAttribute->GetName(AttributeName);
			}
			NameProperty->SetValue(AttributeName);
		}
	}
}

#undef LOCTEXT_NAMESPACE