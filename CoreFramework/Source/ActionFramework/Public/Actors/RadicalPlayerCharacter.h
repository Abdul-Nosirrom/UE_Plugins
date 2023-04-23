// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Actors/RadicalCharacter.h"
#include "RadicalPlayerCharacter.generated.h"

/* ~~~~~ Forward Declarations ~~~~~ */
class UCameraBoomComponent;
class UCameraComponent;
class UActionManagerComponent;
class UInputBufferMap;
class UInputAction;
class UInputBufferSubsystem;
struct FInputActionValue;

UCLASS()
class ACTIONFRAMEWORK_API ARadicalPlayerCharacter : public ARadicalCharacter
{
	GENERATED_BODY()

private:
	/** Camera boom positioning the camera behind the character */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCameraBoomComponent> CameraBoom;

	/** Follow camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCameraComponent> FollowCamera;

	/** Follow camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Actions, meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UActionManagerComponent> ActionManager;
	
	/** Buffered Input Map */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputBufferMap> InputMap;

	/** Look Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> MoveAction;

	/** Look Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> LookAction;

public:
	// Sets default values for this actor's properties
	ARadicalPlayerCharacter();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	
	void RegisterMoveAxis(const FInputActionValue& Value);
	void RegisterLookAxis(const FInputActionValue& Value);

public:
	/** Returns CameraBoom subobject **/
	UFUNCTION(Category="Radical Components", BlueprintCallable)
	FORCEINLINE class UCameraBoomComponent* GetCameraBoom() const { return CameraBoom; }
	/** Returns FollowCamera subobject **/
	UFUNCTION(Category="Radical Components", BlueprintCallable)
	FORCEINLINE class UCameraComponent* GetFollowCamera() const { return FollowCamera; }
	/** Returns ActionManager subobject **/
	UFUNCTION(Category="Radical Components", BlueprintCallable)
	FORCEINLINE UActionManagerComponent* GetActionManager() const { return ActionManager; }
	UFUNCTION(Category="Radical Components", BlueprintCallable)
	UInputBufferSubsystem* GetInputBuffer() ;

	virtual void DisplayDebug(UCanvas* Canvas, const FDebugDisplayInfo& DebugDisplay, float& YL, float& YPos) override;
};
