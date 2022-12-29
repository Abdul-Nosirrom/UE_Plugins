﻿// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "OPCharacter.generated.h"

/* Forward declarations */
class UCustomMovementComponent;
class UCapsuleComponent;
class UArrowComponent;

UCLASS()
class MOVEMENTTESTING_API AOPCharacter : public APawn
{
	GENERATED_BODY()
	
public:
	// Sets default values for this pawn's properties
	AOPCharacter();

#pragma region Primary Components

private:
	/// @brief Main skeletal mesh associated with this Character
	UPROPERTY(Category=Character, VisibleAnywhere, BlueprintReadOnly, meta=(AllowPrivateAccess="true"))
	TObjectPtr<USkeletalMeshComponent> Mesh;
	
	/// @brief Name of the mesh component used when creating the subobject 
	static FName MeshComponentName;
	
	/// @brief Movement component used for movement logic, containing all movement handling logic
	UPROPERTY(Category=Character, VisibleAnywhere, BlueprintReadOnly, meta=(AllowPrivateAccess="true"))
	TObjectPtr<UCustomMovementComponent> CustomMovement;

	/// @brief Name of the movement component used when creating the subobject 
	static FName CustomMovementComponentName;
	
	/// @brief The CapsuleComponent being used for movement collision (by MovementComponent). Alignment arbitrary.
	UPROPERTY(Category=Character, VisibleAnywhere, BlueprintReadOnly, meta=(AllowPrivateAccess="true"))
	TObjectPtr<UCapsuleComponent> CapsuleComponent;

	/// @brief Name of the capsule component used when creating the subobject 
	static FName CapsuleComponentName;

#if WITH_EDITORONLY_DATA
	/// @brief Component shown in the editor only to indicate character facing
	UPROPERTY()
	TObjectPtr<UArrowComponent> ArrowComponent;
#endif

#pragma endregion Primary Components
	
#pragma region Primary Component Getters
public:
	/// @brief Getter for characters skeletal mesh component
	FORCEINLINE USkeletalMeshComponent* GetMesh() const { return Mesh; }

	/// @brief Getter for characters movement component
	FORCEINLINE UCustomMovementComponent* GetCustomMovement() const { return CustomMovement; }
	
	/// @brief Getter for characters capsule component
	FORCEINLINE UCapsuleComponent* GetCapsuleComponent() const { return CapsuleComponent; }
#pragma endregion Primary Component Getters

#pragma region AActor & UObject Interface

	//~ Begin UObject Interface.
	virtual void PostLoad() override;
	//~ End UObject Interface
	
	//~ Begin AActor Interface.
	virtual void BeginPlay() override;
	virtual void ClearCrossLevelReferences() override;
	virtual void GetSimpleCollisionCylinder(float& CollisionRadius, float& CollisionHalfHeight) const override;
	virtual UActorComponent* FindComponentByClass(const TSubclassOf<UActorComponent> ComponentClass) const override;
	virtual void NotifyActorBeginOverlap(AActor* OtherActor);
	virtual void NotifyActorEndOverlap(AActor* OtherActor);
	//~ End AActor Interface
	
#pragma endregion AActor & UObject Interface

#pragma region APawn Interface

	//~ Begin APawn Interface.
	virtual void PostInitializeComponents() override;
	virtual UPawnMovementComponent* GetMovementComponent() const override;
	//virtual UPrimitiveComponent* GetMovementBase() const override final { return BasedMovement.MovementBase; }
	virtual float GetDefaultHalfHeight() const override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	virtual void DisplayDebug(class UCanvas* Canvas, const FDebugDisplayInfo& DebugDisplay, float& YL, float& YPos) override;
	virtual void RecalculateBaseEyeHeight() override;
	virtual void UpdateNavigationRelevance() override;
	//~ End APawn Interface

#pragma endregion APawn Interface

#pragma region Animation Interface
	
	/** Scale to apply to root motion translation on this Character */
	UPROPERTY()
	float AnimRootMotionTranslationScale;

	/** Play Animation Montage on the character mesh. Returns the length of the animation montage in seconds, or 0.f if failed to play. **/
	UFUNCTION(BlueprintCallable, Category=Animation)
	virtual float PlayAnimMontage(class UAnimMontage* AnimMontage, float InPlayRate = 1.f, FName StartSectionName = NAME_None);

	/** Stop Animation Montage. If nullptr, it will stop what's currently active. The Blend Out Time is taken from the montage asset that is being stopped. **/
	UFUNCTION(BlueprintCallable, Category=Animation)
	virtual void StopAnimMontage(class UAnimMontage* AnimMontage = nullptr);

	/** Return current playing Montage **/
	UFUNCTION(BlueprintCallable, Category=Animation)
	class UAnimMontage* GetCurrentMontage() const;

	/** Get FAnimMontageInstance playing RootMotion */
	FAnimMontageInstance * GetRootMotionAnimMontageInstance() const;

	/** True if we are playing Anim root motion right now */
	UFUNCTION(BlueprintCallable, Category=Animation, meta=(DisplayName="Is Playing Anim Root Motion"))
	bool IsPlayingRootMotion() const;

	/** True if we are playing root motion from any source right now (anim root motion, root motion source) */
	UFUNCTION(BlueprintCallable, Category=Animation)
	bool HasAnyRootMotion() const;

#pragma endregion Animation Interface

};
