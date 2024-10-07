// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Interfaces/FlowOwnerInterface.h"
#include "Components/InteractableComponent.h"
#include "FlowDialogue/FlowNode_DialogueSequencer.h"
#include "LevelSequence/FlowLevelSequencePlayer.h"
#include "DialogueInteractableComponent.generated.h"

UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Interaction_Dialogue)

DECLARE_DELEGATE_OneParam(FDialogueEvent, int32);

class ULevelSequence;
class IDialogueWidgetInterface;

/// The dialogue update loop
/// [START]
/// - Start the state machine and pass over the instance to the dialogue widget
/// - Component binds to OnStateChanged so it can respond to events, everytime state is started, we parse its events
/// [UPDATE]
/// -
/// [END]
/// -

/// Dialogue Events Rundown
/// - Dialogue events have the option to pause choices. That is, once entering a dialogue node X that's followed by some choice nodes,
///	  we have the option to choose to pause presenting the choices UNTIL all events have finished running
///	- To support the above, we need then a concept of "OnEventCompleted". Below I present a few different Event templates and how they call Compelted
///
///	1.) Camera Blend: Calls OnDialogueEvtCompleted once the blend is complete, blend time is a known quantity so this is easy
///	2.) Play Sound: Maybe when the sound finishes?
///	3.) Play VFX At Location: Maybe when the VFX is destroyed?
///	4.) Play Animation On Target Character: When the animation montage is finished
///	5.) Trigger Sequencer Animation: When the animation is finished
///
///	- The question is, do we handle events in the state or in this component? The state has access to this component so it can retrieve
///	  the references but I'm not sure. Since the flow of the state machine is governed by the UI, I'm guessing the events should prob be handled
///	  by whoever holds a reference to the UI
/// - Right now with a lot of things I'm just struggling to figure out who "owns" the UI. It has to be assigned as an asset reference, could just make it
///   an interface?

/// This component should support the following:
/// - When a dialogue state machine is assigned, we parse through it retrieving all its events and populating our event variable
///	  (This is so we have it ready and whats left is just assigning the variables in the level editor)

UCLASS(ClassGroup=(Interactables), DisplayName="Dialogue Interactable", PrioritizeCategories=(Status, Dialogue, Behavior), Abstract, meta=(BlueprintSpawnableComponent))
class DIALOGUEFRAMEWORK_API UDialogueInteractableComponent : public UInteractableComponent, public  IFlowOwnerInterface
{
	GENERATED_BODY()

	friend class FDialogueComponentVisualizer;
	friend class UDialogueChoice;
	friend class UDialogueLine;
	friend class UDialogueHideUI;

public:
	// Sets default values for this component's properties
	UDialogueInteractableComponent();

	UFUNCTION(Category=Events, BlueprintPure)
	AActor* GetEventActor(const FName& ID) const 
	{
		if (EventWorldReferences.Contains(ID)) return EventWorldReferences[ID];
		return nullptr;
	}

protected:

	// UObject Interface
	virtual void BeginPlay() override;
	// ~UObject Interface

	// Used for Debug Visualizer
	TArray<AActor*> RetrieveEventActors() const;
	
	UPROPERTY(Category="Dialogue", EditAnywhere, BlueprintReadOnly)
	class UDialogueAsset* DialogueAsset;
	UPROPERTY(Category="Dialogue", EditAnywhere, BlueprintReadOnly, meta=(MakeEditWidget))
	FVector SpeakerLocation;

	// Events: A generic array of actors, the events themselves should expect human error
	UPROPERTY(Category="Dialogue", EditAnywhere, BlueprintReadOnly, meta=(ForceInlineRow))
	TMap<FName, AActor*> EventWorldReferences;
	// ~Events

	// Dialogue Asset Interface
	UPROPERTY(Transient)
	UDialogueAsset* DialogueInstance;
	
	TArray<FText> ActiveChoices;
	FDialogueEvent OnDialogueChoiceSelectedEvent;
public:
	void RegisterDialogueChoice(FDialogueEvent& ChoiceSelectedEvent, const TArray<FText>& Choices);
	void UnregisterDialogueChoice();

protected:
	UPROPERTY(Transient)
	ULevelSequencePlayer* ActiveSequencer;

	FDialogueEvent OnDialogueLinesCompleted;
	TArray<FText> ActiveDialogueLines;
	FText ActiveSpeaker;
	int32 ActiveLineIdx;
public:
	void RegisterDialogueLine(FDialogueEvent& CompletedEvent, const FText& Speaker, const TArray<FText>& Lines);
	void UnregisterDialogueLine();

	// For use with "Call Blueprint Owner Function"
	void HideDialogueUI();
	// Dialogue Asset Interface

	void SetActiveSequencer(ULevelSequencePlayer* SequencePlayer) { ActiveSequencer = SequencePlayer; }
	ULevelSequencePlayer* GetActiveSequencer() const { return ActiveSequencer; }

public:
	
	// UI Interface [Functions to be called by the UI to progress the dialogue
	class IDialogueControllerInterface* DialogueController; 
	TWeakObjectPtr<UUserWidget> DialogueUI;
	void SetDialogueUI(TWeakObjectPtr<UUserWidget> InUI) { DialogueUI = InUI; }
	UUserWidget* GetDialogueUI() const { return DialogueUI.Get(); }
	UFUNCTION(Category="Dialogue", BlueprintCallable)
	void OnDialogueLineDisplayCompleted();
	UFUNCTION(Category="Dialogue", BlueprintCallable)
	void OnDialogueChoiceSelected(int ChoiceIdx);
	// ~UI Interface

protected:
	
	// UFlowSubsystem
	class UFlowSubsystem* GetFlowSubsystem() const;
	// ~UFlowSubsystem

	// UInteractable Interface
	UFUNCTION()
	void StartDialogue();
	UFUNCTION()
	void UpdateDialogue(float DeltaTime);
	UFUNCTION()
	void EndDialogue();
	// ~UInteractable Interface

	// Interaction Action Interface
	UFUNCTION()
	void CalcVelocity(URadicalMovementComponent* MovementComponent, float DeltaTime);
	UFUNCTION()
	void UpdatePlayerRotation(URadicalMovementComponent* MovementComponent, float DeltaTime);
	// ~Interaction Action Interface
	

#if WITH_EDITOR
	UFUNCTION(Category="Dialogue", CallInEditor)
	void ValidateEvents();
#endif 
};

// Simple utility actor to execute dialogue lines in sequencer through bindings
UCLASS()
class ADialogueSequencerManager : public AActor
{
	GENERATED_BODY()
protected:
	FDialogueEvent OnDialogueLineDisplayed;
	
public:
	UPROPERTY(EditInstanceOnly)
	AActor* DialogueActor;
	UPROPERTY(EditInstanceOnly)
	class ULevelSequencePlayer* DlgSequencerPlayer;

	FTimerHandle TimerHandle;
	bool bDialogueLineCompleted;
	
	UFUNCTION(BlueprintCallable)
	void PlaySequencerDialogueLine(class ULevelSequenceDirector* SequencePlayer, const FText& Speaker, UPARAM(meta=(MultiLine=true)) const FText& Lines, float PauseAfterTime = 0.f);

	UFUNCTION(BlueprintCallable)
	void SequencerDialogueLine(const FString& Speaker, const FString& Lines, float PauseAfterTime = 0.f);

};