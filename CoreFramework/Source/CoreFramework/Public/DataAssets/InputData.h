// Copyright 2023 Abdulrahmen Almodaimegh. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "InputAction.h"
#include "InputData.generated.h"

class UMotionMappingContext;
class UMotionAction;

namespace EvalCache
{
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
}

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
	TArray<TObjectPtr<const UInputAction>> GetInputActions() const { return InputActions; }
	TArray<FName> GetActionIDs();
	TArray<FName> GetDirectionalIDs();

private:
	UPROPERTY(Transient)
	TArray<FName> ActionIDs;
	
	UPROPERTY(Transient)
	TArray<FName> DirectionalIDs;

	UPROPERTY(Transient)
	TArray<TObjectPtr<const UInputAction>> InputActions;
};



#pragma endregion General Definitions

#pragma region Directional Input Definitions

UCLASS(ClassGroup="Input Buffer", Category="Input Buffer")
class COREFRAMEWORK_API UMotionMappingContext : public UDataAsset
{
	GENERATED_BODY()

public:
	FORCEINLINE TArray<TObjectPtr<UMotionAction>> GetMappings() { return Mapping; }

	FORCEINLINE FName GetDirectionalActionID() const
	{
		return FName(DirectionalAction->ActionDescription.ToString());
	}
	
public:
	/// @brief Localized context descriptor
	UPROPERTY(Category="Description", BlueprintReadOnly, EditAnywhere, DisplayName="Description")
	FText ContextDescription;
	
protected:
	/// @brief  Axis InputAction you want to use to evaluate directional input
	UPROPERTY(Category= "Mappings", BlueprintReadOnly, EditAnywhere)
	TObjectPtr<UInputAction> DirectionalAction;
	
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

	FName GetID() const { return ActionName; }

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

	FInputActionDelegateHandle() : InputAction(NAME_None), TriggerType(EBufferTriggerEvent::TRIGGER_Press), bAutoConsume(false), Priority(0)
	{}
	
	FInputActionDelegateHandle(const FName InAction, EBufferTriggerEvent SetTriggerEvent, const bool bSetAutoConsume, const int InPriority)
	: InputAction(InAction), TriggerType(SetTriggerEvent), bAutoConsume(bSetAutoConsume), Priority(InPriority)
	{}

protected:
	UPROPERTY()
	FName InputAction;
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

	FInputActionSequenceDelegateHandle() : FirstAction(NAME_None), SecondAction(NAME_None), bButtonOrderMatters(false), bAutoConsume(false), Priority(0)
	{}
	
	FInputActionSequenceDelegateHandle(const FName ActionOne, const FName ActionTwo, bool bSetAutoConsume, bool bSetOrderMatters, const int InPriority)
	: FirstAction(ActionOne), SecondAction(ActionTwo), bButtonOrderMatters(bSetOrderMatters), bAutoConsume(bSetAutoConsume), Priority(InPriority)
	{}

protected:
	UPROPERTY()
	FName FirstAction;
	UPROPERTY()
	FName SecondAction;
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
	
	FDirectionalActionDelegateHandle() : DirectionalAction(NAME_None), bAutoConsume(false), Priority(0) {}
	
	FDirectionalActionDelegateHandle(const FName Directional, const bool bSetAutoConsume, const int InPriority)
	: DirectionalAction(Directional), bAutoConsume(bSetAutoConsume), Priority(InPriority)
	{}

protected:
	UPROPERTY()
	FName DirectionalAction;
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

	FDirectionalAndActionDelegateHandle() : InputAction(NAME_None), DirectionalAction(NAME_None), bAutoConsume(false), SequenceOrder(SEQUENCE_None), Priority(0)
	{}
	
	FDirectionalAndActionDelegateHandle(const FName Action, const FName Directional, bool bSetAutoConsume, EDirectionalSequenceOrder Sequence, const int InPriority)
	: InputAction(Action), DirectionalAction(Directional), bAutoConsume(bSetAutoConsume), SequenceOrder(Sequence), Priority(InPriority)
	{}

protected:
	UPROPERTY()
	FName InputAction;
	UPROPERTY()
	FName DirectionalAction;
	UPROPERTY()
	bool bAutoConsume;
	UPROPERTY()
	TEnumAsByte<EDirectionalSequenceOrder> SequenceOrder;
	UPROPERTY()
	int Priority;
};

#pragma endregion Buffer Event Signatures