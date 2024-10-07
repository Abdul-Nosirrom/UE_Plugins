// Copyright 2023 CoC All rights reserved

#pragma once

#include "CoreMinimal.h"
#include "Components/InteractionScript.h"
#include "InteractionBehavior.generated.h"


UENUM(BlueprintType)
enum class EInteractionDomain : uint8
{
	OnStart,
	OnUpdate,
	OnEnd
};

UCLASS(Abstract)
class INTERACTIONFRAMEWORK_API UInteractionBehavior : public UInteractionScript
{
	GENERATED_BODY()

	friend class UInteractableComponent;
	
protected:
	
	bool bBehaviorCompleted;


	bool IsComplete() const { return bBehaviorCompleted; }
	void CompleteBehavior() { bBehaviorCompleted = true; }

	/// @brief  Called OnBeginPlay, used for setting up stuff for the behavior.
	virtual void Initialize(class UInteractableComponent* OwnerInteractable) override;
	/// @brief  Called before any behaviors in the OnStart domain are called. Used for cleaning up stuff or setting references
	UFUNCTION()
	virtual void InteractionStarted() { bBehaviorCompleted = false; }
	/// @brief  Function where your actual behavior should go. Called numerous times, you should call CompleteBehavior when your behavior is done
	///			otherwise we it won't move on to the next domain
	virtual void ExecuteInteraction(EInteractionDomain Domain, float DeltaTime) { }
	/// @brief  Called after all behaviors in the OnEnd domain are called. Used for cleaning up stuff or setting references.
	UFUNCTION()
	virtual void InteractionEnded() {}

#if WITH_EDITOR
	virtual EDataValidationResult IsBehaviorValid(EInteractionDomain InteractionDomain, FDataValidationContext& Context);
#endif 

};
