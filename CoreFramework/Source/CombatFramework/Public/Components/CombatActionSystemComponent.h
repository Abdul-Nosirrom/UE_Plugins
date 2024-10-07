// Copyright 2023 CoC All rights reserved

#pragma once

#include "CoreMinimal.h"
#include "ActionFramework/Public/Components/ActionSystemComponent.h"
#include "Interfaces/DamageableInterface.h"
#include "CombatActionSystemComponent.generated.h"


/// Events we want:
/// - OnHitLanded
/// - OnRecoveryStarted (may wanna play a shine vfx here to notify that they can cancel out of it)

/// Stages of a hit
/// - "GettingHit": Hit stun decays in this duration. When HitStun == 0, the "HitReact" action ends itself. So HitStun controls the duration the hit react is active
///					but there are special cases. Grapple is always active, launch is active until we hit the ground (?) or it continues till we hit the ground and start a recovery anim
///					but if Hitstun is over we can cancel out of it
/// - InRecovery: Towards the end of the hit stun (can be constant), we're in a recovery state. Here's where we can cancel early

class URadicalMovementComponent;

DECLARE_DYNAMIC_DELEGATE_RetVal_TwoParams(bool, FAcceptAttackQuery, const FAttackData&, AttackData, AActor*, Attacker);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FRecoveryStarted, UAnimMontage*, RecoveryMontage, AActor*, InitialAttacker);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FHitConfirmEvent);

UENUM(BlueprintType)
enum class EHitStunRecoveryTypes : uint8
{
	None,
	FromDowned,
	FromLanding
};

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class COMBATFRAMEWORK_API  UCombatActionSystemComponent :	public UActionSystemComponent
{
	GENERATED_BODY()
protected:
	
	/// UObject Interface
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	/// ~UObject Interface 

	/// Animation Interface (Maybe we shouldn't do it this way, instead have a function on here that directly calls "ClearHitList"?
	UFUNCTION()
	void OnNotifyReceived(FName NotifyName, const FBranchingPointNotifyPayload& BranchingPointNotifyPayload);
	/// ~Animation Interface 

public:
	/// IDamageable Interface
	bool GetHit(FAttackData AttackData, AActor* Attacker);
	void OnHitConfirm(FAttackData AttackData, AActor* Victim);
	/// ~IDamageable Interface

	// Public API
	FHitReactAnimationData GetHitStunAnimInfo() const { return HitAnimData; }
	bool CanAttackTarget(AActor* Target) const { return !HitList.Contains(Target);}
	UFUNCTION(Category=Combat, BlueprintPure)
	bool IsInHitStun() const { return !HitStunInfo.bHitStunOver; }
	UFUNCTION(Category=Combat, BlueprintPure)
	bool IsInRecovery() const { return HitStunInfo.IsInRecovery(); }
	// ~Public API
	
	UPROPERTY(BlueprintAssignable)
	FHitConfirmEvent OnHitConfirmDelegate;
	UPROPERTY(BlueprintAssignable)
	FRecoveryStarted OnRecoveryStartedDelegate;
	
protected:
	// ActionSystem Interface
	/// @brief	Override to block actions from activating during HitStun
	virtual bool InternalTryActivateAction(UGameplayAction* Action, bool bQueryOnly=false) override;
	virtual void CalculateVelocity(URadicalMovementComponent* MovementComponent, float DeltaTime) override
	{
		if (IsInHitStun()) GettingHitVelocity(MovementComponent, DeltaTime);
		else Super::CalculateVelocity(MovementComponent, DeltaTime);
	}
	virtual void UpdateRotation(URadicalMovementComponent* MovementComponent, float DeltaTime) override
	{
		if (IsInHitStun()) GettingHitRotation(MovementComponent, DeltaTime);
		else Super::UpdateRotation(MovementComponent, DeltaTime);
	}
	// ~ActionSystem Interface
	
	/// HitStun State Core
	virtual void GettingHit(float DeltaTime);
	virtual void GettingHitVelocity(URadicalMovementComponent* MovementComponent, float DeltaTime);
	virtual void GettingHitRotation(URadicalMovementComponent* MovementComponent, float DeltaTime);
	virtual void UpdateHitStunAnimation(float DeltaTime);
	/// ~ HitStun State Core
	
	// HitStun State Responses
	UFUNCTION()
	virtual void OnGroundHitStunImpact(const FHitResult& LandingHit);
	UFUNCTION()
	virtual void OnWallHitStunImpact(AActor* SelfActor, AActor* OtherActor, FVector NormalImpulse, const FHitResult& Hit);
	// ~HitStun State Responses

	// Recovery State
	virtual void StartRecovery(EHitStunRecoveryTypes RecoveryType);
	virtual void EndHitStun();

	UFUNCTION()
	void RecoveryMontageEnded(UAnimMontage* Montage, bool bInterrupted);
	// ~Recovery State
	
	// Utility Functions
	void FaceAttacker();
	void ApplyKnockBack();
	void SetupPhysics(bool bHitStateOn);
	// ~Utility Functions

public:
	/// @brief  Query invoked when "GetHit" is called on this actor. Can bind to this to block a hit and respond to the attacker.
	///			If a binding returns "false", the hit won't go through.
	TArray<FAcceptAttackQuery> AcceptAttackQueries;

protected:
	/// @brief	Recovery Montages
	UPROPERTY(Category=Combat, EditDefaultsOnly)
	TMap<EHitStunRecoveryTypes, UAnimMontage*> RecoveryMontages;
	
	/// @brief  Cached active recovery montage, for use when cancelling out of recovery. Frequently null
	struct FAnimMontageInstance* ActiveRecoveryMontage;
	
	/// @brief	Animation data should update these and they're passed over to the animator. Contains stuff like mesh rotation.
	FHitReactAnimationData HitAnimData;
	
	/// @brief  Information about our current HitStun state. How much HitStun is left, what behaviors have we done, and recovery info.
	FTransientHitFrameData HitStunInfo;
	
	/// @brief  Transient info about current hitstun state. Holds the attacker and the attack data (may not be the same we started with,
	///			this attack data is mutable to reflect behaviors we encounter throughout the hitstun state)
	TPair<AActor*, FAttackData> CurrentAtkData;
	
	/// @brief  Container that holds the list of actors we've hit in a given attack event. To prevent pawns from getting hit twice
	///			from the same attack. Must call ClearHitList() to reset this otherwise Standard attacks could be discarded
	UPROPERTY(Transient)
	TArray<AActor*> HitList;

	/* 
	AcceptAttackQuery Binding Example
	{
		auto MyDelegate = AcceptAttackQueries.AddDefaulted_GetRef();
		MyDelegate.BindDynamic();
	}
	*/
};
