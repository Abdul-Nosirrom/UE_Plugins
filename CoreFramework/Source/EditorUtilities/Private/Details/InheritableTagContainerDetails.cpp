// Copyright 2023 CoC All rights reserved


#include "Details/InheritableTagContainerDetails.h"

#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "Data/GameplayTagData.h"

#define LOCTEXT_NAMESPACE "InheritableTagContainerDetailsCustomization"


TSharedRef<IPropertyTypeCustomization> FInheritableTagContainerDetails::MakeInstance()
{
	return MakeShareable(new FInheritableTagContainerDetails());
}

void FInheritableTagContainerDetails::CustomizeHeader(TSharedRef<IPropertyHandle> StructPropertyHandle,
	FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	HeaderRow.NameContent()
		[
			StructPropertyHandle->CreatePropertyNameWidget()
		];
}

void FInheritableTagContainerDetails::CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle,
	IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	uint32 NumChildren;
	StructPropertyHandle->GetNumChildren(NumChildren);

	CombinedTagContainerPropertyHandle = StructPropertyHandle->GetChildHandle(TEXT("CombinedTags"));
	AddedTagContainerPropertyHandle = StructPropertyHandle->GetChildHandle(TEXT("Added"));
	RemovedTagContainerPropertyHandle = StructPropertyHandle->GetChildHandle(TEXT("Removed"));

	FSimpleDelegate OnTagValueChangedDelegate = FSimpleDelegate::CreateSP(this, &FInheritableTagContainerDetails::OnTagsChanged);
	AddedTagContainerPropertyHandle->SetOnPropertyValueChanged(OnTagValueChangedDelegate);
	RemovedTagContainerPropertyHandle->SetOnPropertyValueChanged(OnTagValueChangedDelegate);

	StructBuilder.AddProperty(CombinedTagContainerPropertyHandle.ToSharedRef());
	StructBuilder.AddProperty(AddedTagContainerPropertyHandle.ToSharedRef());
	StructBuilder.AddProperty(RemovedTagContainerPropertyHandle.ToSharedRef());
}

void FInheritableTagContainerDetails::OnTagsChanged()
{
	CombinedTagContainerPropertyHandle->NotifyPreChange();
	
	UpdateSelfFromSuperTags();

	UpdateChildrenFromSelfTags();
	
	CombinedTagContainerPropertyHandle->NotifyPostChange(EPropertyChangeType::ValueSet);
}

void FInheritableTagContainerDetails::UpdateSelfFromSuperTags()
{
	TSharedPtr<IPropertyHandle> StructPropertyHandle = CombinedTagContainerPropertyHandle->GetParentHandle();

	TArray<void*> RawData;
	StructPropertyHandle->AccessRawData(RawData);

	UClass* SuperClass = StructPropertyHandle->GetOuterBaseClass()->GetSuperClass();

	FProperty* ThisProp = StructPropertyHandle->GetProperty();
	FProperty* SuperProperty = nullptr;
	
	// Iterate through all properties of the parent class this property is on to find this property on the parent class
	for (TFieldIterator<FProperty>PropertyIt(SuperClass, EFieldIteratorFlags::IncludeSuper); PropertyIt; ++PropertyIt)
	{
		SuperProperty = *PropertyIt;
		if (SuperProperty->GetNameCPP().Equals(ThisProp->GetNameCPP()))
		{
			break;
		}
	}

	FStructProperty* SuperStructProp = CastField<FStructProperty>(SuperProperty);
	if (!SuperStructProp)
	{
		return;
	}

	const FInheritedTagContainer* SuperTags = SuperStructProp->ContainerPtrToValuePtr<FInheritedTagContainer>(SuperClass->GetDefaultObject<UObject>());
	
	for (void* RawPtr : RawData)
	{
		FInheritedTagContainer* GameplayTagContainer = reinterpret_cast<FInheritedTagContainer*>(RawPtr);
		if (GameplayTagContainer)
		{
			GameplayTagContainer->UpdateInheritedTagProperties(SuperTags);
		}
	}
}

void FInheritableTagContainerDetails::UpdateChildrenFromSelfTags()
{
	TSharedPtr<IPropertyHandle> StructPropertyHandle = CombinedTagContainerPropertyHandle->GetParentHandle();
	const UClass* MyClass = StructPropertyHandle->GetOuterBaseClass();
	FProperty* OurTagsProperty = CombinedTagContainerPropertyHandle->GetParentHandle()->GetProperty();
	const FInheritedTagContainer* OurTags = OurTagsProperty->ContainerPtrToValuePtr<FInheritedTagContainer>(MyClass->GetDefaultObject<UObject>());

	for (TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt)
	{
		UClass* ChildClass = *ClassIt;
		if ((ChildClass == MyClass) || !ChildClass->IsChildOf(MyClass)) continue; // Only update on child classes
		
		// Iterate through all properties of the parent class this property is on to find this property on the parent class
		for (TFieldIterator<FProperty>PropertyIt(ChildClass, EFieldIteratorFlags::IncludeSuper); PropertyIt; ++PropertyIt)
		{
			FProperty* TagPropOnChild = *PropertyIt;

			if (TagPropOnChild->GetNameCPP().Equals(OurTagsProperty->GetNameCPP()))
			{
				FInheritedTagContainer* ChildTags = TagPropOnChild->ContainerPtrToValuePtr<FInheritedTagContainer>(ChildClass->GetDefaultObject<UObject>());

				if (ChildTags)
				{
					ChildTags->UpdateInheritedTagProperties(OurTags);
				}
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE
