// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "SMInstance.h"
#include "GameplayStateMachine_Base.generated.h"

/* ~~~~~ Forward Declarations ~~~~~ */
class ARadicalPlayerCharacter;
class UActionManagerComponent;


/// @brief Blueprint state machine object that corresponds to gameplay state machine. Offers built in functionality for use with a RadicalCharacter & ActionManagerComponent
UCLASS(Blueprintable, BlueprintType, ClassGroup = ActionManager, hideCategories = (SMStateMachineInstance), meta = (DisplayName = "Base Gameplay State Machine"))
class ACTIONFRAMEWORK_API UGameplayStateMachine_Base : public USMInstance
{
	GENERATED_BODY()

	// Overrides to check for performance
	//virtual void Tick_Implementation(float DeltaTime) override;
	
public:
	UPROPERTY(Transient)
	TObjectPtr<ARadicalPlayerCharacter> RadicalOwner;
	UPROPERTY(Transient)
	TObjectPtr<UActionManagerComponent> Component;

	UFUNCTION(Category = "Action State Machine", BlueprintCallable)
	void InitializeActions(ARadicalPlayerCharacter* Character, UActionManagerComponent* SetComponent);

	UFUNCTION(Category="Action State Machine", BlueprintCallable)
	void SetRadicalOwner(ARadicalPlayerCharacter* Character) { RadicalOwner = Character;}
	
	UFUNCTION(Category="Action State Machine", BlueprintCallable)
	ARadicalPlayerCharacter* GetRadicalOwner() const { return RadicalOwner; }
	UFUNCTION(Category="Action State Machine", BlueprintCallable)
	UActionManagerComponent* GetActionComponent() const { return Component; }
	
	// ISMInstanceInterface
	/** The object which this state machine is running for. */
	//virtual UObject* GetContext() const override;
	// ~ISMInstanceInterface

protected:
	virtual void OnStateMachineStart_Implementation() override;
	virtual void OnStateMachineStateStarted_Implementation(const FSMStateInfo& State) override;

};