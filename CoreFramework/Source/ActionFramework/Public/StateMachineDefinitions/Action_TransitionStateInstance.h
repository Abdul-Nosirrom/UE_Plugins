// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ActionStateFlow.h"
#include "SMTransitionInstance.h"
#include "InputData.h"
#include "Action_TransitionStateInstance.generated.h"

/* ~~~~~ Forward Declarations ~~~~~~ */
class UAction_CoreStateInstance;
class ARadicalCharacter;

/// @brief Base gameplay transition for state-to-state transitions. Auto handles some base attributes like input checking
UCLASS(Blueprintable, BlueprintType, ClassGroup="ActionManager", HideCategories=(SMTransitionInstance), meta=(DisplayName="Gameplay Transition"))
class ACTIONFRAMEWORK_API UAction_TransitionStateInstance : public USMTransitionInstance
{
	GENERATED_BODY()

	UAction_TransitionStateInstance();
	
protected:
	virtual void OnTransitionEntered_Implementation() override;
	virtual void OnTransitionInitialized_Implementation() override;
	virtual void OnTransitionShutdown_Implementation() override;

	/// @brief We control CanEvaluate, so we leave this to true
	virtual bool CanEnterTransition_Implementation() const override { return true; };

	virtual void ConstructionScript_Implementation() override;

	// @brief Used to cache references early
	virtual void OnRootStateMachineStart_Implementation() override;
	virtual void OnRootStateMachineStop_Implementation() override;

	UInputBufferSubsystem* GetInputBuffer() const;
	
	/// @brief
	UFUNCTION()
	void EvaluateTransition();
	
	/// @brief  Bound when coming from an AnyState to an EntryState. When called, the PrevState has already "finished" and is
	///			forcing an exit to the entry state
	void ForceExitBinding();

	void SetupBindings();

	void BindInputsIfAny();

	void UnbindInputsIfAny() const;
	
	void ConsumeTransitionInput() const;

	// Cache reference to avoid casting to check tag requirements which may be on tick
	UPROPERTY(Transient)
	TWeakObjectPtr<ARadicalCharacter> CharacterOwner;

	UPROPERTY(Category="Transition Type", EditDefaultsOnly, BlueprintReadWrite)
	bool bIsInterrupt{false};

#pragma region Various Possible Transition Bindings
	UPROPERTY()
	FDirectionalActionSignature DirectionalInputDelegate;
	UFUNCTION()
	void DirectionalInputBinding() { EvaluateTransition(); }
	UPROPERTY()
	FDirectionalAndActionSequenceSignature DirectionalAndButtonInputDelegate;
	UFUNCTION()
	void DirectionalAndButtonInputBinding(const FInputActionValue& ActionValue, float ActionElapsedTime) { EvaluateTransition(); }
	UPROPERTY()
	FInputActionSequenceSignature ButtonSequenceInputDelegate;
	UFUNCTION()
	void ButtonSequenceInputBinding(const FInputActionValue& ValueOne, const FInputActionValue& ValueTwo) { EvaluateTransition(); }
	UPROPERTY()
	FInputActionEventSignature ButtonInputDelegate;
	UFUNCTION()
	void ButtonInputBinding(const FInputActionValue& Value, float ElapsedTime) { EvaluateTransition(); }
	UFUNCTION()
	void TickBinding(USMStateInstance_Base* StateInstance, float DeltaTime) { EvaluateTransition(); }
#pragma endregion Various Possible Transition Bindings

private:
	UPROPERTY(Transient)
	TScriptInterface<IActionStateFlow> PrevState;
	UPROPERTY(Transient)
	TScriptInterface<IActionStateFlow> NextState;
};
