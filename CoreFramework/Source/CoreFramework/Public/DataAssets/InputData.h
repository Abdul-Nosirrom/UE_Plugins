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
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Description", DisplayName = "Description")
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
};



#pragma endregion General Definitions

#pragma region Directional Input Definitions

UCLASS()
class UMotionMappingContext : public UDataAsset
{
	GENERATED_BODY()

public:
	TArray<TObjectPtr<UMotionAction>> GetMappings() { return Mapping; }
	
public:
	/// @brief Localized context descriptor
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category="Description", DisplayName="Description")
	FText ContextDescription;
	
protected:
	/// @brief Holds a collection of motion actions to define the maps motion action map
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category= "Mappings")
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

// TODO: This should be a data asset similar to ActionMaps, much better organization than tacking it onto the controller
UCLASS()
class UMotionAction : public UDataAsset
{
	GENERATED_BODY()
	
protected:

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Description", DisplayName="Action Name")
	FName ActionName{"None"};

	/// @brief Whether to check the input direction relative to the player, or just evaluate the raw direction values
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Motion Command")
	bool bRelativeToPlayer;

	/// @brief Sequence of directions defining this motion command
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Motion Command", meta=(EditCondition="!bAngleChange", EditConditionHides))
	TArray<TEnumAsByte<EMotionCommandDirection>> MotionCommandSequence;

	/// @brief Whether to examine an angle change instead of just evaluating directions. Can define more detailed directional input
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Motion Command")
	bool bAngleChange;

	/// @brief If evaluating angle change, begin checking the angle delta from which direction
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Motion Command", meta=(EditCondition="bAngleChange", EditConditionHides))
	TEnumAsByte<EFacingDirection> FromDirection;

	/// @brief If evaluating angle change, in which direction should the turn be evaluated
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Motion Command", meta=(EditCondition="bAngleChange", EditConditionHides))
	TEnumAsByte<ETurnDirection> TurnDirection;

	/// @brief If evaluating angle change, how big of an angle delta to check
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Motion Command", meta=(EditCondition="bAngleChange", EditConditionHides))
	int AngleDelta{0};

	/// @brief Current Step In The Command Sequence To Check
	int CheckStep{0};

	/// @brief Previous Registered Angle Delta
	float PrevAngle;

	/// @brief Current Registered Angle Delta
	float CurAngle;
	
public:

	FName GetID() const { return ActionName; }

	bool CheckMotionDirection(FVector2D AxisInput);
	
	EMotionCommandDirection GetAxisDirection(FVector2D AxisInput);
	
};

#pragma endregion Directional Input Definitions