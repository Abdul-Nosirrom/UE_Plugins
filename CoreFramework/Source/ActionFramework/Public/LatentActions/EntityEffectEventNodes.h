// Copyright 2023 CoC All rights reserved

#pragma once

#include "CoreMinimal.h"
#include "AttributeSystem/EntityAttributeSet.h"
#include "AttributeSystem/EntityEffectTypes.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "EntityEffectEventNodes.generated.h"


UCLASS(BlueprintType, meta=(ExposedAsyncProxy = AsyncTask))
class ACTIONFRAMEWORK_API UAttributeSystem_WaitForAttributeChanged : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

public:

	UFUNCTION(BlueprintCallable, Category = "Entity Attributes", meta = (DefaultToSelf = "TargetActor", BlueprintInternalUseOnly = "TRUE"))
	static UAttributeSystem_WaitForAttributeChanged* WaitForAttributeChanged(AActor* TargetActor, FEntityAttribute Attribute, bool OnlyTriggerOnce = false);

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FAsyncWaitAttributeChangedDelegate, FEntityAttribute, Attribute, float, NewValue, float, OldValue);
	UPROPERTY(BlueprintAssignable)
	FAsyncWaitAttributeChangedDelegate Changed;

	UFUNCTION(Category="Entity Attributes", BlueprintCallable)
	void StopListening();
	
protected:

	virtual void Activate() override;

	UFUNCTION()
	void OnAttributeChanged(const FOnEntityAttributeChangeData& ChangeData);

	FEntityAttribute Attribute;
	TWeakObjectPtr<UAttributeSystemComponent> TargetASC;
	bool OnlyTriggerOnce = false;

	FDelegateHandle MyHandle;
};
