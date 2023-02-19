// Copyright 2023 Abdulrahmen Almodaimegh. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "InputBuffer.h"
#include "InputData.h"
#include "BufferedController.generated.h"

/* Forward Declarations */
struct FInputActionValue;
struct FInputActionInstance;
class UInputMappingContext;
/* ~~~~~~~~~~~~~~~~~~~ */

// EVENT Input Press Ready To Consume
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FInputActionPressedDelegate, const UInputAction*, InputAction);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDirectionalInputRegisteredDelegate, const UMotionAction*, DirectionalInput);

// EVENT Input Held Ready To Consume
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FInputActionHeldDelegate, const UInputAction*, FOnInputAction, float, TimeHeld);

// EVENT Input Release Ready To Consume
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FInputActionReleasedDelegate, const UInputAction*, InputAction);

UCLASS()
class COREFRAMEWORK_API ABufferedController : public APlayerController
{
	GENERATED_BODY()

	UPROPERTY(BlueprintAssignable)
	FInputActionPressedDelegate InputPressedDelegateSignature;
	UPROPERTY(BlueprintAssignable)
	FDirectionalInputRegisteredDelegate DirectionalRegisteredDelegateSignature;
	UPROPERTY(BlueprintAssignable)
	FInputActionHeldDelegate InputHeldDelegateSignature;
	UPROPERTY(BlueprintAssignable)
	FInputActionReleasedDelegate InputReleasedDelegateSignature;
	
protected:
	FInputBuffer InputBufferObject;
	
public:
	// Sets default values for this actor's properties
	ABufferedController();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enhanced Input")
	class UInputBufferMap* InputMap;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input Buffer")
	uint8 BufferSize{20};

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	/// @brief	Sets up the input IDs via their FName descriptors, and creates the action bindings
	///			whose goals are to just update a shared list between the controller and Buffer object.
	virtual void SetupInputComponent() override;


	/*~~~~~ API ~~~~~*/
public:
	UFUNCTION(BlueprintCallable)
	bool ConsumeInput(UInputAction* InputAction);


private:

	/*~~~~~ Primarily for EIC to bind to and pass inputs ~~~~~*/
	
	/// @brief Event triggered for an input once its been and continues to be triggered
	/// @param ActionInstance Action Instance From Input Action
	/// @param InputName FName descriptor of action used to index into Input Maps
	void TriggerInput(const FInputActionInstance& ActionInstance, const FName InputName);

	/// @brief Event triggered for an input once its no longer considered held
	/// @param ActionValue Action Instance From Input Action
	/// @param InputName FName descriptor of action used to index into Input Maps
	void CompleteInput(const FInputActionValue& ActionValue, const FName InputName);

	virtual void DisplayDebug(UCanvas* Canvas, const FDebugDisplayInfo& DebugDisplay, float& YL, float& YPos) override;
};
