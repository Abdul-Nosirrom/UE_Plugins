// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "InputData.h"
#include "InteractionManager.generated.h"

class UInteractableComponent;
class UInteractableWidgetBase;
class ARadicalCharacter;

UCLASS()
class INTERACTIONFRAMEWORK_API UInteractionManager : public UTickableWorldSubsystem
{
	GENERATED_BODY()

	friend class UInteractableComponent;

	/*--------------------------------------------------------------------------------------------------------------
	* Inherited Interface Setup
	*--------------------------------------------------------------------------------------------------------------*/

	
	// BEGIN USubsystem Interface
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	// END USubsystem Interface

	// BEGIN FTickableGameObject Interface
	virtual void Tick( float DeltaTime ) override;
	virtual ETickableTickType GetTickableTickType() const override { return ETickableTickType::Always; }
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT( UInputBufferSubsystem, STATGROUP_Tickables ); }
	virtual bool IsTickableWhenPaused() const { return true; }
	virtual bool IsTickableInEditor() const { return false; }
	// END FTickableGameObject Interface

	UPROPERTY(Transient)
	ARadicalCharacter* PlayerCharacter;
	
	bool InitializeRequirements();

protected:
	/*--------------------------------------------------------------------------------------------------------------
	* Registration
	*--------------------------------------------------------------------------------------------------------------*/

	TArray<TWeakObjectPtr<UInteractableComponent>> AvailableInteractables;
	TArray<TWeakObjectPtr<UInteractableComponent>> AvailableInputInteractables;

public:
	
	void RegisterInteractable(UInteractableComponent* InInteractable);
	void UnRegisterInteractable(UInteractableComponent* InInteractable);

	void RegisterInputInteractable(UInteractableComponent* InInteractable);
	void UnRegisterInputInteractable(UInteractableComponent* InInteractable);

protected:
	/*--------------------------------------------------------------------------------------------------------------
	* Rankings And Callbacks
	*--------------------------------------------------------------------------------------------------------------*/

	UPROPERTY(Category=Active, BlueprintReadOnly, BlueprintGetter=GetActiveInteractable)
	UInteractableComponent* ActiveInteractable;

	void UpdateMostViableInteractable();

	void UpdateInteractableWidget();

	void GetOrCreateInteractableWidget();

	UPROPERTY(Transient)
	UInteractableWidgetBase* ActiveInteractableWidget;
	UPROPERTY(Transient)
	TMap<TSubclassOf<UUserWidget>, UInteractableWidgetBase*> InteractableWidgets;
	bool bHasInteractableChanged;

public:
	UFUNCTION(Category=Active, BlueprintCallable)
	UInteractableComponent* GetActiveInteractable() const;

protected:
	/*--------------------------------------------------------------------------------------------------------------
	* Input Handling
	*--------------------------------------------------------------------------------------------------------------*/

	bool bHasInitializedInput;
	

	FButtonBinding InputDelegate;

	UFUNCTION()
	void InputCallback(const FInputActionValue& Value, float ElapsedTime);

	void SetupInputCallbacks();
	void CleanupInputCallbacks();
};
