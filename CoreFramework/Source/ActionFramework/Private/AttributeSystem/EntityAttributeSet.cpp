// Copyright 2023 CoC All rights reserved
#include "AttributeSystem/EntityAttributeSet.h"

#if WITH_EDITOR 
#include "DataTableEditorUtils.h"
#endif

#include "Components/AttributeSystemComponent.h"
#include "Debug/AttributeLog.h"
#include "Helpers/ActionFrameworkStatics.h"
#include "UObject/FieldPathProperty.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UObjectThreadContext.h"
#include "VisualLogger/VisualLogger.h"

#if ENABLE_VISUAL_LOG
namespace AttributeDebug
{
	int32 bDoAttributeGraphVLogging = 1;
	FAutoConsoleVariableRef CVarDoAttributeGraphVLogging(TEXT("attribute.vlog.AttributeGraph")
		, bDoAttributeGraphVLogging, TEXT("Controls whether Attribute changes are being recorded by VisLog"), ECVF_Cheat);
}
#endif

/*--------------------------------------------------------------------------------------------------------------
* Attribute Reflection
*--------------------------------------------------------------------------------------------------------------*/

bool FEntityAttribute::IsEntityAttributeDataProperty(const FProperty* Property)
{
	if (const FStructProperty* StructProp = CastField<FStructProperty>(Property))
	{
		const UStruct* Struct = StructProp->Struct;
		return Struct && Struct->IsChildOf(FEntityAttributeData::StaticStruct());
	}
	return false;
}

void FEntityAttribute::GetAllAttributeProperties(TArray<FProperty*>& OutProperties, FString FilterMetaStr,
	bool bUseEditorOnlyData)
{
	// Gather all UAttribute classes
	for (TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt)
	{
		UClass *Class = *ClassIt;
		if (Class->IsChildOf(UEntityAttributeSet::StaticClass()) 
#if WITH_EDITORONLY_DATA
			// ClassGeneratedBy TODO: This is wrong in cooked builds
			&& !Class->ClassGeneratedBy
#endif
			)
		{
			if (bUseEditorOnlyData)
			{
#if WITH_EDITOR
				// Allow entire classes to be filtered globally
				if (Class->HasMetaData(TEXT("HideInDetailsView")))
				{
					continue;
				}
#endif
			}

			if (Class == UAttributeSystemComponent::StaticClass())
			{
				continue;
			}


			for (TFieldIterator<FProperty> PropertyIt(Class, EFieldIteratorFlags::ExcludeSuper); PropertyIt; ++PropertyIt)
			{
				FProperty* Property = *PropertyIt;

				if (bUseEditorOnlyData)
				{
#if WITH_EDITOR
					if (!FilterMetaStr.IsEmpty() && Property->HasMetaData(*FilterMetaStr))
					{
						continue;
					}

					// Allow properties to be filtered globally (never show up)
					if (Property->HasMetaData(TEXT("HideInDetailsView")))
					{
						continue;
					}
#endif
				}
				
				OutProperties.Add(Property);
			}
		}
	}
}

FEntityAttributeData* FEntityAttribute::GetEntityAttributeData(UEntityAttributeSet* Src) const
{
	if (Src && IsEntityAttributeDataProperty(Attribute.Get()))
	{
		FStructProperty* StructProperty = CastField<FStructProperty>(Attribute.Get());
		check(StructProperty);
		return StructProperty->ContainerPtrToValuePtr<FEntityAttributeData>(Src);
	}
	return nullptr;
}

const FEntityAttributeData* FEntityAttribute::GetEntityAttributeData(const UEntityAttributeSet* Src) const
{
	if (Src && IsEntityAttributeDataProperty(Attribute.Get()))
	{
		FStructProperty* StructProperty = CastField<FStructProperty>(Attribute.Get());
		check(StructProperty);
		return StructProperty->ContainerPtrToValuePtr<FEntityAttributeData>(Src);
	}
	return nullptr;
}

void FEntityAttribute::SetAttributeValue(float& NewValue, UEntityAttributeSet* Dest) const
{
	float OldValue = 0.f;
	if (Dest && IsEntityAttributeDataProperty(Attribute.Get()))
	{
		FStructProperty* StructProperty = CastField<FStructProperty>(Attribute.Get());
		check(StructProperty);
		FEntityAttributeData* DataPtr = StructProperty->ContainerPtrToValuePtr<FEntityAttributeData>(Dest);
		check(DataPtr);
		OldValue = DataPtr->GetCurrentValue();
		Dest->PreAttributeChange(*this, NewValue);
		DataPtr->SetCurrentValue(NewValue);
		Dest->PostAttributeChange(*this, OldValue, NewValue);
	}
	else
	{
		check(false);
	}
	
#if ENABLE_VISUAL_LOG
	if (AttributeDebug::bDoAttributeGraphVLogging && FVisualLogger::IsRecording())
	{
		AActor* OwnerActor = Dest->GetOwningActor();
		if (OwnerActor)
		{
			ATTRIBUTE_VLOG_GRAPH(OwnerActor, Log, GetName(), OldValue, NewValue);
		}
	}
#endif 
}

float FEntityAttribute::GetAttributeValue(const UEntityAttributeSet* Src) const
{
	if (Src && IsEntityAttributeDataProperty(Attribute.Get()))
	{
		FStructProperty* StructProperty = CastField<FStructProperty>(Attribute.Get());
		check(StructProperty);
		const FEntityAttributeData* DataPtr = StructProperty->ContainerPtrToValuePtr<FEntityAttributeData>(Src);
		if (ensure(DataPtr))
			return DataPtr->GetCurrentValue();
	}
	return 0.f;
}

#if WITH_EDITORONLY_DATA
void FEntityAttribute::PostSerialize(const FArchive& Ar)
{
	// Fill in missing information like name and owner
	if (Ar.IsLoading() && Ar.IsPersistent() && !Ar.HasAnyPortFlags(PPF_Duplicate | PPF_DuplicateForPIE))
	{
		if (Attribute.Get())
		{
			AttributeOwner = Attribute->GetOwnerStruct();
			Attribute->GetName(AttributeName);
		}
		else if (!AttributeName.IsEmpty() && AttributeOwner != nullptr)
		{
			Attribute = FindFProperty<FProperty>(AttributeOwner, *AttributeName);

			if (!Attribute.Get())
			{
				FUObjectSerializeContext* LoadContext = const_cast<FArchive*>(&Ar)->GetSerializeContext();
				FString AssetName = (LoadContext && LoadContext->SerializedObject) ? LoadContext->SerializedObject->GetPathName() : TEXT("Unknown Object");

				FString OwnerName = AttributeOwner ? AttributeOwner->GetName() : TEXT("NONE");
				ATTRIBUTE_LOG(Warning, TEXT("FEntityAttribute::PostSerialize called on an invalid attribute with owner %s and name %s. (Asset: %s)"), *OwnerName, *AttributeName, *AssetName);
			}
		}
	}
}
#endif

/*--------------------------------------------------------------------------------------------------------------
* Attribute Data Table
*--------------------------------------------------------------------------------------------------------------*/

#if WITH_EDITOR

void UAttributeInitDataTable::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	
	if (PropertyChangedEvent.GetPropertyName() != GET_MEMBER_NAME_CHECKED(UAttributeInitDataTable, AttributeSetsToInit))
	{
		return;
	}
	
	// Get all attributes for the sets we wanna initialize
	TArray<FEntityAttribute> AttributesWeInit = {};
	for (TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt)
	{
		UClass* Class = *ClassIt;
		if (!Class || !Class->IsChildOf(UEntityAttributeSet::StaticClass())) continue;
		if (!AttributeSetsToInit.Contains(Class)) continue;
		
		for (TFieldIterator<FProperty> PropertyIt(Class, EFieldIteratorFlags::ExcludeSuper); PropertyIt; ++PropertyIt)
		{
			FProperty* Property = *PropertyIt;

			if (Property)
			{
				AttributesWeInit.Add(FEntityAttribute(Property));
			}
		}
	}

	/// Check which of these attributes we don't support.
	/// - If we have a duplicate of the object, mark the duplicate for remove
	/// - If we have attributes that are not part of our sets, remove those
	TArray<FName> MarkForDelete = {};
	TArray<FEntityAttribute> AttrsAlreadyExists = {};
	TArray<FEntityAttribute> AttrsWeDontHave = AttributesWeInit;
	
	for (auto& RowPair : RowMap)
	{
		if (const auto AttributeEntry = reinterpret_cast<FAttributeTableInit*>(RowPair.Value))
		{
			bool bDuplicate = AttrsAlreadyExists.Contains(AttributeEntry->Attribute);
			bool bNotSupported = !(AttributesWeInit.Contains(AttributeEntry->Attribute) && AttributeEntry->Attribute.IsValid());

			if (bDuplicate || bNotSupported)
			{
				MarkForDelete.Add(RowPair.Key);
			}
			else
			{
				AttrsWeDontHave.RemoveSingle(AttributeEntry->Attribute);
				AttrsAlreadyExists.Add(AttributeEntry->Attribute);
			}
		}
	}

	FDataTableEditorUtils::BroadcastPreChange(this, FDataTableEditorUtils::EDataTableChangeInfo::RowList);

	for (const auto& ToDelete : MarkForDelete)
	{
		RemoveRow(ToDelete);
	}
	for (auto Attribute : AttrsWeDontHave)
	{
		UE_LOG(LogTemp, Error, TEXT("Attempting To Add Attribute %s"), *Attribute.AttributeName);
		AddRow(FName(Attribute.GetName()), FAttributeTableInit(Attribute));
	}

	FDataTableEditorUtils::BroadcastPostChange(this, FDataTableEditorUtils::EDataTableChangeInfo::RowList);
}

void UAttributeInitDataTable::AddRowInternal(FName RowName, uint8* RowDataPtr)
{
	// If we use the AddRow button, the attribute will initialize to null, so we query that to determine whether to allow the add or not
	if (const auto Attr = reinterpret_cast<FAttributeTableInit*>(RowDataPtr))
	{
		if (!Attr->Attribute.IsValid()) return;
	}
	Super::AddRowInternal(RowName, RowDataPtr);
}


#endif

/*--------------------------------------------------------------------------------------------------------------
* Attribute Set
*--------------------------------------------------------------------------------------------------------------*/

void UEntityAttributeSet::InitAttributes(const UAttributeInitDataTable* InitialStats)
{
	// Initialize their values
	// Loop through all our attributes to figure out which one to initialize
	for (TFieldIterator<FProperty> It(GetClass()); It; ++It)
	{
		if (FEntityAttribute::IsEntityAttributeDataProperty(*It))
		{
			auto Attr = (*It)->ContainerPtrToValuePtr<FEntityAttributeData>(this);
			if (Attr)
			{
				const auto InitialAttr = InitialStats->FindRow<FAttributeTableInit>(FName((*It)->GetName()), "AttributeInit");
				if (InitialAttr)
				{
					Attr->SetBaseValue(InitialAttr->InitialValue);
					Attr->SetCurrentValue(InitialAttr->InitialValue);
				}
			}
		}
	}
}

UAttributeSystemComponent* UEntityAttributeSet::GetOwningAttributeSystemComponent() const
{
	return UActionFrameworkStatics::GetAttributeSystemFromActor(GetOwningActor());
}

FActionActorInfo* UEntityAttributeSet::GetActorInfo() const
{
	UAttributeSystemComponent* ASC = GetOwningAttributeSystemComponent();
	if (ASC)
	{
		return &ASC->CurrentActorInfo;
	}

	return nullptr;
}
