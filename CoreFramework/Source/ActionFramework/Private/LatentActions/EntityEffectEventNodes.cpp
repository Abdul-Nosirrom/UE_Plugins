// Copyright 2023 CoC All rights reserved


#include "LatentActions/EntityEffectEventNodes.h"

#include "Components/AttributeSystemComponent.h"
#include "Helpers/ActionFrameworkStatics.h"

UAttributeSystem_WaitForAttributeChanged* UAttributeSystem_WaitForAttributeChanged::WaitForAttributeChanged(
	AActor* TargetActor, FEntityAttribute Attribute, bool OnlyTriggerOnce)
{
	UAttributeSystem_WaitForAttributeChanged* MyObj = NewObject<UAttributeSystem_WaitForAttributeChanged>();
	MyObj->TargetASC = UActionFrameworkStatics::GetAttributeSystemFromActor(TargetActor);
	if (!MyObj->TargetASC.IsValid()) return nullptr;
	MyObj->Attribute = Attribute;
	MyObj->OnlyTriggerOnce = OnlyTriggerOnce;

	return MyObj;
}

void UAttributeSystem_WaitForAttributeChanged::StopListening()
{
	TargetASC->GetEntityAttributeValueChangeDelegate(Attribute).RemoveAll(this);
	SetReadyToDestroy();
	MarkAsGarbage();
}

void UAttributeSystem_WaitForAttributeChanged::Activate()
{
	Super::Activate();

	TargetASC->GetEntityAttributeValueChangeDelegate(Attribute).AddDynamic(this, &ThisClass::OnAttributeChanged);
}

void UAttributeSystem_WaitForAttributeChanged::OnAttributeChanged(const FOnEntityAttributeChangeData& ChangeData)
{
	Changed.Broadcast(ChangeData.Attribute, ChangeData.NewValue, ChangeData.OldValue);
	if (OnlyTriggerOnce)
	{
		StopListening();
	}
}
