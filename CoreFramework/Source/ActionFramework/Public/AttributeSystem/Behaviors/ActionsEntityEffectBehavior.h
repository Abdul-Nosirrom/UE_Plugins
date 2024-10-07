// Copyright 2023 CoC All rights reserved

#pragma once

#include "CoreMinimal.h"
#include "AttributeSystem/EntityEffectBehavior.h"
#include "ActionsEntityEffectBehavior.generated.h"


UENUM(BlueprintType)
enum class EEntityEffectGrantedActionRemovePolicy : uint8
{
	/// @brief	Active abilities are immediately canceled and the ability is removed. 
	CancelActionImmediately,
	/// @brief	Active abilities are allowed to finish, and then removed. 
	RemoveActionOnEnd,
	/// @brief	Granted actions are left lone when the granting EntityEffect is removed.
	DoNothing,
};

USTRUCT()
struct FEffectActionGrantingSpec
{
	GENERATED_BODY()
	
	/// @brief  What action to grant
	UPROPERTY(Category="Action Definition", EditDefaultsOnly)
	class UGameplayActionData* Action;
	
	/// @brief	What will remove this action later
	UPROPERTY(Category="Action Definition", EditDefaultsOnly)
	EEntityEffectGrantedActionRemovePolicy RemovalPolicy = EEntityEffectGrantedActionRemovePolicy::CancelActionImmediately;
};

UCLASS()
class ACTIONFRAMEWORK_API UActionsEntityEffectBehavior : public UEntityEffectBehavior
{
	GENERATED_BODY()

public:
	UActionsEntityEffectBehavior()
	{
#if WITH_EDITORONLY_DATA
		EditorFriendlyName = TEXT("Grant Actions While Active");
#endif
	}

	virtual void OnEntityEffectApplied(UAttributeSystemComponent* AttributeSystem, FActiveEntityEffect& ActiveEffect) const override;
	virtual void OnEntityEffectRemoved(UAttributeSystemComponent* AttributeSystem, FActiveEntityEffect& ActiveEffect, bool bPrematureRemoval) const override;
	
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

protected:

	UPROPERTY(Category="GrantActions", EditDefaultsOnly)
	TArray<FEffectActionGrantingSpec> GrantActionsConfigs;
};
