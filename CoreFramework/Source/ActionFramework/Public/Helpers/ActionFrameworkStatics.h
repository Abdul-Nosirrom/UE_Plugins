// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AttributeSystem/EntityAttributeSet.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ActionFrameworkStatics.generated.h"


class UAttributeSystemComponent;
struct FEntityEffectContext;
struct FEntityEffectSpec;

UCLASS()
class ACTIONFRAMEWORK_API UActionFrameworkStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/*--------------------------------------------------------------------------------------------------------------
	* Getters for stuff
	*--------------------------------------------------------------------------------------------------------------*/

	UFUNCTION(Category="Entity Attributes", BlueprintPure)
	static UAttributeSystemComponent* GetAttributeSystemFromActor(const AActor* Actor, bool bLookForComponent=true);

	/*--------------------------------------------------------------------------------------------------------------
	* Entity Effect Helpers
	*--------------------------------------------------------------------------------------------------------------*/
	
	UFUNCTION(Category="Entity Effects", BlueprintPure)
	static void SetEntityEffectCauser(FEntityEffectSpec& InEffectSpec, AActor* InEffectCauser);
	
	UFUNCTION(Category="Entity Effects", BlueprintPure)
	static AActor* GetEntityEffectInstigator(const FEntityEffectSpec& InEffectSpec);

	UFUNCTION(Category="Entity Effects", BlueprintPure)
	static AActor* GetEntityEffectCauser(const FEntityEffectSpec& InEffectSpec);

	/*--------------------------------------------------------------------------------------------------------------
	* Attribute Helpers
	*--------------------------------------------------------------------------------------------------------------*/

	/// @brief	Returns the value of Attribute from the attribute system component belonging to Actor. */
	UFUNCTION(Category = "Entity Attributes", BlueprintPure)
	static float GetEntityAttributeValue(const AActor* Actor, FEntityAttribute Attribute, bool& bSuccessfullyFoundAttribute);
	
	/// @brief	Returns the base value of Attribute from the attribute system component belonging to Actor. */
	UFUNCTION(Category = "Attributes", BlueprintPure)
	static float GetEntityAttributeBaseValue(const AActor* Actor, FEntityAttribute Attribute, bool& bSuccessfullyFoundAttribute);
	
	/// @brief	Simple equality operator for entity attributes */
	UFUNCTION(Category = "Attributes", BlueprintPure, meta=(DisplayName = "Equal (Gameplay Attribute)", CompactNodeTitle = "==", Keywords = "== equal"))
	static bool EqualEqual_EntityAttributeEntityAttribute(FEntityAttribute AttributeA, FEntityAttribute AttributeB);

	/// @brief	Simple inequality operator for entity attributes */
	UFUNCTION(Category = "Attributes", BlueprintPure, meta=(DisplayName = "Equal (Gameplay Attribute)", CompactNodeTitle = "==", Keywords = "== equal"))
	static bool NotEqual_EntityAttributeEntityAttribute(FEntityAttribute AttributeA, FEntityAttribute AttributeB);

	/// @brief	Used for K2Node_EntityAttributeSwitch
	UFUNCTION(Category = PinOptions, BlueprintPure, meta=(BlueprintInternalUseOnly="TRUE"))
	static bool NotEqual_EntityAttributeStringAttribute(FEntityAttribute AttributeA, FString AttributeB)
	{
		return AttributeA.GetName() != AttributeB;
	}
};
