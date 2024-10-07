// Fill out your copyright notice in the Description page of Project Settings.


#include "Helpers/ActionFrameworkStatics.h"

#include "AttributeSystem/AttributeSystemInterface.h"
#include "Components/AttributeSystemComponent.h"


UAttributeSystemComponent* UActionFrameworkStatics::GetAttributeSystemFromActor(const AActor* Actor, bool bLookForComponent)
{
	
	if (Actor == nullptr)
	{
		return nullptr;
	}

	const IAttributeSystemInterface* ASI = Cast<IAttributeSystemInterface>(Actor);
	if (ASI)
	{
		return ASI->GetAttributeSystem();
	}

	if (bLookForComponent)
	{
		// Fall back to a component search to better support BP-only actors
		return Actor->FindComponentByClass<UAttributeSystemComponent>();
	}

	return nullptr;
}

void UActionFrameworkStatics::SetEntityEffectCauser(FEntityEffectSpec& InEffectSpec, AActor* InEffectCauser)
{
	InEffectSpec.SetContextEffectCauser(InEffectCauser);
}

AActor* UActionFrameworkStatics::GetEntityEffectInstigator(const FEntityEffectSpec& InEffectSpec)
{
	return InEffectSpec.GetContext().GetInstigatorActor();
}

AActor* UActionFrameworkStatics::GetEntityEffectCauser(const FEntityEffectSpec& InEffectSpec)
{
	return InEffectSpec.GetContext().GetEffectCauser();
}

float UActionFrameworkStatics::GetEntityAttributeValue(const AActor* Actor, FEntityAttribute Attribute,
	bool& bSuccessfullyFoundAttribute)
{
	bSuccessfullyFoundAttribute = false;
	if (const auto ASC = GetAttributeSystemFromActor(Actor))
	{
		return ASC->GetAttributeValue(Attribute, bSuccessfullyFoundAttribute);
	}
	return 0.f;
}

float UActionFrameworkStatics::GetEntityAttributeBaseValue(const AActor* Actor, FEntityAttribute Attribute,
	bool& bSuccessfullyFoundAttribute)
{
	float Result = 0.f;
	bSuccessfullyFoundAttribute = false;
	if (const auto ASC = GetAttributeSystemFromActor(Actor))
	{
		if (ASC->HasAttributeSetForAttribute(Attribute))
		{
			bSuccessfullyFoundAttribute = true;
			Result = ASC->GetAttributeBaseValue(Attribute);
		}
	}
	return Result;
}

bool UActionFrameworkStatics::EqualEqual_EntityAttributeEntityAttribute(FEntityAttribute AttributeA,
	FEntityAttribute AttributeB)
{
	return AttributeA == AttributeB;
}

bool UActionFrameworkStatics::NotEqual_EntityAttributeEntityAttribute(FEntityAttribute AttributeA,
	FEntityAttribute AttributeB)
{
	return AttributeA != AttributeB;
}
