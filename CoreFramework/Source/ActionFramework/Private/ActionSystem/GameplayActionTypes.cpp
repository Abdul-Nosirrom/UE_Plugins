// 


#include "ActionSystem/GameplayActionTypes.h"

#include "InputBufferSubsystem.h"
#include "RadicalCharacter.h"
#include "RadicalMovementComponent.h"
#include "Components/ActionSystemComponent.h"
#include "Components/AttributeSystemComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Debug/ActionSystemLog.h"
#include "UObject/UObjectIterator.h"

void FActionActorInfo::InitFromCharacter(ARadicalCharacter* Character, UActionSystemComponent* InActionSystemComponent)
{
	check(Character);
	check(InActionSystemComponent);
	
	CharacterOwner = Character;
	OwnerActor = Character;
	ActionSystemComponent = InActionSystemComponent;
	MovementComponent = Character->GetCharacterMovement();
	SkeletalMeshComponent = Character->GetMesh();
	AttributeSystemComponent = Character->FindComponentByClass<UAttributeSystemComponent>();
}

void FActionActorInfo::InitFromActor(AActor* InOwnerActor)
{
	check(InOwnerActor);

	OwnerActor = InOwnerActor;
	CharacterOwner = Cast<ARadicalCharacter>(InOwnerActor);
	ActionSystemComponent = OwnerActor->FindComponentByClass<UActionSystemComponent>();
	AttributeSystemComponent = OwnerActor->FindComponentByClass<UAttributeSystemComponent>();
	MovementComponent = OwnerActor->FindComponentByClass<URadicalMovementComponent>();
	SkeletalMeshComponent = OwnerActor->FindComponentByClass<USkeletalMeshComponent>();
}


FActionScriptEvent* FActionEventWrapper::GetEvent(UGameplayAction* Action)
{
	FStructProperty* StructProperty = CastField<FStructProperty>(Event.Get());
	check(StructProperty);
	return StructProperty->ContainerPtrToValuePtr<FActionScriptEvent>(Action);
}

// NOTE: Maybe not needed, we do this there just to fill out other info (which idk if we need here)
#if WITH_EDITORONLY_DATA
void FActionEventWrapper::PostSerialize(const FArchive& Ar)
{
	/*
	// Fill in missing information like name and owner
	if (Ar.IsLoading() && Ar.IsPersistent() && !Ar.HasAnyPortFlags(PPF_Duplicate | PPF_DuplicateForPIE))
	{
		if (Event.Get())
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
				ACTIONSYSTEM_LOG(Warning, "FEntityAttribute::PostSerialize called on an invalid attribute with owner %s and name %s. (Asset: %s)", *OwnerName, *AttributeName, *AssetName);
			}
		}
	}*/
}
#endif

void FActionEventWrapper::GetAllEventProperties(const UClass* TargetClass, TArray<FProperty*>& OutProperties, FString FilterMetaStr)
{
	
	// Gather all event properties we'd be allowed to bind to
	for (TFieldIterator<FProperty> PropertyIt(TargetClass, EFieldIterationFlags::IncludeSuper); PropertyIt; ++PropertyIt)
	{
		FProperty* Property = *PropertyIt;

#if WITH_EDITOR
		if (!FilterMetaStr.IsEmpty() && Property->HasMetaData(*FilterMetaStr)) continue;

		if (Property->HasMetaData(TEXT("HideInDetailsView"))) continue;
#endif

		if (auto StructProp = CastField<FStructProperty>(Property))
		{
			if (StructProp->Struct == FActionScriptEvent::StaticStruct())
				OutProperties.Add(Property);
		}
	}
}
