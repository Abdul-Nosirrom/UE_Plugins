// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "InputData.h"
#include "RadicalCharacter.h"
#include "GameFramework/Character.h"
#include "TOPCharacter.generated.h"

/* Delegate Declaration */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnJumpedSignature);


UCLASS()
class COREFRAMEWORK_API ATOPCharacter : public ARadicalCharacter
{
	GENERATED_BODY()

public:

	UPROPERTY(BlueprintAssignable)
	FOnJumpedSignature OnJumpedDelegate;

#pragma region ACharacter Parameters
	/** 
	 * The max time the jump key can be held.
	 * Note that if StopJumping() is not called before the max jump hold time is reached,
	 * then the character will carry on receiving vertical velocity. Therefore it is usually 
	 * best to call StopJumping() when jump input has ceased (such as a button up event).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category=Character, Meta=(ClampMin=0.0, UIMin=0.0))
	float JumpZVelocity;

#pragma endregion ACharacter Parameters

private:
	/** Camera boom positioning the camera behind the character */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	class USpringArmComponent* CameraBoom;

	/** Follow camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	class UCameraComponent* FollowCamera;
	
	/** Buffered Input Map */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	class UInputBufferMap* DefaultInputBufferMapping;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	class UMotionAction* DirectionalInput;
	
	/** Jump Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	class UInputAction* JumpAction;

	/** Move Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	class UInputAction* MoveAction;

	/** Look Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	class UInputAction* LookAction;

public:
	ATOPCharacter();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	
protected:

	/** General Input Registration Binding */
	UFUNCTION()
	void InputTriggered(const UInputAction* InputAction, const FInputActionValue& Value);
	UFUNCTION()
	void InputReleased(const UInputAction* InputAction);
	UFUNCTION()
	void DirectionalRegistered(const UMotionAction* Motion);

	/** Action Binding */
	void Move(const FInputActionValue& Value);

	/** Action Binding */
	void Look(const FInputActionValue& Value);

	/** Action Binding */
	void Jump(const FInputActionValue& Value);

	/** Action Binding */
	void StopJumping(const FInputActionValue& Value);
			

protected:
	// APawn interface
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	
	// To add mapping context
	virtual void BeginPlay() override;

public:
	/** Returns CameraBoom subobject **/
	FORCEINLINE class USpringArmComponent* GetCameraBoom() const { return CameraBoom; }
	/** Returns FollowCamera subobject **/
	FORCEINLINE class UCameraComponent* GetFollowCamera() const { return FollowCamera; }
};

