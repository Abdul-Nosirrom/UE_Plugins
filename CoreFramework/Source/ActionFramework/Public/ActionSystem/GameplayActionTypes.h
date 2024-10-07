// 

#pragma once

#include "CoreMinimal.h"
#include "GameplayActionTypes.generated.h"

class ARadicalCharacter;
class UActionSystemComponent;
class UAttributeSystemComponent;
class URadicalMovementComponent;
class UAnimInstance;
class USkeletalMeshComponent;

#pragma region MACROS

/* Macros */

#define TICKS_false			virtual void OnActionTick_Implementation(float DeltaTime) override {}
#define TICKS_true			virtual void OnActionTick_Implementation(float DeltaTime) override;
#define TICKS(Val)			TICKS_ ## Val

#define CALC_VEL_false		virtual void CalcVelocity_Implementation(URadicalMovementComponent* MovementComponent, float DeltaTime) override {}
#define CALC_VEL_true		virtual void CalcVelocity_Implementation(URadicalMovementComponent* MovementComponent, float DeltaTime) override;
#define CALC_VEL(Val)		CALC_VEL_ ## Val

#define UPDATE_ROT_false	virtual void UpdateRotation_Implementation(URadicalMovementComponent* MovementComponent, float DeltaTime) override {}
#define UPDATE_ROT_true		virtual void UpdateRotation_Implementation(URadicalMovementComponent* MovementComponent, float DeltaTime) override;
#define UPDATE_ROT(Val)		UPDATE_ROT_ ## Val

#define SETUP_PRIMARY_ACTION(ActionTag, bShouldTick, bCalcVel, bUpdateRot)		\
	virtual void ActionRuntimeSetup() override									\
	{																			\
		ActionData->SetActionTag(ActionTag);									\
		bActionTicks				= bShouldTick;								\
		bRespondToMovementEvents	= bCalcVel;									\
		bRespondToRotationEvents	= bUpdateRot;								\
	}																			\
	virtual void PostLoad() override											\
	{																			\
		Super::PostLoad();														\
		PrimaryActionTag = ActionTag;											\
	}																			\
	virtual void OnActionActivated_Implementation() override;					\
	TICKS(bShouldTick)															\
	virtual void OnActionEnd_Implementation() override;							\
	CALC_VEL(bCalcVel)															\
	UPDATE_ROT(bUpdateRot)														\
	virtual bool EnterCondition_Implementation() override;						\

#define SETUP_ADDITIVE_ACTION(ActionTag, bShouldTick)							\
	virtual void ActionRuntimeSetup() override									\
	{																			\
		ActionData->SetActionTag(ActionTag);									\
		bActionTicks				= bShouldTick;								\
	}																			\
	virtual void PostLoad() override											\
	{																			\
		Super::PostLoad();														\
		PrimaryActionTag = ActionTag;											\
	}																			\
	virtual void OnActionActivated_Implementation() override;					\
	TICKS(bShouldTick)															\
	virtual void OnActionEnd_Implementation() override;							\
	virtual bool EnterCondition_Implementation() override;						\

#pragma endregion

/* Actor Info Class */
USTRUCT(BlueprintType)
struct ACTIONFRAMEWORK_API FActionActorInfo
{
	GENERATED_BODY()

	virtual ~FActionActorInfo() {}
	
	// NOTE: May change these to TWeakObjectPtr later

	UPROPERTY(Category=ActorInfo, BlueprintReadOnly)
	TWeakObjectPtr<AActor> OwnerActor;
	
	UPROPERTY(Category=ActorInfo, BlueprintReadOnly)
	TWeakObjectPtr<ARadicalCharacter> CharacterOwner;

	UPROPERTY(Category=ActorInfo, BlueprintReadOnly)
	TWeakObjectPtr<UActionSystemComponent> ActionSystemComponent;

	UPROPERTY(Category=ActorInfo, BlueprintReadOnly)
	TWeakObjectPtr<UAttributeSystemComponent> AttributeSystemComponent;

	UPROPERTY(Category=ActorInfo, BlueprintReadOnly)
	TWeakObjectPtr<URadicalMovementComponent> MovementComponent;

	UPROPERTY(Category=ActorInfo, BlueprintReadOnly)
	TWeakObjectPtr<USkeletalMeshComponent> SkeletalMeshComponent;

	virtual void InitFromCharacter(ARadicalCharacter* Character, UActionSystemComponent* InActionSystemComponent);

	virtual void InitFromActor(AActor* InOwnerActor);
};

/*~~~~~ Helper Data Structs ~~~~~*/
/// @brief Struct containing arbitrary data that can be passed of to animations (e.g for a wall run, Scalar = -1/1 depending on left/right wall)
USTRUCT(BlueprintType)
struct FActionAnimPayload
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite)
	bool bActionBool;
	UPROPERTY(BlueprintReadWrite)
	float ActionScalar;
	UPROPERTY(BlueprintReadWrite)
	FVector ActionVector;
};

USTRUCT()
struct FActionDataID
{
	GENERATED_BODY()

	FActionDataID()
	{
		static int32 GlobalHandle = 0;
		GlobalHandle++;
		ID = GlobalHandle;
	}
	
	uint32 ID;
};

UENUM()
enum class EActionCategory
{
	/// @brief  Only one primary action can be running at a given time
	Primary,
	/// @brief  Passives continue to run throughout, they're usually associated with a primary action
	Passive,
	/// @brief	Multiple additive actions can be running at a given time  
	Additive
};

/*--------------------------------------------------------------------------------------------------------------
* Action Event System: An event that action scripts can bind to through drop down menus, and a wrapper for it for UE Reflection
*--------------------------------------------------------------------------------------------------------------*/

class UGameplayAction;

DECLARE_MULTICAST_DELEGATE(FGameplayActionEvent);

USTRUCT(BlueprintType)
struct ACTIONFRAMEWORK_API FActionScriptEvent
{
	GENERATED_BODY()

	FGameplayActionEvent Event;

	void Execute() const { Event.Broadcast(); }
};

USTRUCT(BlueprintType)
struct ACTIONFRAMEWORK_API FActionEventWrapper
{
	GENERATED_BODY()

	friend class FActionScriptEventDetails;
	friend class SActionEventListWidget;

	FActionEventWrapper() : Event(nullptr) {}

	bool IsValid() const { return Event != nullptr; }
	
	/// @brief  Get the selected event that exists on this action
	FActionScriptEvent* GetEvent(UGameplayAction* Action);

#if WITH_EDITORONLY_DATA
	/** Custom serialization */
	void PostSerialize(const FArchive& Ar);
#endif 
	
private:

	static void GetAllEventProperties(const UClass* TargetClass, TArray<FProperty*>& OutProperties, FString FilterMetaStr);

	friend class FActionScriptEventPropertyDetails;

	UPROPERTY(Category="Event", EditAnywhere)
	TFieldPath<FProperty> Event;

	//UClass* ActionClass;
};
