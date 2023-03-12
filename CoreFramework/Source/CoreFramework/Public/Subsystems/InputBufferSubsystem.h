﻿// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "BufferContainer.h"
#include "InputData.h"
#include "InputBufferPrimitives.h"
#include "InputAction.h"
#include "InputBufferSubsystem.generated.h"

/* Profiling & Log Groups */
DECLARE_STATS_GROUP(TEXT("InputBuffer_Game"), STATGROUP_InputBuffer, STATCAT_Advanced);
DECLARE_LOG_CATEGORY_EXTERN(LogInputBuffer, Log, All);
/* ~~~~~~~~~~~~~~~~ */


/* FORWARD DECLARATIONS */
struct FBufferFrame;
struct FRawInputValue;
/*~~~~~~~~~~~~~~~~~~~~~*/

UCLASS()
class COREFRAMEWORK_API UInputBufferSubsystem : public ULocalPlayerSubsystem, public FTickableGameObject
{
	GENERATED_BODY()

	friend class ABufferedController;
	friend struct FBufferFrame;
	friend struct FInputFrameState;
	
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
	
public:
	/* ~~~~~ Input Buffer API ~~~~~ */
	/// @brief  Creates an input buffer from the given buffer map, and registers input and directional action events
	/// @param  TargetInputMap Input Buffer Data Asset To Initialize Actions With
	/// @param  InputComponent EIC Necessary For Binding To Its Events
	void AddMappingContext(UInputBufferMap* TargetInputMap, UEnhancedInputComponent* InputComponent);

	/// @brief Registers an input in the buffer as used, setting its current state to (-1) which is maintained until it's released
	/// @param Input Data asset corresponding to input to be consumed
	/// @return True if the input is consumed
	UFUNCTION(BlueprintCallable)
	bool ConsumeButtonInput(const UInputAction* Input);
	bool ConsumeButtonInput(const FName InputID, bool bConsumeNewer = false);

	/// @brief Registers an input in the buffer as used, setting its current state to (-1) which is maintained until it's released
	/// @param Input Data asset corresponding to input to be consumed
	/// @return True if the input is consumed
	UFUNCTION(BlueprintCallable)
	bool ConsumeDirectionalInput(const UMotionAction* Input);
	bool ConsumeDirectionalInput(const FName InputID);

protected:
	/// @brief  If an input is consumed, propagate the consume state up the buffer to most recent register of said input
	void PropagateConsume(const FName InputID, const uint8 FromFrame);

	/// @brief  Updates the buffer with input received from EIC [Fixed Tick Interval]
	void UpdateBuffer();

	/// @brief  Checks the buffers current state and broadcasts the appropriate input events [Tick constantly]
	void EvaluateEvents();

protected:
	/* ~~~~~ Input Registration & Initialization ~~~~~ */
	/// @brief  Adds the input map to the EIC subsystem & binds EIC events to buffer data
	void InitializeInputMapping(UEnhancedInputComponent* InputComponent);

	/// @brief Initializes buffer structures, called after InputMapping was initialized and IDs generated
	void InitializeInputBufferData();

	/// @brief Event triggered for an input once its been and continues to be triggered
	/// @param InputName FName descriptor of action used to index into Input Maps
	void TriggerInput(const FInputActionInstance& ActionInstance, const FName InputName);

	/// @brief Event triggered for an input once its no longer considered held
	/// @param InputName FName descriptor of action used to index into Input Maps
	void CompleteInput(const FInputActionInstance& ActionInstance, const FName InputName);
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

	void ProcessDirectionVector(const FVector2D& DirectionInputVector, FVector& ProcessedInputVector, FVector& OutPlayerForward, FVector& OutPlayerRight) const;
	
	void DisplayDebug(UCanvas* Canvas, const FDebugDisplayInfo& DebugDisplay, float& YL, float& YPos);
	
protected:
	/* References */
	TObjectPtr<UInputBufferMap> InputMap;

	/* CONSTANT BUFFER SETTINGS */
	static constexpr uint8 BUTTON_BUFFER_SIZE	= 20;
	static constexpr uint8 BUFFER_SIZE			= 40;
	static constexpr float TICK_INTERVAL		= 0.02;
	uint32 LastFrameNumberWeTicked = INDEX_NONE;
	
	/* ~~~~~ Initialization & Update Tracking ~~~~~ */
	UPROPERTY(Transient)
	bool bInitialized;

	UPROPERTY(Transient)
	double ElapsedTime;

	// NOTE: Static so buffer primitives can access it without having to go through a Subsystem getter (also in our case we're only ever gonna have 1 LocalPlayer)
	static TArray<FName> CachedActionIDs; // BUG: These being static will result in crashes in PIE when making a change
	static TMap<FName, FRawInputValue> RawValueContainer;

	/* ~~~~~ Managed Data ~~~~~ */
	/// @brief	The rows of the input buffer, each containing a column corresponding to each input type
	///			each row is an input buffer frame (FBufferFrame), corresponding to the state of each input
	///			at the buffer frame (i)
	TBufferContainer<FBufferFrame> InputBuffer;
	
	/// @brief	Holds the two oldest frames of each input in which it can be used. (-1) corresponds to no input that can used,
	///			meaning its not been registered, or been held for a while such that its no longer valid. Oldest frame to more
	///			easily check chorded actions/input sequence (We hold 2 to account for a button sequence of the same input type)
	UPROPERTY(Transient)
	TMap<FName, FBufferStateTuple> ButtonInputValidFrame;

	/// @brief	Holds the oldest frame of which a directional input was registered valid. (-1) corresponds to no input that can be used,
	///			meaning its not been registered (DI have no concept of "held"). Frame held is the oldest frame in the buffer in which the input
	///			was valid.
	UPROPERTY(Transient)
	TMap<FName, int8> DirectionalInputValidFrame;

#pragma region EVENTS

protected:

	UPROPERTY(Transient)
	TMap<FInputActionEventSignature,FInputActionDelegateHandle> ActionDelegates;
	UPROPERTY(Transient)
	TMap<FInputActionSequenceSignature, FInputActionSequenceDelegateHandle> ActionSeqDelegates;
	UPROPERTY(Transient)
	TMap<FDirectionalActionSignature, FDirectionalActionDelegateHandle> DirectionalDelegates;
	UPROPERTY(Transient)
	TMap<FDirectionalAndActionSequenceSignature, FDirectionalAndActionDelegateHandle> DirectionAndActionDelegates;
	
public:
	
	UFUNCTION(BlueprintCallable)
	void BindAction(FInputActionEventSignature Event, UInputAction* Action, EBufferTriggerEvent Trigger, bool bAutoConsume)
	{
		if (!Action) return;
		ActionDelegates.Add(Event, FInputActionDelegateHandle(FName(Action->ActionDescription.ToString()), Trigger, bAutoConsume));
	}


	UFUNCTION(BlueprintCallable)
	void BindActionSequence(FInputActionSequenceSignature Event, const UInputAction* FirstAction, const UInputAction* SecondAction, bool bAutoConsume, bool bConsiderOrder)
	{
		if (!FirstAction || !SecondAction) return;
		const FName ID1 = FName(FirstAction->ActionDescription.ToString());
		const FName ID2 = FName(SecondAction->ActionDescription.ToString());
		ActionSeqDelegates.Add(Event, FInputActionSequenceDelegateHandle(ID1, ID2, bAutoConsume, bConsiderOrder));
	}

	UFUNCTION(BlueprintCallable)
	void BindDirectionalAction(FDirectionalActionSignature Event, const UMotionAction* DirectionalAction, bool bAutoConsume)
	{
		if (!DirectionalAction) return;
		DirectionalDelegates.Add(Event, FDirectionalActionDelegateHandle(DirectionalAction->GetID(), bAutoConsume));
	}

	UFUNCTION(BlueprintCallable)
	void BindDirectionalActionSequence(FDirectionalAndActionSequenceSignature Event, const UInputAction* InputAction, const UMotionAction* DirectionalAction, EDirectionalSequenceOrder SequenceOrder, bool bAutoConsume)
	{
		if (!InputAction) return;
		DirectionAndActionDelegates.Add(Event, FDirectionalAndActionDelegateHandle(FName(InputAction->ActionDescription.ToString()), DirectionalAction->GetID(), bAutoConsume, SequenceOrder));
	}

	UFUNCTION(BlueprintCallable)
	void UnbindAction(FInputActionEventSignature Event)
	{
		if (ActionDelegates.Contains(Event) && Event.IsBound())
		{
			Event.Unbind();
			ActionDelegates.Remove(Event);
		}
	}

	UFUNCTION(BlueprintCallable)
	void UnbindActionSequence(FInputActionSequenceSignature Event)
	{
		if (ActionSeqDelegates.Contains(Event) && Event.IsBound())
		{
			Event.Unbind();
			ActionSeqDelegates.Remove(Event);
		}
	}

	UFUNCTION(BlueprintCallable)
	void UnbindDirectionalAction(FDirectionalActionSignature Event)
	{
		if (DirectionalDelegates.Contains(Event) && Event.IsBound())
		{
			Event.Unbind();
			DirectionalDelegates.Remove(Event);
		}
	}

	UFUNCTION(BlueprintCallable)
	void UnbindDirectionalActionSequence(FDirectionalAndActionSequenceSignature Event)
	{
		if (DirectionAndActionDelegates.Contains(Event) && Event.IsBound())
		{
			Event.Unbind();
			DirectionAndActionDelegates.Remove(Event);
		}
	}

	/*	FEnhancedInputActionEventBinding& BindAction(const UInputAction* Action, ETriggerEvent TriggerEvent, UObject* Object, FName FunctionName)
		{
			TUniquePtr<FEnhancedInputActionEventDelegateBinding<FEnhancedInputActionHandlerDynamicSignature>> AB = MakeUnique<FEnhancedInputActionEventDelegateBinding<FEnhancedInputActionHandlerDynamicSignature>>(Action, TriggerEvent);
			AB->Delegate.BindDelegate(Object, FunctionName);
			AB->Delegate.SetShouldFireWithEditorScriptGuard(bShouldFireDelegatesInEditor);
			return *EnhancedActionEventBindings.Add_GetRef(MoveTemp(AB));
		}*/

#pragma endregion EVENTS
};
