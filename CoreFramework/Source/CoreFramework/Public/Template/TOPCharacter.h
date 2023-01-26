// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "OPCharacter.h"
#include "GameFramework/Character.h"
#include "TOPCharacter.generated.h"

UCLASS()
class COREFRAMEWORK_API ATOPCharacter : public AOPCharacter
{
	GENERATED_BODY()

public:
#pragma region ACharacter Parameters
	/** 
	 * The max time the jump key can be held.
	 * Note that if StopJumping() is not called before the max jump hold time is reached,
	 * then the character will carry on receiving vertical velocity. Therefore it is usually 
	 * best to call StopJumping() when jump input has ceased (such as a button up event).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category=Character, Meta=(ClampMin=0.0, UIMin=0.0))
	float JumpMaxHoldTime;

	/**
	 * The max number of jumps the character can perform.
	 * Note that if JumpMaxHoldTime is non zero and StopJumping is not called, the player
	 * may be able to perform and unlimited number of jumps. Therefore it is usually
	 * best to call StopJumping() when jump input has ceased (such as a button up event).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category=Character)
	int32 JumpMaxCount;

	/**
	 * Tracks the current number of jumps performed.
	 * This is incremented in CheckJumpInput, used in CanJump_Implementation, and reset in OnMovementModeChanged.
	 * When providing overrides for these methods, it's recommended to either manually
	 * increment / reset this value, or call the Super:: method.
	 */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category=Character)
	int32 JumpCurrentCount;

	/**
	 * Represents the current number of jumps performed before CheckJumpInput modifies JumpCurrentCount.
	 * This is set in CheckJumpInput and is used in SetMoveFor and PrepMoveFor instead of JumpCurrentCount
	 * since CheckJumpInput can modify JumpCurrentCount.
	 * When providing overrides for these methods, it's recommended to either manually
	 * set this value, or call the Super:: method.
	*/
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = Character)
	int32 JumpCurrentCountPreJump;

	/** When true, player wants to jump */
	UPROPERTY(BlueprintReadOnly, Category=Character)
	uint32 bPressedJump:1;

	/** Tracks whether or not the character was already jumping last frame. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category=Character)
	uint32 bWasJumping : 1;

	/** 
	 * Jump key Held Time.
	 * This is the time that the player has held the jump key, in seconds.
	 */
	UPROPERTY(Transient, BlueprintReadOnly, VisibleInstanceOnly, Category=Character)
	float JumpKeyHoldTime;

	/** Amount of jump force time remaining, if JumpMaxHoldTime > 0. */
	UPROPERTY(Transient, BlueprintReadOnly, VisibleInstanceOnly, Category=Character)
	float JumpForceTimeRemaining;

#pragma endregion ACharacter Parameters

private:
	/** Camera boom positioning the camera behind the character */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	class USpringArmComponent* CameraBoom;

	/** Follow camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	class UCameraComponent* FollowCamera;
	
	/** MappingContext */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	class UInputMappingContext* DefaultMappingContext;

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

#pragma region Gameplay Methods

	void ResetJumpState();
	
	bool CanJump() const;

	UFUNCTION(BlueprintNativeEvent, Category=Character, meta=(DisplayName="CanJump"))
	bool CanJumpInternal() const;
	virtual bool CanJumpInternal_Implementation() const;

	bool JumpIsAllowedInternal() const;

	virtual bool IsJumpProvidingForce() const;

	virtual void LaunchCharacter(FVector LaunchVelocity, bool bXYOverride, bool zOverride);

	virtual void CheckJumpInput(float DeltaTime);
	virtual void ClearJumpInput(float DeltaTime);

	virtual float GetJumpMaxHoldTime() const;

#pragma endregion Gameplay Methods

	
protected:

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

