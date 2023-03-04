// Copyright 2023 Abdulrahmen Almodaimegh. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
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

UCLASS()
class UInputBufferMap : public UDataAsset
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

UCLASS()
class UMotionMappingContext : public UDataAsset
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


UCLASS()
class UMotionAction : public UDataAsset
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


// TODO: Mimic EnhancedInputComponent Event Bindings?
#pragma region Buffer Event Signatures

/*
template<typename TSignature>
struct TInputBufferUnifiedDelegate
{
private:
	TSharedPtr<TSignature> Delegate;

public:

	bool IsBound() const { return Delegate.IsValid() && Delegate->IsBound(); }

	bool IsBoundToObject(void const* Object) const
	{
		return IsBound() && Delegate->IsBoundToObject(Object);
	}

	void Unbind()
	{
		if (Delegate)
		{
			Delegate->Unbind();
		}
	}

	/** Binds a native delegate, hidden for script delegates 
	template<	typename UserClass,
				typename TSig = TSignature,
				typename... TVars>
	void BindDelegate(UserClass* Object, typename TSig::template TMethodPtr<UserClass, TVars...> Func, TVars... Vars)
	{
		Unbind();
		Delegate = MakeShared<TSig>(TSig::CreateUObject(Object, Func, Vars...));
	}

	/** Binds a script delegate on an arbitrary UObject, hidden for native delegates 
	template<	typename TSig = TSignature,
				typename = typename TEnableIf<TIsDerivedFrom<TSig, TScriptDelegate<FWeakObjectPtr> >::IsDerived || TIsDerivedFrom<TSig, TMulticastScriptDelegate<FWeakObjectPtr> >::IsDerived>::Type>
	void BindDelegate(UObject* Object, const FName FuncName)
	{
		Unbind();
		Delegate = MakeShared<TSig>();
		Delegate->BindUFunction(Object, FuncName);
	}

	template<typename TSig = TSignature>
	TSig& MakeDelegate()
	{
		Unbind();
		Delegate = MakeShared<TSig>();
		return *Delegate;
	}

	template<typename... TArgs>
	void Execute(TArgs... Args) const
	{
		if (IsBound())
		{
			Delegate->Execute(Args...);
		}
	}
};

template<typename TSignature>
struct FInputBufferEventBinding : public FInputBindingHandle
{
public:


	void Execute(const FName& ActionID) const;
	TInputBufferUnifiedDelegate<TSignature> Delegate;
};
*/
#pragma endregion Buffer Event Signatures