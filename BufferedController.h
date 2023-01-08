// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Utility/InputBuffer.h"
#include "BufferedController.generated.h"

/* Forward Declarations */
struct FInputActionValue;
struct FInputActionInstance;
class UInputMappingContext;
/* ~~~~~~~~~~~~~~~~~~~ */

//DECLARE_DELEGATE_OneParam(FRegisterInputDelegate, const int32);

UCLASS()
class MOVEMENTTESTING_API ABufferedController : public APlayerController
{
	GENERATED_BODY()

protected:
	FInputBuffer InputBufferObject;
	
public:
	// Sets default values for this actor's properties
	ABufferedController();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enhanced Input")
	class UInputMappingContext* InputMap;

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

	UFUNCTION(BlueprintCallable)
	bool ConsumeInput(UInputAction* InputAction);

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
