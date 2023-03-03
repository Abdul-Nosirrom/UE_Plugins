// Copyright 2023 Abdulrahmen Almodaimegh. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "InputData.generated.h"

class UMotionMappingContext;
class UMotionAction;

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
	TArray<FName> GetActionIDs();
	TArray<FName> GetDirectionalIDs();

private:
	UPROPERTY(Transient)
	TArray<FName> ActionIDs;
	
	UPROPERTY(Transient)
	TArray<FName> DirectionalIDs;
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
	CW, CCW
};

#pragma endregion Enum Primitives


UCLASS()
class UMotionAction : public UDataAsset
{
	GENERATED_BODY()
	
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

	/// @brief  Set to true when stick input aligns with FromDirection, used to track angle changes
	UPROPERTY(Transient)
	bool bAngleChangeInitialized;
	
public:

	FName GetID() const { return ActionName; }

	// NOTE: We wanna pass in the RAW axis input here. We then dictate which basis to evaluate it against
	
	bool CheckMotionDirection(const FVector2D& AxisInput);

	void UpdateCurrentAngle(const FVector2D& AxisInput);
	
	EMotionCommandDirection GetAxisDirection(const FVector2D& AxisInput);

	FORCEINLINE void Reset() 
	{
		CurAngle = 0;
		CheckStep = 0;
	}
	
};

#pragma endregion Directional Input Definitions