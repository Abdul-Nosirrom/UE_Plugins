// 

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "ActionSystem/GameplayAction.h"
#include "Action_LevelPrimitive.generated.h"

/*~~~~~ Forward Declarations ~~~~~*/
class ALevelPrimitiveActor;

UCLASS(Blueprintable, BlueprintType, ClassGroup=ActionManager)
class ACTIONFRAMEWORK_API UActionData_LevelPrimitive : public UGameplayActionData
{
	GENERATED_BODY()

public:
	float SomethingElse;
};

/// @brief	Base class for state machines that attach to a level primitive. Template states handles common enter/exit conditions
///			and retrieving reference to level primitive actor. As well as base parameters like Rail Tag. Will also auto-set the
///			characters movement state to GENERAL (though this can be overridden) as it assumes that's what'll be useful.
UCLASS(Abstract, Blueprintable, BlueprintType, ClassGroup=ActionManager, meta=(DisplayName="Level Primitive Action Template"))
class ACTIONFRAMEWORK_API UAction_LevelPrimitive : public UGameplayAction
{
	GENERATED_BODY()

protected:

	DECLARE_ACTION_DATA(UActionData_LevelPrimitive);
	
	UPROPERTY(Category="Level Primitive Info", EditAnywhere, BlueprintReadWrite, meta=(GameplayTagFilter="LP"))
	FGameplayTag PrimitiveTag;
	
	UPROPERTY(Transient)
	ALevelPrimitiveActor* LevelPrimitive;

	// BEGIN UGameplayAction Interface
	virtual void OnActionActivated_Implementation() override;
	virtual void OnActionEnd_Implementation() override;
	// END UGameplayAction Interface

	// BEGIN Control Flow
	virtual bool EnterCondition_Implementation() override;
	// END Control Flow
};
