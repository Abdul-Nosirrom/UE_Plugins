// Copyright 2023 CoC All rights reserved

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "CombatData.generated.h"

// Primary resource [https://www.dustloop.com/w/BBTag/Damage#Other_Damage]

class UNiagaraSystem;

namespace CombatNotifies
{
	inline static FName ClearHitListNotify = "ClearHitList";
}

namespace CombatConstants
{
	inline static float WhiffWindow = 0.133f; // 8 frames in 60 fps
	inline static float GroundStunMax = 0.5f; // In seconds, when we start a ground behavior we reset our hitstun to this
	inline static float WallStunMax = 1.f; // In seconds, when we start a wall behavior, we reset our hitstun to this
	inline static FName GrappleSocket = "Grapple"; // Common socket for grapple moves
}


/// @brief	Used by the victim of an attack to determine reaction stuff as well as how to perform detection
UENUM(BlueprintType)
enum class EAttackImpactType : uint8
{
	/// @brief  
	Standard,
	/// @brief  
	Ranged,
	/// @brief  
	GrappleImpact
};

/// @brief	Drives the animation and hit react action in certain cases (e.g Grapple)
UENUM(BlueprintType)
enum class EAttackLaunchType : uint8
{
	/// @brief	Basic Light Attack
	Light	= 0,
	/// @brief	Attack that puts the victim in a "stunned" state, where they'd then slowly fall to the ground
	Crumple	= 1,
	/// @brief	Attack that launches the victim
	Launch	= 2,
	/// @brief	Attack that puts the victim in a "stunned" state where they're instantly knocked into the ground 
	Down	= 3,
	/// @brief	Attack that will apply only to enemies who are performing an attack
	Parry	= 4,
	/// @brief  Attack that will cache a single victim, for use later in a grapple state
	Grapple	= 5
};

/// @brief	Determines how to react once we hit the ground if the attack sent us aerial
UENUM(BlueprintType)
enum class EAttackGroundReact : uint8
{
	/// @brief	Ground collision leads to recovery.
	None,
	/// @brief	Enter a "Downed" stun state. Affects Animation Only
	Down,
	/// @brief	Bounce (by BounceVel) when hitting the ground, maintaining lateral velocity that has friction applied to it
	Bounce,
	/// @brief	Slide back with current pose. Affects Animation Only
	Skid,
	/// @brief	Slide back with rotating/rolling pose. Affects Animation Only.
	Tumble
};

/// @brief	Determines how to react once we hit a wall (non-walkable ground) if the attack sent us into it
UENUM(BlueprintType)
enum class EAttackWallReact : uint8
{
	/// @brief  Wall collision ignored
	None,
	/// @brief  Will hit the wall, staying there stunned for a duration, then fall face flat onto the ground
	Splat,
	/// @brief  Will splat for a bit then slide down (maybe lets not do this idk)
	Slide,
	/// @brief  Will hit the wall then bounce away from it
	Bounce
};

/// @brief	Determines the basis in which to evaluate the knockback direction from
UENUM()
enum class EAttackKnockBackType : uint8
{
	/// @brief	Knockback direction determined based on attackers orientation. Forward is their forward, right is their right, up is world up.
	Frontal,
	/// @brief	X is away from the instigator, Y is the tangent of the circle, Z is up
	Radial,
	/// @brief	X is away from the instigator, Y is the tangent of the circle, Z is up
	Radial2D
};

USTRUCT(BlueprintType)
struct COMBATFRAMEWORK_API FHitReactAnimationData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FVector HitAnim;
	
	FVector TargetHitAnim;
	
	EAttackLaunchType HitType;
	UPROPERTY(BlueprintReadOnly)
	FRotator HitRoll;

	bool bDowned;

	int8 bFaceDownType; // [1 face up, 0 ignore, -1 face down]
};

USTRUCT(BlueprintType)
struct COMBATFRAMEWORK_API FAttackData
{
	GENERATED_BODY()

	FString ToString() const
	{
		return FString::Printf(TEXT("\nImpact Type: %d\nLaunch Type: %d \nGroundBehavior: %d \nKnockbackVector %s"),
			ImpactType, LaunchType, GroundReaction, *KnockBack.ToCompactString());
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EAttackImpactType ImpactType;
	// NOTE: Depending on the knockback rotation type, this will need be transformed (e.g Facing the attacker, we rotate the victim then transform this, steady facing we apply it from attackers reference frame right away?)
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector AnimDirection;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float HitStop;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float HitStun;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector KnockBack;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EAttackKnockBackType KnockBackReferenceFrame;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin=0, UIMin=0, ClampMax=1, UIMax=1))
	FVector2D AirFriction; // X for lateral, Y for Vertical
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin=0, UIMin=0, ClampMax=1, UIMax=1))
	float GroundFriction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FRotator HitRoll;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FRotator RollFriction;
	
	// Is blockable or parry-able

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EAttackLaunchType LaunchType;
	// These are how to rotate at the moment of impact
	/// @brief	Face down after getting hit?
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	uint8 bFaceDownWithHit	: 1;
	/// @brief	Rotate in the direction of velocity during hitstun
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	uint8 bVelocityRotation : 1;
	/// @brief  Rotate in the opposite direction of the hit direction during hitstun
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	uint8 bReverseHit		: 1;
	/// @brief	Maintain current rotation when hit
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	uint8 bSteadyFacing		: 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EAttackGroundReact GroundReaction;
	/// @brief	Face down when hitting the ground?
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	uint8 bFaceDownWhenGround : 1;
	/// @brief	Upwards velocity of the "Bounce" when hitting the ground
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float BounceVelocity;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EAttackWallReact WallReaction;

	UPROPERTY(Category=Stats, EditAnywhere, BlueprintReadWrite)
	TSubclassOf<class UEntityEffect> EffectToApply;

	UPROPERTY(Category=Stats, EditAnywhere, BlueprintReadWrite)
	TObjectPtr<UNiagaraSystem> HitSparkEffect;

	FVector GetKnockBackVector(AActor* Attacker, AActor* Victim) const;

	FVector GetAnimationDirection(AActor* Attacker, AActor* Victim) const;
};

// Use this to store all our current "HitStun frame data"
USTRUCT()
struct COMBATFRAMEWORK_API FTransientHitFrameData
{
	GENERATED_BODY()

	void Initialize(float InHitStun)
	{
		HitStun = InHitStun;
		bWallBehaviorCompleted = false;
		bGroundBehaviorCompleted = false;
		bRecoveryStarted = false;
		bHitStunOver = HitStun <= 0.f;
	}

	void Tick(float DeltaTime)
	{
		HitStun = FMath::Max(HitStun - DeltaTime, 0.f);
		//bHitStunOver = HitStun <= 0.f;
	}

	void WallImpact()
	{
		bWallBehaviorCompleted = true;
		HitStun = FMath::Max(CombatConstants::WallStunMax, HitStun);
	}

	void GroundImpact()
	{
		bGroundBehaviorCompleted = true;
		HitStun = FMath::Max(CombatConstants::GroundStunMax, HitStun);
	}

	bool ShouldRecover() const
	{
		return HitStun <= 0.1f && !bRecoveryStarted;
	}

	bool IsInRecovery() const { return bRecoveryStarted; } 
	
	void StartRecovery()
	{
		bRecoveryStarted = true;
	}
	
	float HitStun;

	uint8 bHitStunOver				: 1;
	uint8 bWallBehaviorCompleted	: 1;
	uint8 bGroundBehaviorCompleted	: 1;
	uint8 bRecoveryStarted			: 1;
};