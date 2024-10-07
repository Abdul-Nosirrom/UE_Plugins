// Copyright 2023 CoC All rights reserved

#pragma once

#include "CoreMinimal.h"
#include "IDetailCustomization.h"


class FEntityAttributePropertyDetails : public IPropertyTypeCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();

	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils) override;
	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils) override;

private:

	// the attribute property
	TSharedPtr<IPropertyHandle> MyProperty;
	// the owner property
	TSharedPtr<IPropertyHandle> OwnerProperty;
	// the name property
	TSharedPtr<IPropertyHandle> NameProperty;

	TArray<TSharedPtr<FString>> PropertyOptions;

	TSharedPtr<FString>	GetPropertyType() const;

	void OnChangeProperty(TSharedPtr<FString> ItemSelected, ESelectInfo::Type SelectInfo);
	void OnAttributeChanged(FProperty* SelectedAttribute);
};
