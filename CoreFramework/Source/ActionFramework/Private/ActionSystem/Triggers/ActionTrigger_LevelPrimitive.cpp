// Copyright 2023 CoC All rights reserved


#include "ActionSystem/Triggers/ActionTrigger_LevelPrimitive.h"

#include "Components/ActionSystemComponent.h"

void UActionTrigger_LevelPrimitive::SetupTrigger(int32 Priority)
{
	GetCharacterInfo()->ActionSystemComponent->OnLevelPrimitiveRegisteredDelegate.AddUniqueDynamic(this, &UActionTrigger_LevelPrimitive::LevelPrimitiveRegistered);
	GetCharacterInfo()->ActionSystemComponent->OnLevelPrimitiveUnRegisteredDelegate.AddUniqueDynamic(this, &UActionTrigger_LevelPrimitive::LevelPrimitiveUnRegistered);
}

void UActionTrigger_LevelPrimitive::CleanupTrigger()
{
	GetCharacterInfo()->ActionSystemComponent->OnLevelPrimitiveRegisteredDelegate.RemoveDynamic(this, &UActionTrigger_LevelPrimitive::LevelPrimitiveRegistered);
	GetCharacterInfo()->ActionSystemComponent->OnLevelPrimitiveUnRegisteredDelegate.RemoveDynamic(this, &UActionTrigger_LevelPrimitive::LevelPrimitiveUnRegistered);
}

void UActionTrigger_LevelPrimitive::LevelPrimitiveRegistered(FGameplayTag PrimitiveTag)
{
	if (PrimitiveTag != LevelPrimitiveTag) return;
	
	GetCharacterInfo()->ActionSystemComponent->RegisterActionExecution(OwnerAction.Get());
}

void UActionTrigger_LevelPrimitive::LevelPrimitiveUnRegistered(FGameplayTag PrimitiveTag)
{
	if (PrimitiveTag != LevelPrimitiveTag) return;

	GetCharacterInfo()->ActionSystemComponent->UnRegisterActionExecution(OwnerAction.Get());
}
