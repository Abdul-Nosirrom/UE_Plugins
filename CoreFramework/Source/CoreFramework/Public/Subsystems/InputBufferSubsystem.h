// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "BufferContainer.h"
#include "InputData.h"
#include "InputBufferPrimitives.h"
#include "InputBufferSubsystem.generated.h"

/* Profiling & Log Groups */
DECLARE_STATS_GROUP(TEXT("InputBuffer_Game"), STATGROUP_InputBuffer, STATCAT_Advanced);
DECLARE_CYCLE_STAT(TEXT("Action Bindings"), STAT_ActionBindings, STATGROUP_InputBuffer);
DECLARE_LOG_CATEGORY_EXTERN(LogInputBuffer, Log, All);
/* ~~~~~~~~~~~~~~~~ */

/* FORWARD DECLARATIONS */
class UEnhancedInputComponent;
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
	virtual void PlayerControllerChanged(APlayerController* NewPlayerController) override;
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
	/// @return True if the input is consumed
	UFUNCTION(Category="InputBuffer", BlueprintCallable, meta=(GameplayTagFilter="Input"))
	bool ConsumeInput(const FGameplayTag& InputTag, bool bConsumeNewer = false);
	
	/// @brief  Returns the state of the most recent register of a given input in the buffer
	UFUNCTION(Category="InputBuffer", BlueprintPure, meta=(GameplayTagFilter="Input"))
	bool IsConsumedInputHeld(const FGameplayTag& InputTag) const;

	/// @brief	Returns the time an inputs been held in seconds, regardless of whether its been consumed or not. < 0 if not held.
	UFUNCTION(Category="InputBuffer", BlueprintPure, meta=(GameplayTagFilter="Input"))
	float GetTimeInputHeld(const FGameplayTag& InputTag) const;

	/// @brief	Returns true if an input is firing off the press event and can be consumed
	UFUNCTION(Category="InputBuffer", BlueprintPure, meta=(GameplayTagFilter="Input"))
	bool CanPressInput(const FGameplayTag& InputTag);

	/// @brief	Get an input action from the tag to avoid needing asset references in your code
	UFUNCTION(Category="InputBuffer", BlueprintPure, meta=(GameplayTagFilter="Input"))
	const UBufferedInputAction* GetInputAction(const FGameplayTag& InputTag) const
	{
		auto AllActions = InputMap->GetInputActions();
		for (const auto& action : AllActions)
		{
			if (action->InputTag.MatchesTag(InputTag)) return action;
		}
		return nullptr;
	}

	
	void DisableInput() { bInputDisabled = true; }
	void EnableInput() { bInputDisabled = false; }

	const TBufferContainer<FBufferFrame>& GetInputBufferData() const { return InputBuffer; }

protected:
	/// @brief  True if the specified input has already been consumed
	bool IsInputConsumed(const FGameplayTag& InputID, bool bCheckNewer = false);
	
	/// @brief  If an input is consumed, propagate the consume state up the buffer to most recent register of said input
	void PropagateConsume(const FGameplayTag& InputID, const uint8 FromFrame);

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
	void TriggerInput(const FInputActionInstance& ActionInstance, const FGameplayTag InputID);

	/// @brief Event triggered for an input once its no longer considered held
	void CompleteInput(const FInputActionInstance& ActionInstance, const FGameplayTag InputID);
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

	void ProcessDirectionVector(const FVector2D& DirectionInputVector, FVector& ProcessedInputVector, FVector& OutPlayerForward, FVector& OutPlayerRight) const;

public:
	void DisplayDebug(UCanvas* Canvas, const FDebugDisplayInfo& DebugDisplay, float& YL, float& YPos);
	
protected:
	/* References */
	UPROPERTY(Transient)
	TObjectPtr<UInputBufferMap> InputMap;

	/* CONSTANT BUFFER SETTINGS (Filled during OnInitialize from project settings) */
	uint8 BUTTON_BUFFER_SIZE;
	uint8 BUFFER_SIZE;
	float TICK_INTERVAL;
	uint32 LastFrameNumberWeTicked = INDEX_NONE;
	
	/* ~~~~~ Initialization & Update Tracking ~~~~~ */
	UPROPERTY(Transient)
	bool bInitialized;

	UPROPERTY(Transient)
	bool bInputDisabled;

	UPROPERTY(Transient)
	double ElapsedTime;

	FGameplayTagContainer CachedActionIDs;
	FGameplayTagContainer CachedDirectionalActionIDs;
	TMap<FGameplayTag, FRawInputValue> RawValueContainer;

	/* ~~~~~ Managed Data ~~~~~ */
	/// @brief	The rows of the input buffer, each containing a column corresponding to each input type
	///			each row is an input buffer frame (FBufferFrame), corresponding to the state of each input
	///			at the buffer frame (i)
	TBufferContainer<FBufferFrame> InputBuffer;
	
	/// @brief	Holds the two oldest frames of each input in which it can be used. (-1) corresponds to no input that can used,
	///			meaning its not been registered, or been held for a while such that its no longer valid. Oldest frame to more
	///			easily check chorded actions/input sequence (We hold 2 to account for a button sequence of the same input type)
	UPROPERTY(Transient)
	TMap<FGameplayTag, FBufferStateTuple> ButtonInputValidFrame;

	/// @brief	Holds the oldest frame of which a directional input was registered valid. (-1) corresponds to no input that can be used,
	///			meaning its not been registered (DI have no concept of "held"). Frame held is the oldest frame in the buffer in which the input
	///			was valid.
	UPROPERTY(Transient)
	TMap<FGameplayTag, int8> DirectionalInputValidFrame;

#pragma region EVENTS

protected:

	TMap<FButtonBinding, FInputActionDelegateHandle> ActionDelegates;
	TMap<FButtonSequenceBinding, FInputActionSequenceDelegateHandle> ActionSeqDelegates;
	TMap<FDirectionalBinding, FDirectionalActionDelegateHandle> DirectionalDelegates;
	TMap<FDirectionalSequenceBinding, FDirectionalAndActionDelegateHandle> DirectionAndActionDelegates;
	
public:
	
	void BindAction(const FButtonBinding& Event, const FGameplayTag& InputTag, EBufferTriggerEvent Trigger, bool bAutoConsume, int Priority = 0)
	{
		SCOPE_CYCLE_COUNTER(STAT_ActionBindings);
		if (!CachedActionIDs.HasTagExact(InputTag))
		{
			UE_LOG(LogInputBuffer, Warning, TEXT("FAILED Bind Action: [%s] Input does not exist"), *InputTag.ToString());
			return;
		}
		
		int32 NumBindings = ActionDelegates.Num();
		
		ActionDelegates.Add(Event, FInputActionDelegateHandle(InputTag, Trigger, bAutoConsume, Priority));

		// Sort by Priority When New Action Is bound
		const auto SortByPriority = [](const FInputActionDelegateHandle& Handle1, const FInputActionDelegateHandle& Handle2) -> bool
		{
			return Handle1.Priority > Handle2.Priority;
		};

		ActionDelegates.ValueSort(SortByPriority);

		UE_LOG(LogInputBuffer, Warning, TEXT("<----- BIND ----->"))
		if (NumBindings < ActionDelegates.Num())
		{
			UE_LOG(LogInputBuffer, Warning, TEXT("Bound Input Buffer Action (%s) To Delegate (%s)"), *InputTag.ToString(), *Event.Delegate.GetUObject()->GetName());
		}
		else
		{
			UE_LOG(LogInputBuffer, Warning, TEXT("FAILED to bind Action to delegate (%s)"), *Event.Delegate.GetUObject()->GetName());
		}
		UE_LOG(LogInputBuffer, Warning, TEXT("<------------------>"))
	}


	void BindActionSequence(const FButtonSequenceBinding& Event, const FGameplayTag& FirstAction, const FGameplayTag& SecondAction, bool bAutoConsume, bool bFirstInputIsHold, bool bConsiderOrder, int Priority = 0)
	{
		SCOPE_CYCLE_COUNTER(STAT_ActionBindings);
		if (!(CachedActionIDs.HasTagExact(FirstAction) || CachedActionIDs.HasTagExact(SecondAction)))
		{
			UE_LOG(LogInputBuffer, Warning, TEXT("FAILED Bind Action Sequence: [%s | %s] Both inputs do not exist"), *FirstAction.ToString(), *SecondAction.ToString());
			return;
		}
		
		if (bFirstInputIsHold && (FirstAction == SecondAction))
		{
			UE_LOG(LogInputBuffer, Warning, TEXT("FAILED BindActionSequence: Asked to treat first input as hold but both inputs supplied are the same [%s]"), *FirstAction.ToString());
			return;
		}
		
		ActionSeqDelegates.Add(Event, FInputActionSequenceDelegateHandle(FirstAction, SecondAction, bAutoConsume, bFirstInputIsHold, bConsiderOrder, Priority));

		// Sort by Priority When New Action Is bound
		const auto SortByPriority = [](const FInputActionSequenceDelegateHandle& Handle1, const FInputActionSequenceDelegateHandle& Handle2) -> bool
		{
			return Handle1.Priority > Handle2.Priority;
		};

		ActionSeqDelegates.ValueSort(SortByPriority);
	}

	void BindDirectionalAction(const FDirectionalBinding& Event, const FGameplayTag& DirectionalAction, bool bAutoConsume, int Priority = 0)
	{
		SCOPE_CYCLE_COUNTER(STAT_ActionBindings);
		if (!CachedDirectionalActionIDs.HasTagExact(DirectionalAction))
		{
			UE_LOG(LogInputBuffer, Warning, TEXT("FAILED BindDirectionalAction: [%s] Input does not exist"), *DirectionalAction.ToString());
			return;
		}
		DirectionalDelegates.Add(Event, FDirectionalActionDelegateHandle(DirectionalAction, bAutoConsume, Priority));

		// Sort by Priority When New Action Is bound
		const auto SortByPriority = [](const FDirectionalActionDelegateHandle& Handle1, const FDirectionalActionDelegateHandle& Handle2) -> bool
		{
			return Handle1.Priority > Handle2.Priority;
		};

		DirectionalDelegates.ValueSort(SortByPriority);
	}

	void BindDirectionalActionSequence(const FDirectionalSequenceBinding& Event, const FGameplayTag& InputAction, const FGameplayTag& DirectionalAction, EDirectionalSequenceOrder SequenceOrder, bool bAutoConsume, int Priority = 0)
	{
		SCOPE_CYCLE_COUNTER(STAT_ActionBindings);
		if (!CachedActionIDs.HasTagExact(InputAction) && !CachedDirectionalActionIDs.HasTagExact(DirectionalAction))
		{
			return;
		}
		DirectionAndActionDelegates.Add(Event, FDirectionalAndActionDelegateHandle(InputAction, DirectionalAction, bAutoConsume, SequenceOrder, Priority));

		// Sort by Priority When New Action Is bound
		const auto SortByPriority = [](const FDirectionalAndActionDelegateHandle& Handle1, const FDirectionalAndActionDelegateHandle& Handle2) -> bool
		{
			return Handle1.Priority > Handle2.Priority;
		};

		DirectionAndActionDelegates.ValueSort(SortByPriority);
	}

	// Can't delete them directly because apparently C++ and BP don't run in sequence. Calling this in BP can happen while EvaluateEvents is running causing an ensure fail when an element is removed during the loop
	void UnbindAction(const FButtonBinding& Event)
	{
		UE_LOG(LogInputBuffer, Warning, TEXT("<----- UNBIND ----->"))
		//if (ActionDelegates.Contains(Event))
		{
			//if (Event.IsBound()) Event.Unbind();
			if (ActionDelegates.Contains(Event) && ActionDelegates.Remove(Event))
			{
				const FString ObjName = Event.Delegate.GetUObject() ? Event.Delegate.GetUObject()->GetName() : Event.Delegate.GetFunctionName().ToString();
				UE_LOG(LogInputBuffer, Warning, TEXT("Attempting To Unbind: %s"), *ObjName)
			}
			else
			{
				UE_LOG(LogInputBuffer, Error, TEXT("[%s] FAILED To Unbind: %s"), (ActionDelegates.Contains(Event) ? *FString("TRUE") : *FString("FALSE") ),*Event.Delegate.GetUObject()->GetName())
			}
		}
		UE_LOG(LogInputBuffer, Warning, TEXT("<----------------->"))
	}

	void UnbindActionSequence(const FButtonSequenceBinding& Event)
	{
		if (ActionSeqDelegates.Contains(Event))
		{
			//if (Event.IsBound()) Event.Unbind();
			ActionSeqDelegates.Remove(Event);
		}
	}

	void UnbindDirectionalAction(const FDirectionalBinding& Event)
	{
		if (DirectionalDelegates.Contains(Event))
		{
			//if (Event.IsBound()) Event.Unbind();
			DirectionalDelegates.Remove(Event);
		}
	}

	void UnbindDirectionalActionSequence(const FDirectionalSequenceBinding& Event)
	{
		if (DirectionAndActionDelegates.Contains(Event))
		{
			//if (Event.IsBound()) Event.Unbind();
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
