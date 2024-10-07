// 

#pragma once

#include "CoreMinimal.h"
#include "CharacterActionSet.h"
#include "GameplayActionTypes.h"
#include "Data/GameplayTagData.h"
#include "InputData.h"
#include "Engine/World.h"
#include "UObject/Object.h"
#include "GameplayTagContainer.h"
#include "NativeGameplayTags.h"
#include "GameplayAction.generated.h"


/* ~~~~~ Forward Declarations ~~~~~ */
class ARadicalCharacter;
class UActionSystemComponent;
class URadicalMovementComponent;
class UActionTrigger;
class UActionCondition;
class UActionEvent;
struct FActionActorInfo;
struct FActionInputRequirement;

/* ~~~~~ Event Definitions ~~~~~ */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCoolDownStarted, float, CoolDown);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnCoolDownEnded);

/* ~~~~~ Declare Primary Gameplay Tags ~~~~~ */
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Action_Base)
/// @brief Gameplay tags automatically added to actions who override CalcVelocity
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Action_Movement)

#pragma region Action Data

UCLASS(Abstract, NotBlueprintable, ClassGroup=Actions, Category="Gameplay Actions", DisplayName="Base Gameplay Action Data")
class ACTIONFRAMEWORK_API UGameplayActionData : public UDataAsset
{
	friend class UGameplayAction;
	friend class UActionSystemComponent;
	
	GENERATED_BODY()

public:
	UPROPERTY(Category=Definition, EditDefaultsOnly, Instanced)
	UGameplayAction* Action;

	void SetActionTag(FGameplayTag Tag) { ActionTags.AddTag(Tag); }

	bool HasTag(FGameplayTag Tag) const { return ActionTags.HasTag(Tag); }

protected:
	/*--------------------------------------------------------------------------------------------------------------
	* Activation Rules
	*--------------------------------------------------------------------------------------------------------------*/
	/// @brief  If true, if the action has not been granted in an attempted activate call, it'll be auto-granted
	UPROPERTY(Category="Activation Rules", EditDefaultsOnly)
	uint8 bGrantOnActivation		: 1;
	/// @brief  If true, when trying to activate this ability when its already active, it'll end and restart
	UPROPERTY(Category="Activation Rules", EditDefaultsOnly)
	uint8 bRetriggerAbility			: 1;

public:
	/*--------------------------------------------------------------------------------------------------------------
	* Tags 
	*--------------------------------------------------------------------------------------------------------------*/
	/// @brief  The action has these tags associated with it. These tags describe the action.
	///			Example, a combat action would have Action.Combat, an action that modifies movement Action.Movement.
	///			These tags are also granted to the owning ActionSystemComponent on activation and removed on end
	UPROPERTY(Category=Tags, EditDefaultsOnly, BlueprintReadOnly)
	FGameplayTagContainer ActionTags;
	/// @brief  The action can only activate if the owner has all of these tags
	UPROPERTY(Category=Tags, EditDefaultsOnly, BlueprintReadOnly)
	FGameplayTagContainer OwnerRequiredTags;
	/// @brief  The action will block the activation of any other action that has any of these tags in ActionTags
	UPROPERTY(Category=Tags, EditDefaultsOnly, BlueprintReadOnly)
	FGameplayTagContainer BlockActionsWithTag;
	/// @brief  The action will cancel any other running action that has any of these tags in ActionTags
	UPROPERTY(Category=Tags, EditDefaultsOnly, BlueprintReadOnly)
	FGameplayTagContainer CancelActionsWithTag;

	/*--------------------------------------------------------------------------------------------------------------
	* Followups Info
	*--------------------------------------------------------------------------------------------------------------*/

	// NOTE: Maybe split up into PrimaryActionData & AdditiveActionData, only primary has followups and additives?

	virtual TArray<FActionSetEntry> GetFollowups() const { return TArray<FActionSetEntry>(); }
	virtual TArray<FActionSetEntry> GetAdditives() const { return TArray<FActionSetEntry>(); }

};

UCLASS(ClassGroup=Actions, Category="Gameplay Actions", DisplayName="Additive Action Data", Blueprintable)
class ACTIONFRAMEWORK_API UAdditiveActionData : public UGameplayActionData
{
	GENERATED_BODY()
};

UCLASS(ClassGroup=Actions, Category="Gameplay Actions", DisplayName="Primary Action Data", Blueprintable)
class ACTIONFRAMEWORK_API UPrimaryActionData : public UGameplayActionData
{
	GENERATED_BODY()

public:
	/// @brief	Actions that we can transition to, starting any of these will auto-cancel us  
	UPROPERTY(Category=Followups, EditDefaultsOnly, meta=(ShowOnlyInnerProperties))
	TArray<FActionSetEntry> FollowUps;
	
	/// @brief  Akin to followups, except activating these wont end our current action. But the action will auto-end once we end.
	UPROPERTY(Category=Followups, EditDefaultsOnly, meta=(ShowOnlyInnerProperties))
	TArray<FActionSetEntry> Additives;

	virtual TArray<FActionSetEntry> GetFollowups() const override { return FollowUps; }
	virtual TArray<FActionSetEntry> GetAdditives() const override { return Additives; }

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override
	{
		for (int32 Idx = FollowUps.Num()-1; Idx >= 0; Idx--)
		{
			if (FollowUps[Idx].Action && !Cast<UPrimaryActionData>(FollowUps[Idx].Action)) FollowUps[Idx].Action = nullptr;
		}
		for (int32 Idx = Additives.Num()-1; Idx >= 0; Idx--)
		{
			if (Additives[Idx].Action && !Cast<UAdditiveActionData>(Additives[Idx].Action)) Additives[Idx].Action = nullptr;
		}
	}
#endif 
};

#pragma endregion

// NOTE: Set this to non-blueprintable, set Primary & Additive to blueprintable
UCLASS(Abstract, EditInlineNew, DefaultToInstanced, ClassGroup=Actions, Category="Gameplay Actions", DisplayName="Base Gameplay Action")
class ACTIONFRAMEWORK_API UGameplayAction : public UObject
{
	friend class UActionSystemComponent;
	friend class UActionScript;
	friend class FActionSetEntryDetails;
	friend class FGameplayWindow_Actions;
	
	GENERATED_BODY()

protected:

	UGameplayAction()
	{
		static const FName TickFunction = GET_FUNCTION_NAME_CHECKED(UGameplayAction, OnActionTick);
		bActionTicks = GetClass()->IsFunctionImplementedInScript(TickFunction);
	}

	UPROPERTY(VisibleAnywhere)
	EActionCategory ActionCategory;
	
	/// @brief  Essentially the "Name" of this action, e.g Jump has primary action tag [Action.Def.Jump]. Should be set from the constructor. (ScriptReadWrite to allow for default set in AS)
	UPROPERTY(VisibleAnywhere, ScriptReadWrite)
	FGameplayTag PrimaryActionTag;
	
	/// @brief  How the action would be triggered. If null, we'll constantly check on Tick to try activating the action
	UPROPERTY(EditDefaultsOnly, Instanced, meta = (DisplayName = "Trigger", TitleProperty = EditorFriendlyName, ShowOnlyInnerProperties))
	UActionTrigger* ActionTrigger;
	/// @brief  Conditions that must pass for the action to start. We check these first before checking the actions own EnterCondition
	UPROPERTY(EditDefaultsOnly, Instanced, meta = (DisplayName = "Conditions", TitleProperty = EditorFriendlyName, ShowOnlyInnerProperties))
	TArray<UActionCondition*> ActionConditions;
	/// @brief  Various events that can be invoked on the action. These are bound to specific delegate types on the action.
	UPROPERTY(EditDefaultsOnly, Instanced, meta = (DisplayName = "Events", TitleProperty = EditorFriendlyName, ShowOnlyInnerProperties))
	TArray<UActionEvent*> ActionEvents;

public:

	/*--------------------------------------------------------------------------------------------------------------
	* Available Event Delegates To Bind To
	*--------------------------------------------------------------------------------------------------------------*/
	// NOTE: These could be generic events, we don't need to pass in the action to them so they could just be void
	/// @brief  Event that the action has been succesfully activated
	UPROPERTY()
	FActionScriptEvent OnGameplayActionStarted;
	/// @brief  Event that the action has ended.
	UPROPERTY()
	FActionScriptEvent OnGameplayActionEnded;
	/// @brief  Event that the cooldown for this action has started, passes along its cooldown length
	UPROPERTY(Category="Action Events", BlueprintAssignable)
	FOnCoolDownStarted OnCoolDownStarted;
	/// @brief  Event that the cooldown for this action has ended
	UPROPERTY(Category="Action Events", BlueprintAssignable)
	FOnCoolDownEnded OnCoolDownEnded;

	UPROPERTY(Category="Action Events", BlueprintReadWrite)
	FActionAnimPayload AnimPayload;

	/*--------------------------------------------------------------------------------------------------------------
	* UObject Override
	*--------------------------------------------------------------------------------------------------------------*/
	virtual UWorld* GetWorld() const override;

	UFUNCTION(Category="Action Data", BlueprintPure)
	UGameplayActionData* GetActionData() const { return ActionData.Get(); }

	void Cleanup();

//private:
	TSoftObjectPtr<UGameplayActionData> ActionData;

protected:
	
	/*--------------------------------------------------------------------------------------------------------------
	* Rules and Runtime Data
	*--------------------------------------------------------------------------------------------------------------*/

	/// @brief  If true, this action will be ticked. Otherwise, it won't be.
	uint8 bActionTicks				: 1;
	
	/// @brief  Default set to true. Set via SetCanBeCancelled, allowing the action to be ended outside of itself
	UPROPERTY()
	uint8 bIsCancelable				: 1;

private:
	/// @brief  True if the action is currently running
	UPROPERTY(Transient)
	uint8 bIsActive					: 1;

	UPROPERTY(Transient)
	float TimeActivated;
	
protected:

	/*--------------------------------------------------------------------------------------------------------------
	* Owner Information (Character, MovementComp, ActionManager... Or just ActionManager, the rest can be derived from it)
	*--------------------------------------------------------------------------------------------------------------*/
	/// @brief  Shared cached info about the thing using us. Per UE, hopefully allocated
	///			once per actor and shared by many abilities owned by said actor.
	mutable const FActionActorInfo* CurrentActorInfo;

	/// @brief  Cached input requirement field from an input trigger. Could be null/empty
	//UPROPERTY(Transient)
	FActionInputRequirement* InputRequirement;

	/// @brief	Excutes a script event - this is mainly needed for actions implemented in AS
	UFUNCTION(Category="Action | Event", ScriptCallable)
	void ExecuteEvent(const FActionScriptEvent& Event) { Event.Execute(); }

public:
	/*--------------------------------------------------------------------------------------------------------------
	* Accessors
	*--------------------------------------------------------------------------------------------------------------*/
	
	/// @brief  Returns owner as a RadicalCharacter
	UFUNCTION(Category="Action | Info", BlueprintPure)
	ARadicalCharacter* GetRadicalOwner() const { return CurrentActorInfo->CharacterOwner.Get(); }

	/// @brief  Returns movement component of owner
	UFUNCTION(Category="Action | Info", BlueprintPure)
	URadicalMovementComponent* GetMovementComponent() const { return CurrentActorInfo->MovementComponent.Get(); }

	/// @brief  Returns action manager component of owner
	UFUNCTION(Category="Action | Info", BlueprintPure)
	UActionSystemComponent* GetActionManager() const { return CurrentActorInfo->ActionSystemComponent.Get(); }

	const UActionTrigger* GetActionTrigger() const { return ActionTrigger; }
	
	/// @brief	Returns input requirement from an input trigger. Could be null.
	//UFUNCTION(Category="Action | Info", BlueprintPure)
	FActionInputRequirement* GetInputRequirement() const { return InputRequirement; }
	
	/// @brief  Returns time since the action was activated
	UFUNCTION(Category="Action | Info", BlueprintPure)
	float GetTimeInAction() const { return GetWorld() ? GetWorld()->GetTimeSeconds() - TimeActivated : 0.f; }

	/// @brief  True if the action is currently active
	UFUNCTION(Category="Action | Info", BlueprintPure)
	bool IsActive() const { return bIsActive; }
	
protected:
	/*--------------------------------------------------------------------------------------------------------------
	 * Virtual Functions: Update Loop (Perhaps split these up into pure C++ and BPImplementable?
	 *--------------------------------------------------------------------------------------------------------------*/
	
	UFUNCTION(Category="Action | UpdateLoop", BlueprintNativeEvent, DisplayName="OnActionActivated", meta=(ScriptName="OnActionActivated"))
	void OnActionActivated();
	virtual void OnActionActivated_Implementation() {};
	
	UFUNCTION(Category="Action | UpdateLoop", BlueprintNativeEvent, DisplayName="OnActionTick", meta=(ScriptName="OnActionTick"))
	void OnActionTick(float DeltaTime);
	virtual void OnActionTick_Implementation(float DeltaTime) {};
	
	UFUNCTION(Category="Action | UpdateLoop", BlueprintNativeEvent, DisplayName="OnActionEnd", meta=(ScriptName="OnActionEnd"))
	void OnActionEnd();
	virtual void OnActionEnd_Implementation() {};
	
	/*--------------------------------------------------------------------------------------------------------------
	* Virtual Functions: Entry and Exit conditions (Blueprint native is what we want here)
	*--------------------------------------------------------------------------------------------------------------*/
	UFUNCTION(Category="Action | Conditions", BlueprintNativeEvent, DisplayName="EnterCondition", meta=(ScriptName="EnterCondition"))
	bool EnterCondition();
	virtual bool EnterCondition_Implementation() { return true; };

	/*--------------------------------------------------------------------------------------------------------------
	* Virtual Functions: Granting and Removing actions events. Sets up Owner info here
	*--------------------------------------------------------------------------------------------------------------*/

	/// @brief  Called when the action has been granted and instantiated by the ActionManager. Passes its specifier for storage
	virtual void OnActionGranted(const FActionActorInfo* ActorInfo); 
	UFUNCTION(Category="Action | Event Responses", BlueprintImplementableEvent, DisplayName="OnActionGranted")
	void K2_OnActionGranted(); // NOTE: no need to pass through other stuff, it'd be readily accessible by now since this is called from OnGrantAction

	/// @brief  Called when the action has been removed from ActionManager. This is where you'd want to reset transient data.
	virtual void OnActionRemoved(const FActionActorInfo* ActorInfo);
	UFUNCTION(Category="Action | Event Responses", BlueprintImplementableEvent, DisplayName="OnActionRemoved")
	void K2_OnActionRemoved();
	// NOTE: Can add a bool for blueprint implementable events to check if they've actually been implemented and not call the function if not. Avoids going through the BP Graph

	/// @brief  Used internally to just setup some runtime data from above
	virtual void ActionRuntimeSetup() {}
	
public: // NOTE: Add Instigator passthrough for Activate and End ability calls
	/*--------------------------------------------------------------------------------------------------------------
	* Non-Virtual Functions; Activation Shit
	*--------------------------------------------------------------------------------------------------------------*/
	/// @brief  True if the action can be activated, meaning owner tag requirements are satisfied and so is the actions internal condition
	UFUNCTION(Category="Action | Conditions", BlueprintCallable)
	bool CanActivateAction();

	/// @brief  True if the property tags of the ability are satisfied by the ActionManager owner
	UFUNCTION(Category="Action | Conditions", BlueprintCallable)
	bool DoesSatisfyTagRequirements() const;

protected:
	// NOTE: Activate actions through an actionmanager component. EndAction is meant to be called internally, to end an action externally call CancelAction which is public
	/// @brief  Activates the action and applies the appropriate tag. After this call, ActionManager will start ticking this.
	UFUNCTION(Category="Action | Activation", BlueprintCallable)
	void ActivateAction();

	UFUNCTION(Category="Action | Activation", BlueprintCallable, meta=(HidePin="bWasCancelled"))
	void EndAction(bool bWasCancelled = false);

	/*--------------------------------------------------------------------------------------------------------------
	* Non-Virtual Functions; Cancel Action
	*--------------------------------------------------------------------------------------------------------------*/
public:
	/// @brief  Checks if the action can be canceled, if so, calls EndAction
	UFUNCTION(Category="Action | Activation", BlueprintCallable)
	void CancelAction();

	/// @brief  Returns true if bCanCancel is true.
	UFUNCTION(Category="Action | Conditions", BlueprintCallable)
	FORCEINLINE bool CanBeCanceled() const { return bIsCancelable; }

	/// @brief  Set whether or not the action can be cancelled at any given point
	UFUNCTION(Category="Action | Conditions", BlueprintCallable)
	FORCEINLINE void SetCanBeCanceled(bool bCanBeCanceled) { bIsCancelable = bCanBeCanceled; }

	/*--------------------------------------------------------------------------------------------------------------
	* Follow Up & Event Setup
	*--------------------------------------------------------------------------------------------------------------*/

	/// @brief	Cached reference to the action that would have cancelled us (Only relevant for PrimaryActions). Set by the owning ActionSystem
	///			right before it ends us to start the given action. Will be called/setup right before we call OnActionEnd [in case that info is useful]
	UFUNCTION(Category="Action | Info", BlueprintPure)
	FORCEINLINE UGameplayActionData* GetActionThatCancelledUs() const { return ActionThatCancelledUs; }
	
private:

	/// @brief	Cached reference to the action that would have cancelled us (Only relevant for PrimaryActions). Set by the owning ActionSystem
	///			right before it ends us to start the given action. Will be called/setup right before we call OnActionEnd [in case that info is useful]
	UPROPERTY(Transient)
	UGameplayActionData* ActionThatCancelledUs;
	
	/// @brief  Priority of the action automatically setup during initialization from the action-set or followups
	uint32 Priority;
	
	/// @brief  Calls on followups and additives to setup their triggers. Called when this action starts.
	void InitializeFollowupsAndAdditives();
	
	/// @brief  Calls on followups and additives to cleanup their triggers, while also cancelling active additives. Called when this action ends
	void DeinitializeFollowupsAndAdditives();
	
	/// @brief  Called when our action becomes available (either we're a followup or a base action), used to setup triggers
	void SetupActionTrigger(uint32 InPriority);
	
	/// @brief  Called when our action is no longer available (Primarily when the action we're a followup of ends), used to cleanup triggers
	void CleanupActionTrigger();

protected:
#if WITH_EDITORONLY_DATA
	UPROPERTY(VisibleAnywhere, meta=(DisplayPriority=-1))
	mutable FText EditorStatusText;
#endif 
	
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostLoad() override;
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif 

};

/*--------------------------------------------------------------------------------------------------------------
* Action Categories
*--------------------------------------------------------------------------------------------------------------*/

UCLASS(DisplayName="Primary Action", Blueprintable)
class ACTIONFRAMEWORK_API UPrimaryAction : public UGameplayAction
{
	GENERATED_BODY()

	friend class UActionSystemComponent;

protected:

	UPrimaryAction()
	{
		ActionCategory = EActionCategory::Primary;
		
		static FName CalcVelFunction = GET_FUNCTION_NAME_CHECKED(UPrimaryAction, CalcVelocity);
		static FName UpdateRotFunction = GET_FUNCTION_NAME_CHECKED(UPrimaryAction, UpdateRotation);
		
		bRespondToMovementEvents = GetClass()->IsFunctionImplementedInScript(CalcVelFunction);
		bRespondToRotationEvents = GetClass()->IsFunctionImplementedInScript(UpdateRotFunction);
	}
	
	/// @brief  If true, this action will receive CalcVelocity callback from the movement component. Otherwise it wont.
	uint8 bRespondToMovementEvents	: 1;
	/// @brief  If true, this action will receive UpdateRotation callback from the movement component. Otherwise it wont.
	uint8 bRespondToRotationEvents	: 1;

	UFUNCTION(Category="Action | Event Response", BlueprintNativeEvent, DisplayName="CalcVelocity", meta=(ScriptName="CalcVelocity"))
	void CalcVelocity(URadicalMovementComponent* MovementComponent, float DeltaTime);
	virtual void CalcVelocity_Implementation(URadicalMovementComponent* MovementComponent, float DeltaTime) {};
	
	UFUNCTION(Category="Action | Event Response", BlueprintNativeEvent, DisplayName="UpdateRotation", meta=(ScriptName="UpdateRotation"))
	void UpdateRotation(URadicalMovementComponent* MovementComponent, float DeltaTime);
	virtual void UpdateRotation_Implementation(URadicalMovementComponent* MovementComponent, float DeltaTime) {};
};

UCLASS(DisplayName="Additive Action", Blueprintable)
class ACTIONFRAMEWORK_API UAdditiveAction : public UGameplayAction
{
	GENERATED_BODY()

protected:

	UAdditiveAction()
	{
		ActionCategory = EActionCategory::Additive;
	}
};