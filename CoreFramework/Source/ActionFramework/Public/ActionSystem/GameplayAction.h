// 

#pragma once

#include "CoreMinimal.h"
#include "GameplayActionTypes.h"
#include "Engine/World.h"
#include "UObject/Object.h"
#include "GameplayTagContainer.h"
#include "GameplayAction.generated.h"


/* ~~~~~ Forward Declarations ~~~~~ */
class ARadicalCharacter;
class UActionSystemComponent;
class URadicalMovementComponent;
class UInputAction;
struct FActionActorInfo;

/* ~~~~~ Event Definitions ~~~~~ */
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnGameplayActionEnded, class UGameplayAction*, bool);

/* ~~~~~ Helper Macros For Managing ActionDats ~~~~~ */
#define DECLARE_ACTION_DATA(Class) \
	virtual FORCEINLINE UGameplayActionData* GetActionData_Implementation() const override { return ActionData; } \
	virtual FORCEINLINE void SetActionData_Implementation(UGameplayActionData* InActionData) override \
	{ \
		checkf(Cast<Class>(InActionData), TEXT("[%s]: Incompatible Action Data Class Given, Require Data Of Type [%s]"), *StaticClass()->GetName(), *Class::StaticClass()->GetName())\
		ActionData = Cast<Class>(InActionData); \
	}

#define CALC_VEL_false		virtual void CalcVelocity_Implementation(URadicalMovementComponent* MovementComponent, float DeltaTime) override {}
#define CALC_VEL_true		virtual void CalcVelocity_Implementation(URadicalMovementComponent* MovementComponent, float DeltaTime) override;
#define CALC_VEL(Val)		CALC_VEL_ ## Val 
#define UPDATE_ROT_false	virtual void UpdateRotation_Implementation(URadicalMovementComponent* MovementComponent, float DeltaTime) override {}
#define UPDATE_ROT_true		virtual void UpdateRotation_Implementation(URadicalMovementComponent* MovementComponent, float DeltaTime) override;
#define UPDATE_ROT(Val)		UPDATE_ROT_ ## Val
#define ANIM_ACTION_false \
	virtual void PostProcessRMVelocity_Implementation(FVector& Velocity, float DeltaTime) {} \
	virtual void PostProcessRMRotation_Implementation(FVector& Velocity, float DeltaTime) {}
#define ANIM_ACTION_true \
	virtual void PostProcessRMVelocity_Implementation(FVector& Velocity, float DeltaTime);\
	virtual void PostProcessRMRotation_Implementation(FVector& Velocity, float DeltaTime);
#define ANIM_ACTION(Val)	ANIM_ACTION_ ## Val

#define SETUP_ACTION(Class, DataClass, bRequiresCalcVelocity, bRequiresUpdateRot, bPostProcessRootMotion)					\
	Class() { bRespondToMovementEvents = bRequiresCalcVelocity; bRespondToRotationEvents = bRequiresUpdateRot; }			\
	virtual void OnActionActivated_Implementation() override;																\
	virtual void OnActionTick_Implementation(float DeltaTime) override;														\
	virtual void OnActionEnd_Implementation() override;																		\
	CALC_VEL(bRequiresCalcVelocity)																							\
	UPDATE_ROT(bRequiresUpdateRot)																							\
	ANIM_ACTION(bPostProcessRootMotion)																						\
	virtual bool EnterCondition_Implementation() override;																	\
	DECLARE_ACTION_DATA(DataClass);																							\

UCLASS(Abstract, Blueprintable, ClassGroup=Actions, Category="Gameplay Actions", DisplayName="Base Gameplay Action Data")
class ACTIONFRAMEWORK_API UGameplayActionData : public UDataAsset
{
	friend class UGameplayAction;
	friend class UActionSystemComponent;
	
	GENERATED_BODY()

public:
	UPROPERTY(Category=Definition, BlueprintReadOnly)
	const TSubclassOf<UGameplayAction> ActionClass;

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

protected:
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
};

UCLASS(Abstract, Blueprintable, ClassGroup=Actions, Category="Gameplay Actions", DisplayName="Base Gameplay Action")
class ACTIONFRAMEWORK_API UGameplayAction : public UObject
{
	friend class UActionSystemComponent;
	friend class UAction_CoreStateInstance;
	friend class UAction_CoreStateMachineInstance;
	
	GENERATED_BODY()

public:
	/*--------------------------------------------------------------------------------------------------------------
	* Available Event Delegates To Bind To
	*--------------------------------------------------------------------------------------------------------------*/
	/// @brief  Notification that the action has ended. Set using TryActivateAction. Transitions also bind to this.
	FOnGameplayActionEnded OnGameplayActionEnded;

	/*--------------------------------------------------------------------------------------------------------------
	* UObject Override
	*--------------------------------------------------------------------------------------------------------------*/
	virtual UWorld* GetWorld() const override;

	UFUNCTION(Category="Action Data", BlueprintNativeEvent, BlueprintPure)
	UGameplayActionData* GetActionData() const;
	virtual FORCEINLINE UGameplayActionData* GetActionData_Implementation() const PURE_VIRTUAL(UGameplayAction::GetActionData, return nullptr; );
	UFUNCTION(Category="Action Data", BlueprintNativeEvent, BlueprintCallable)
	void SetActionData(UGameplayActionData* InActionData);
	virtual FORCEINLINE void SetActionData_Implementation(UGameplayActionData* InActionData) PURE_VIRTUAL(UGameplayAction::SetActionData, );

protected:
	
	/*--------------------------------------------------------------------------------------------------------------
	* Rules and Runtime Data
	*--------------------------------------------------------------------------------------------------------------*/
	// NOTE: THE MOVEMENT EVENT FLAGS MUST BE SET IF YOU WANT TO USE THEM
	
	/// @brief  If true, this action will receive CalcVelocity callback from the movement component. Otherwise it wont.
	UPROPERTY(Category=Advanced, EditDefaultsOnly)
	uint8 bRespondToMovementEvents	: 1;
	/// @brief  If true, this action will receive UpdateRotation callback from the movement component. Otherwise it wont.
	UPROPERTY(Category=Advanced, EditDefaultsOnly)
	uint8 bRespondToRotationEvents	: 1;

	// NOTE: Wanna think through the root motion event some more, should only PP the one started by us here.

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

public:
	/*--------------------------------------------------------------------------------------------------------------
	* Accessors
	*--------------------------------------------------------------------------------------------------------------*/

	/// @brief  Returns owner as a RadicalCharacter
	UFUNCTION(Category="Action | Info", BlueprintCallable)
	ARadicalCharacter* GetRadicalOwner() const { return CurrentActorInfo->CharacterOwner.Get(); }

	/// @brief  Returns movement component of owner
	UFUNCTION(Category="Action | Info", BlueprintCallable)
	URadicalMovementComponent* GetMovementComponent() const { return CurrentActorInfo->MovementComponent.Get(); }

	/// @brief  Returns action manager component of owner
	UFUNCTION(Category="Action | Info", BlueprintCallable)
	UActionSystemComponent* GetActionManager() const { return CurrentActorInfo->ActionSystemComponent.Get(); }
	
	/// @brief  Returns time since the action was activated
	UFUNCTION(Category="Action | Info", BlueprintCallable)
	float GetTimeInAction() const { return GetWorld() ? GetWorld()->GetTimeSeconds() - TimeActivated : 0.f; }

	/// @brief  True if the action is currently active
	UFUNCTION(Category="Action | Info", BlueprintCallable)
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
	* Virtual Functions: Movement Component Events (Called via ActionManagerComp which is what is bound to them)
	*--------------------------------------------------------------------------------------------------------------*/
	// NOTE: CPP only for now, PostProcessRM is safe to open to blueprint though (once per tick). Will get back to it later.
	UFUNCTION(Category="Action | Event Response", BlueprintNativeEvent, DisplayName="CalcVelocity", meta=(ScriptName="CalcVelocity"))
	void CalcVelocity(URadicalMovementComponent* MovementComponent, float DeltaTime);
	virtual void CalcVelocity_Implementation(URadicalMovementComponent* MovementComponent, float DeltaTime) {};
	
	UFUNCTION(Category="Action | Event Response", BlueprintNativeEvent, DisplayName="UpdateRotation", meta=(ScriptName="UpdateRotation"))
	void UpdateRotation(URadicalMovementComponent* MovementComponent, float DeltaTime);
	virtual void UpdateRotation_Implementation(URadicalMovementComponent* MovementComponent, float DeltaTime) {};

	
	UFUNCTION(Category="Action | Event Response", BlueprintNativeEvent)
	void PostProcessRMVelocity(FVector& Velocity, float DeltaTime);
	virtual void PostProcessRMVelocity_Implementation(FVector& Velocity, float DeltaTime) {};
	
	UFUNCTION(Category="Action | Event Response", BlueprintNativeEvent)
	void PostProcessRMRotation(FQuat& RootMotionRotation, float DeltaTime);
	virtual void PostProcessRMRotation_Implementation(FQuat& RootMotionRotation, float DeltaTime) {};
	
	
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
	virtual void OnActionGranted(const FActionActorInfo* ActorInfo); // TODO: Set owner stuff
	UFUNCTION(Category="Action | Event Responses", BlueprintImplementableEvent, DisplayName="OnActionGranted")
	void K2_OnActionGranted(); // NOTE: no need to pass through other stuff, it'd be readily accessible by now since this is called from OnGrantAction

	/// @brief  Called when the action has been removed from ActionManager. This is where you'd want to reset transient data.
	virtual void OnActionRemoved(const FActionActorInfo* ActorInfo);
	UFUNCTION(Category="Action | Event Responses", BlueprintImplementableEvent, DisplayName="OnActionRemoved")
	void K2_OnActionRemoved();
	// NOTE: Can add a bool for blueprint implementable events to check if they've actually been implemented and not call the function if not. Avoids going through the BP Graph
	
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

//public: Keeping this protected
	/// @brief  Input that instigated this action, set from wherever relevant (e.g a state)
	UPROPERTY(Transient)
	UInputAction* InputInstigator;
	/// @brief  Auto bound to the Release of the activation input if the activation input was a button
	UFUNCTION()
	virtual void ButtonInputReleased(float ElapsedTime) {};

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

};
