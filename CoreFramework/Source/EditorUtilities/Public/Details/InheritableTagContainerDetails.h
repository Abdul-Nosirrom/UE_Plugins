// Copyright 2023 CoC All rights reserved

#pragma once

#include "CoreMinimal.h"
#include "IPropertyTypeCustomization.h"

class FInheritableTagContainerDetails : public IPropertyTypeCustomization
{
public:

	static TSharedRef<IPropertyTypeCustomization> MakeInstance();
	
	/** IPropertyTypeCustomization interface */
	virtual void CustomizeHeader( TSharedRef<class IPropertyHandle> StructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils ) override;
	virtual void CustomizeChildren( TSharedRef<class IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils ) override;              

private:
	void OnTagsChanged();

	void UpdateSelfFromSuperTags();
	void UpdateChildrenFromSelfTags();

	TSharedPtr<IPropertyHandle> CombinedTagContainerPropertyHandle;
	TSharedPtr<IPropertyHandle> AddedTagContainerPropertyHandle;
	TSharedPtr<IPropertyHandle> RemovedTagContainerPropertyHandle;

};
