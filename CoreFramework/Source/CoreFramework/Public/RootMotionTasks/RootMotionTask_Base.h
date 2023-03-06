// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "RootMotionTask_Base.generated.h"

class AOPCharacter;
class UCustomMovementComponent;
enum class ERootMotionFinishVelocityMode : uint8;

/** Base class for specialized root motion attributes */
UCLASS()
class COREFRAMEWORK_API URootMotionTask_Base : public UObject, public FTickableGameObject
{
	friend class AOPCharacter;
	friend class URootMotionTask_MoveToActorForce;
	friend class URootMotionTask_MoveToForce;
	friend class URootMotionTask_JumpForce;
	friend class URootMotionTask_ConstantForce;
	friend class URootMotionTask_RadialForce;
	
	GENERATED_BODY()
	
	URootMotionTask_Base();

	// BEGIN FTickableGameObject Interface
	virtual void Tick( float DeltaTime ) override;
	virtual ETickableTickType GetTickableTickType() const override { return ETickableTickType::Always; }
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT( UInputBufferSubsystem, STATGROUP_Tickables ); }
	virtual bool IsTickableWhenPaused() const { return true; }
	virtual bool IsTickableInEditor() const { return false; }
	// END FTickableGameObject Interface
	
public:
	void InitTask(AOPCharacter* InTaskOwner);
	
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true"))
	void ReadyForActivation();

	void PerformActivation();

	virtual void Activate() {}
	
	AActor* GetAvatarActor() const;

	virtual void TickTask(float DeltaTime) {};

	virtual void ExternalCancel() {};

	virtual void OnDestroy()
	{
		MarkAsGarbage();
	};

protected:

	virtual void SharedInitAndApply() {};

	virtual bool HasTimedOut() const;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override {}
	
	UPROPERTY()
	FName ForceName;

	/** What to do with character's Velocity when root motion finishes */
	UPROPERTY()
	ERootMotionFinishVelocityMode FinishVelocityMode;

	/** If FinishVelocityMode mode is "SetVelocity", character velocity is set to this value when root motion finishes */
	UPROPERTY()
	FVector FinishSetVelocity;

	/** If FinishVelocityMode mode is "ClampVelocity", character velocity is clamped to this value when root motion finishes */
	UPROPERTY()
	float FinishClampVelocity;

	UPROPERTY()
	TWeakObjectPtr<AOPCharacter> CharacterOwner;
	
	UPROPERTY()
	TObjectPtr<UCustomMovementComponent> MovementComponent; 
	
	uint16 RootMotionSourceID;

	bool bIsFinished;

	float StartTime;
	float EndTime;

	uint32 LastFrameNumberWeTicked = INDEX_NONE;

};
