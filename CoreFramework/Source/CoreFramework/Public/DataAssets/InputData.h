// Copyright 2023 Abdulrahmen Almodaimegh. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "InputAction.h"
#include "InputData.generated.h"

class UInputMappingContext;
class UMotionMappingContext;
class UMotionAction;

#define DOT_PRODUCT_0   (1.f)
#define DOT_PRODUCT_15  (0.966f)
#define DOT_PRODUCT_30  (0.866f)
#define DOT_PRODUCT_45  (0.707f)
#define DOT_PRODUCT_60  (0.5f)
#define DOT_PRODUCT_75  (0.259f)
#define DOT_PRODUCT_90  (0.f)
#define DOT_PRODUCT_105 (-0.259f)
#define DOT_PRODUCT_120 (-0.5f)
#define DOT_PRODUCT_135 (-0.707f)
#define DOT_PRODUCT_150 (-0.866f)
#define DOT_PRODUCT_165 (-0.966f)
#define DOT_PRODUCT_180 (-1.f)


UCLASS(ClassGroup="Input Buffer", Category="Input Buffer")
class COREFRAMEWORK_API UBufferedInputAction : public UInputAction
{
	GENERATED_BODY()

	friend class UInputBufferSubsystem;

public:

	FORCEINLINE FGameplayTag GetID() const { return InputTag; }
	
protected:

	UPROPERTY(Category="Description", EditDefaultsOnly, BlueprintReadOnly, meta=(GameplayTagFilter="Input"))
	FGameplayTag InputTag;

};

#pragma region General Definitions

UCLASS(ClassGroup="Input Buffer", Category="Input Buffer")
class COREFRAMEWORK_API UInputBufferMap : public UDataAsset
{
	GENERATED_BODY()

public:
	// Localized context descriptor
	UPROPERTY(Category = "Description", BlueprintReadOnly, EditAnywhere, DisplayName = "Description")
	FText ContextDescription;

	/// @brief  Input Action Mappings associated with this input buffer data map
	UPROPERTY(Category="Mappings", BlueprintReadOnly, EditAnywhere)
	TObjectPtr<UInputMappingContext> InputActionMap = nullptr;

	/// @brief  Directional Input Action Mappings associated with this input buffer data map
	UPROPERTY(Category="Mappings", BlueprintReadOnly, EditAnywhere)
	TObjectPtr<UMotionMappingContext> DirectionalActionMap = nullptr;

public:
	void GenerateInputActions();
	TArray<TObjectPtr<const UBufferedInputAction>> GetInputActions() const { return InputActions; }
	FGameplayTagContainer GetActionIDs();
	FGameplayTagContainer GetDirectionalIDs();

private:
	UPROPERTY(Transient)
	FGameplayTagContainer ActionIDs;
	
	UPROPERTY(Transient)
	FGameplayTagContainer DirectionalIDs;

	UPROPERTY(Transient)
	TArray<TObjectPtr<const UBufferedInputAction>> InputActions;
};



#pragma endregion General Definitions

#pragma region Directional Input Definitions

UCLASS(ClassGroup="Input Buffer", Category="Input Buffer")
class COREFRAMEWORK_API UMotionMappingContext : public UDataAsset
{
	GENERATED_BODY()

public:
	FORCEINLINE TArray<TObjectPtr<UMotionAction>> GetMappings() { return Mapping; }

	FORCEINLINE FGameplayTag GetDirectionalActionID() const
	{
		return DirectionalAction->GetID();
	}
	
public:
	/// @brief Localized context descriptor
	UPROPERTY(Category="Description", BlueprintReadOnly, EditAnywhere, DisplayName="Description")
	FText ContextDescription;
	
protected:
	/// @brief  Axis InputAction you want to use to evaluate directional input
	UPROPERTY(Category= "Mappings", BlueprintReadOnly, EditAnywhere)
	TObjectPtr<UBufferedInputAction> DirectionalAction;
	
	/// @brief Holds a collection of motion actions to define the maps motion action map
	UPROPERTY(Category= "Mappings", BlueprintReadOnly, EditAnywhere)
	TArray<TObjectPtr<UMotionAction>> Mapping;
};

#pragma region Enum Primitives

/* Primitive to define various motion commands */
UENUM()
enum EMotionCommandDirection
{
	NEUTRAL, FORWARD, BACK, LEFT, RIGHT
};

UENUM()
enum EFacingDirection
{
	FROM_FORWARD, FROM_BACK, FROM_LEFT, FROM_RIGHT
};

UENUM()
enum ETurnDirection
{
	CW, CCW, EITHER
};

#pragma endregion Enum Primitives


UCLASS(ClassGroup="Input Buffer", Category="Input Buffer")
class COREFRAMEWORK_API UMotionAction : public UDataAsset
{
	GENERATED_BODY()

	friend class UInputBufferSubsystem;
	
protected:

	UPROPERTY(Category="Description", EditDefaultsOnly, BlueprintReadOnly, meta=(GameplayTagFilter="Input.Directional"))
	FGameplayTag InputTag;
	
	UPROPERTY(Category="Description", EditDefaultsOnly, BlueprintReadOnly, DisplayName="Action Name")
	FName ActionName{"None"};

	/// @brief Whether to examine an angle change instead of just evaluating directions. Can define more detailed directional input
	UPROPERTY(Category="Motion Command", EditDefaultsOnly, BlueprintReadOnly)
	bool bAngleChange;

	/// @brief Whether to check the input direction relative to the player, or just evaluate the raw direction values
	UPROPERTY(Category="Motion Command", EditDefaultsOnly, BlueprintReadOnly, meta=(EditCondition="!bAngleChange", EditConditionHides))
	bool bRelativeToPlayer;
	
	/// @brief Sequence of directions defining this motion command
	UPROPERTY(Category="Motion Command", EditDefaultsOnly, BlueprintReadOnly, meta=(EditCondition="!bAngleChange", EditConditionHides))
	TArray<TEnumAsByte<EMotionCommandDirection>> MotionCommandSequence;
	
	/// @brief If evaluating angle change, begin checking the angle delta from which direction
	UPROPERTY(Category="Motion Command", EditDefaultsOnly, BlueprintReadOnly, meta=(EditCondition="bAngleChange", EditConditionHides))
	TEnumAsByte<EFacingDirection> FromDirection;

	/// @brief If evaluating angle change, in which direction should the turn be evaluated
	UPROPERTY(Category="Motion Command", EditDefaultsOnly, BlueprintReadOnly, meta=(EditCondition="bAngleChange", EditConditionHides))
	TEnumAsByte<ETurnDirection> TurnDirection;

	/// @brief If evaluating angle change, how big of an angle delta to check
	UPROPERTY(Category="Motion Command", EditDefaultsOnly, BlueprintReadOnly, meta=(EditCondition="bAngleChange", EditConditionHides))
	int AngleDelta{0};

private: // Vars to keep track of current directional input state

	/// @brief Current Step In The Command Sequence To Check
	UPROPERTY(Transient)
	int CheckStep{0};

	/// @brief Previous Registered Angle Delta
	UPROPERTY(Transient)
	float PrevAngle{0};

	/// @brief Current Registered Angle Delta
	UPROPERTY(Transient)
	float CurAngle{0};

	/// @brief  Previous input vector, used to determine whether CW or CCW
	UPROPERTY(Transient)
	FVector2D PrevVector;

	/// @brief  Set to true when stick input is zero
	UPROPERTY(Transient)
	bool bAngleChangeInitialized;

	/// @brief  Set to true when stick input aligns with FromDirection (to happen after bAngleChangeInitialized)
	UPROPERTY(Transient)
	bool bInputAlignmentInitialized;
	
public:

	FGameplayTag GetID() const { return InputTag; }

	// NOTE: We wanna pass in the RAW axis input here. We then dictate which basis to evaluate it against
	
	bool CheckMotionDirection(const FVector2D& AxisInput, const FVector& ProcessedInput, const FVector& PlayerForward, const FVector& PlayerRight);

	bool IsAlignedWithFacingDirection(const FVector2D& AxisInput) const;

	void UpdateCurrentAngle(const FVector2D& AxisInput);
	
	EMotionCommandDirection GetAxisDirection(const FVector2D& AxisInput, const FVector& ProcessedInput, const FVector& PlayerForward, const FVector& PlayerRight) const;

	FORCEINLINE FVector2D GetFromDirection() const
	{
		switch (FromDirection)
		{
			case EFacingDirection::FROM_FORWARD:
				return FVector2D(0,1);
			case EFacingDirection::FROM_BACK:
				return FVector2D(0,-1);
			case EFacingDirection::FROM_RIGHT:
				return FVector2D(1,0);
			case EFacingDirection::FROM_LEFT:
				return FVector2D(-1,0);
			default:
				return FVector2D::ZeroVector;
		}
	}

	FORCEINLINE bool ProperRotation(const FVector2D& AxisInput) const
	{
		// If value is negative, going CW, otherwise going CCW
		const float CWorCCW = PrevVector.X * AxisInput.Y - PrevVector.Y * AxisInput.X;

		switch (TurnDirection)
		{
			case ETurnDirection::CW: return CWorCCW < 0;
			case ETurnDirection::CCW: return CWorCCW > 0;
			case ETurnDirection::EITHER: return true;
		}

		return false;
	}
	
	FORCEINLINE void Reset() 
	{
		PrevAngle = 0;
		CurAngle = 0;
		CheckStep = 0;
		bAngleChangeInitialized = false;
		bInputAlignmentInitialized = false;
		PrevVector = FVector2D::ZeroVector;
	}
	
};

#pragma endregion Directional Input Definitions


#pragma region Buffer Event Signatures

// Input Action Registration
DECLARE_DYNAMIC_DELEGATE_TwoParams(FInputActionEventSignature, const FInputActionValue&, Value, float, ElapsedTime);

// Input Action Sequence
DECLARE_DYNAMIC_DELEGATE_TwoParams(FInputActionSequenceSignature, const FInputActionValue&, ValueOne, const FInputActionValue&, ValueTwo);

// Directional Action Registration
DECLARE_DYNAMIC_DELEGATE(FDirectionalActionSignature);

// Directional Action & Input Action Serquence
DECLARE_DYNAMIC_DELEGATE_TwoParams(FDirectionalAndActionSequenceSignature, const FInputActionValue&, ActionValue, float, ActionElapsedTime);

UENUM()
enum EBufferTriggerEvent
{
	TRIGGER_Press	UMETA(DisplayName="Pressed"),
	TRIGGER_Hold	UMETA(DisplayName="Hold"),
	TRIGGER_Release	UMETA(DisplayName="Release")
};

UENUM()
enum EDirectionalSequenceOrder
{
	SEQUENCE_None				UMETA(DisplayName="Doesn't Matter"),
	SEQUENCE_DirectionalFirst	UMETA(DisplayName="Directional First"),
	SEQUENCE_ButtonFirst		UMETA(DisplayName="Button First")
};

USTRUCT()
struct COREFRAMEWORK_API FInputActionDelegateHandle
{
	GENERATED_BODY()

	friend class UInputBufferSubsystem;

	FInputActionDelegateHandle() : TriggerType(EBufferTriggerEvent::TRIGGER_Press), bAutoConsume(false), Priority(0)
	{}
	
	FInputActionDelegateHandle(const FGameplayTag& InAction, EBufferTriggerEvent SetTriggerEvent, const bool bSetAutoConsume, const int InPriority)
	: InputAction(InAction), TriggerType(SetTriggerEvent), bAutoConsume(bSetAutoConsume), Priority(InPriority)
	{}

protected:
	UPROPERTY()
	FGameplayTag InputAction;
	UPROPERTY()
	TEnumAsByte<EBufferTriggerEvent> TriggerType;
	UPROPERTY()
	bool bAutoConsume;
	UPROPERTY()
	int Priority;
};

USTRUCT()
struct COREFRAMEWORK_API FInputActionSequenceDelegateHandle
{
	GENERATED_BODY()

	friend class UInputBufferSubsystem;

	FInputActionSequenceDelegateHandle() : bFirstInputIsHold(false), bButtonOrderMatters(false), bAutoConsume(false), Priority(0)
	{}
	
	FInputActionSequenceDelegateHandle(const FGameplayTag& ActionOne, const FGameplayTag& ActionTwo, bool bSetAutoConsume, bool bSetFirstInputIsHold, bool bSetOrderMatters, const int InPriority)
	: FirstAction(ActionOne), SecondAction(ActionTwo), bFirstInputIsHold(bSetFirstInputIsHold), bButtonOrderMatters(bSetOrderMatters), bAutoConsume(bSetAutoConsume), Priority(InPriority)
	{}

protected:
	UPROPERTY()
	FGameplayTag FirstAction;
	UPROPERTY()
	FGameplayTag SecondAction;
	/// @brief	Whether the first Action should be a 'Hold' modifier, meaning event is triggered only if SecondAction is pressed while first is being held. Order automatically does not matter in this case and only second input is consumed
	UPROPERTY()
	bool bFirstInputIsHold;
	UPROPERTY()
	bool bButtonOrderMatters;
	UPROPERTY()
	bool bAutoConsume;
	UPROPERTY()
	int Priority;
};

USTRUCT()
struct COREFRAMEWORK_API FDirectionalActionDelegateHandle
{
	GENERATED_BODY()

	friend class UInputBufferSubsystem;
	
	FDirectionalActionDelegateHandle() : bAutoConsume(false), Priority(0) {}
	
	FDirectionalActionDelegateHandle(const FGameplayTag& Directional, const bool bSetAutoConsume, const int InPriority)
	: DirectionalAction(Directional), bAutoConsume(bSetAutoConsume), Priority(InPriority)
	{}

protected:
	UPROPERTY()
	FGameplayTag DirectionalAction;
	UPROPERTY()
	bool bAutoConsume;
	UPROPERTY()
	int Priority;
};

USTRUCT()
struct COREFRAMEWORK_API FDirectionalAndActionDelegateHandle
{
	GENERATED_BODY()

	friend class UInputBufferSubsystem;

	FDirectionalAndActionDelegateHandle() : bAutoConsume(false), SequenceOrder(SEQUENCE_None), Priority(0)
	{}
	
	FDirectionalAndActionDelegateHandle(const FGameplayTag& Action, const FGameplayTag& Directional, bool bSetAutoConsume, EDirectionalSequenceOrder Sequence, const int InPriority)
	: InputAction(Action), DirectionalAction(Directional), bAutoConsume(bSetAutoConsume), SequenceOrder(Sequence), Priority(InPriority)
	{}

protected:
	UPROPERTY()
	FGameplayTag InputAction;
	UPROPERTY()
	FGameplayTag DirectionalAction;
	UPROPERTY()
	bool bAutoConsume;
	UPROPERTY()
	TEnumAsByte<EDirectionalSequenceOrder> SequenceOrder;
	UPROPERTY()
	int Priority;
};

template<typename DelType>
struct COREFRAMEWORK_API FInputBufferBinding
{
	FInputBufferBinding()
	{
		//DelTest = MakeShared<DelType>();
		//Delegate = DelType();
		static int32 GlobalHandle = 0;
		Handle = GlobalHandle++;
	}

	bool operator==(const FInputBufferBinding& Other) const
	{
		return Handle == Other.Handle;
	}

	bool operator!=(const FInputBufferBinding& Other) const
	{
		return Handle != Other.Handle;
	}

	friend uint32 GetTypeHash(const FInputBufferBinding& InHandle)
	{
		return InHandle.Handle;
	}

	//TSharedPtr<DelType> DelTest;
	DelType Delegate;

	int32 Handle;
};

// Typedefs
typedef FInputBufferBinding<FInputActionEventSignature> FButtonBinding;
typedef FInputBufferBinding<FInputActionSequenceSignature> FButtonSequenceBinding;
typedef FInputBufferBinding<FDirectionalActionSignature> FDirectionalBinding;
typedef FInputBufferBinding<FDirectionalAndActionSequenceSignature> FDirectionalSequenceBinding;

#pragma endregion Buffer Event Signatures