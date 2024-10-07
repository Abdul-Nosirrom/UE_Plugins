// Copyright 2023 CoC All rights reserved


#include "Components/CombatActionSystemComponent.h"

#include "NiagaraFunctionLibrary.h"
#include "ActionFramework/Public/ActionSystem/GameplayAction.h"
#include "ActionFramework/Public/AttributeSystem/EntityEffectTypes.h"
#include "ActionFramework/Public/Components/AttributeSystemComponent.h"
#include "CoreFramework/Public/Actors/RadicalCharacter.h"
#include "CoreFramework/Public/Components/RadicalMovementComponent.h"
#include "CoreFramework/Public/DataAssets/MovementData.h"
#include "Debug/CombatFrameworkLog.h"
#include "GameFramework/PhysicsVolume.h"
#include "Kismet/KismetMathLibrary.h"
#include "Subsystems/CombatUtilitiesSubsystem.h"
#include "VisualLogger/VisualLogger.h"

namespace CombatCVars
{
#if ALLOW_CONSOLE && !NO_LOGGING
	
	int32 DisplayHitStun = 0;
	FAutoConsoleVariableRef CVarShowMovementVectors
	(
		TEXT("combat.DisplayState"),
		DisplayHitStun,
		TEXT("Display State. 0: Disable, 1: Enable"),
		ECVF_Default
	);

#endif
}

void UCombatActionSystemComponent::BeginPlay()
{
	Super::BeginPlay();

	ActionActorInfo.SkeletalMeshComponent->GetAnimInstance()->OnPlayMontageNotifyBegin.AddDynamic(this, &UCombatActionSystemComponent::OnNotifyReceived);
	HitStunInfo.Initialize(0.f);
}

void UCombatActionSystemComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	if (IsInHitStun())
	{
		GettingHit(DeltaTime);
	}
	else
	{
		Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	// Draw debug temp
	if (CombatCVars::DisplayHitStun > 0)
	{
		if (!IsInHitStun())
		{
			const FVector DrawLoc = CharacterOwner->GetActorLocation() + 90.f * FVector::UpVector;
			DrawDebugString(GetWorld(), DrawLoc, FString::Printf(TEXT("NOT HITSTUNNED")), 0, FColor::Green, 0.f, true, 1.5f);
		}
		else
		{
			const FVector DrawLoc = CharacterOwner->GetActorLocation() + 130.f * FVector::UpVector;
			DrawDebugString(GetWorld(), DrawLoc, FString::Printf(TEXT("HitStun %.3f\n%s"), HitStunInfo.HitStun, *CurrentAtkData.Value.ToString()), 0, FColor::Red, 0.f, true, 1.5f);

		}
	}
#endif 
}


bool UCombatActionSystemComponent::InternalTryActivateAction(UGameplayAction* Action, bool bQueryOnly)
{
	// Block any action that's trying to activate while we're in hit stun and recovery hasn't started
	if (IsInHitStun() && !(HitStunInfo.ShouldRecover() || HitStunInfo.IsInRecovery())) return false; // TODO: Wrong
	
	const bool bActionStartedAttempt = Super::InternalTryActivateAction(Action, bQueryOnly);

	if (bActionStartedAttempt && IsInHitStun()) 
	{
		StartRecovery(EHitStunRecoveryTypes::None); // Will unbind hit events and bind them back to action system
	}
	
	return bActionStartedAttempt;
}

void UCombatActionSystemComponent::OnNotifyReceived(FName NotifyName,
													const FBranchingPointNotifyPayload& BranchingPointNotifyPayload)
{
	if (NotifyName == CombatNotifies::ClearHitListNotify)
	{
		HitList.Empty();
	}
}

/*--------------------------------------------------------------------------------------------------------------
* Damageable Interface
*--------------------------------------------------------------------------------------------------------------*/

bool UCombatActionSystemComponent::GetHit(FAttackData AttackData, AActor* Attacker)
{
	// Is the attack allowed?
	for (const auto& AtkQueryDelegate : AcceptAttackQueries)
	{
		// All bound queries must pass for the attack to go through. Here's where you'd setup your blocks / parries
		if (!AtkQueryDelegate.Execute(AttackData, Attacker))
		{
			return false;
		}
	}
	
	/* If we got here, that means we're gonna actually apply the attack */

	// Built in I-Frames during recovery
	//if (HitStunInfo.IsInRecovery()) return false;
	
	// If we were hit while already being in HitStun, reset a few things (like bCanWalkOffLedges)
	if (IsInHitStun())
	{
		EndHitStun();
	}

	HitStunInfo.Initialize(AttackData.HitStun);

	// Scaling up the friction values for better readability in editor (so they're 0-1)
	AttackData.AirFriction *= 100.f;
	AttackData.GroundFriction *= 100.f;

	// Cache information about our hitstun state so we can access it later as we update the hit
	CurrentAtkData = {Attacker, AttackData};
	
	// Cancel all active actions, HitStun state is authoritative
	CancelAllActions();
	
	// Snap to face the attacker on first impact frame
	FaceAttacker();

	// Apply knockback
	ApplyKnockBack();

	// Setup physics for our hit stun state'
	SetupPhysics(true);

	HitAnimData.TargetHitAnim = AttackData.AnimDirection /*AttackData.GetAnimationDirection(Attacker, CharacterOwner)*/ + (FMath::VRand() * 0.1f); // Slight perturbation to get some variation
	HitAnimData.HitAnim = 0.6f * HitAnimData.TargetHitAnim;

#if WITH_EDITOR
	// Log victim
	UE_VLOG_LOCATION(CharacterOwner, VLogCombatSystem, Log, CharacterOwner->GetActorLocation(), 50.f, FColor::Yellow, TEXT("Got Hit: %s"), *AttackData.ToString());
	UE_VLOG_LOCATION(CharacterOwner, VLogCombatSystem, Log, Attacker->GetActorLocation(), 50.f, FColor::Green, TEXT(""));
	UE_VLOG_ARROW(CharacterOwner, VLogCombatSystem, Log, Attacker->GetActorLocation(), CharacterOwner->GetActorLocation(), FColor::Green, TEXT(""));

	// Log attacker
	UE_VLOG_LOCATION(Attacker, VLogCombatSystem, Log, CharacterOwner->GetActorLocation(), 50.f, FColor::Yellow, TEXT("Hit Actor: %s"), *AttackData.ToString());
	UE_VLOG_LOCATION(Attacker, VLogCombatSystem, Log, Attacker->GetActorLocation(), 50.f, FColor::Green, TEXT(""));
	UE_VLOG_ARROW(Attacker, VLogCombatSystem, Log, Attacker->GetActorLocation(), CharacterOwner->GetActorLocation(), FColor::Green, TEXT(""));
#endif
	
	return true;
}

void UCombatActionSystemComponent::OnHitConfirm(FAttackData AttackData, AActor* Victim)
{
	// Only add standard impact types to the list, they're the only ones confirmed via a hitbox which persist and could hit multiple enemies
	if (AttackData.ImpactType == EAttackImpactType::Standard)
		HitList.AddUnique(Victim);
	OnHitConfirmDelegate.Broadcast();
}

/*--------------------------------------------------------------------------------------------------------------
* HitStun State
*--------------------------------------------------------------------------------------------------------------*/

void UCombatActionSystemComponent::GettingHit(float DeltaTime)
{
	// Don't tick anything if we're recovering
	if (HitStunInfo.IsInRecovery()) return;
	
	// Decrement Hit Stun only if we're on the ground
	if (CharacterOwner->GetCharacterMovement()->IsMovingOnGround() || CurrentAtkData.Value.LaunchType == EAttackLaunchType::Light)
		HitStunInfo.Tick(DeltaTime);

	// Only start recovery if hitstun is over and we're grounded, if we're not grounded we have to perform a manual recovery (starting an action)
	if (HitStunInfo.ShouldRecover() && CharacterOwner->GetCharacterMovement()->IsMovingOnGround())
	{
		StartRecovery(HitStunInfo.bGroundBehaviorCompleted ? EHitStunRecoveryTypes::FromDowned : EHitStunRecoveryTypes::None);
		return;
	}
	
	UpdateHitStunAnimation(DeltaTime);
}

void UCombatActionSystemComponent::GettingHitVelocity(URadicalMovementComponent* MovementComponent, float DeltaTime)
{
	// In Grapple, we set our velocity to follow the grapple point and that's it
	if (CurrentAtkData.Value.LaunchType == EAttackLaunchType::Grapple)
	{
		//MovementComponent->bIgnorePushout = true; TODO:
		const auto GrappleTarget = CurrentAtkData.Key->FindComponentByClass<USkeletalMeshComponent>()->GetSocketLocation(CombatConstants::GrappleSocket);
		MovementComponent->Velocity = GrappleTarget - MovementComponent->GetActorLocation();
		return;
	}

	// Equation: Velocity = Velocity * (1 - Friction * DeltaTime)
	
	// Just applying gravity if not grounded && Friction terms
	if (MovementComponent->IsMovingOnGround())
	{
		MovementComponent->Velocity *=  (1.f - FMath::Min(CurrentAtkData.Value.GroundFriction * DeltaTime, 1.f));
	}
	else
	{
		//const float VSpeed = MovementComponent->Velocity.Z;
		//const float GravMult = HitStunGravityDecayCurve.GetRichCurve()->Eval(VSpeed);
		//TGuardValue<float> RestoreGravScale(MovementComponent->GravityScale, 2.5f);
		const FVector PlanarVel = FVector::VectorPlaneProject(MovementComponent->Velocity, MovementComponent->GetGravityDir());
		const FVector VerticalVel = MovementComponent->Velocity.ProjectOnToNormal(MovementComponent->GetGravityDir());

		// Apply Friction
		MovementComponent->Velocity = PlanarVel * (1.f - FMath::Min(CurrentAtkData.Value.AirFriction.X * DeltaTime, 1.f)) + VerticalVel * (1.f - FMath::Min(CurrentAtkData.Value.AirFriction.Y * DeltaTime, 1.f));

		// Apply Gravity
		MovementComponent->Velocity += MovementComponent->GetGravityDir() * FMath::Abs(MovementComponent->GetGravityZ()) * DeltaTime;

		// Clamp to terminal speeds
		MovementComponent->Velocity.Z = FMath::Sign(MovementComponent->Velocity.Z) * FMath::Min(FMath::Abs(MovementComponent->Velocity.Z), MovementComponent->GetPhysicsVolume()->TerminalVelocity);
	}
}

void UCombatActionSystemComponent::GettingHitRotation(URadicalMovementComponent* MovementComponent, float DeltaTime)
{
	// We don't actually do any rotation of the capsule, instead, our rotations are sent off to the animator during HitStun
}

void UCombatActionSystemComponent::UpdateHitStunAnimation(float DeltaTime)
{
	HitAnimData.HitAnim += (HitAnimData.TargetHitAnim - HitAnimData.HitAnim) * 0.2f * DeltaTime;

	HitAnimData.HitRoll += CurrentAtkData.Value.HitRoll * DeltaTime;
	CurrentAtkData.Value.HitRoll.Pitch *= (1.f - FMath::Min(CurrentAtkData.Value.RollFriction.Pitch * DeltaTime, 1.f));
	CurrentAtkData.Value.HitRoll.Yaw *= (1.f - FMath::Min(CurrentAtkData.Value.RollFriction.Yaw * DeltaTime, 1.f));
	CurrentAtkData.Value.HitRoll.Roll *= (1.f - FMath::Min(CurrentAtkData.Value.RollFriction.Roll * DeltaTime, 1.f));
}

/*--------------------------------------------------------------------------------------------------------------
* Ground / Wall behaviors
*--------------------------------------------------------------------------------------------------------------*/

void UCombatActionSystemComponent::OnGroundHitStunImpact(const FHitResult& LandingHit)
{
	if (CurrentAtkData.Value.GroundReaction == EAttackGroundReact::None)
	{
		StartRecovery(EHitStunRecoveryTypes::FromLanding);
		return;
	}

	if (HitStunInfo.bGroundBehaviorCompleted) return;

	HitStunInfo.GroundImpact();
	
	switch (CurrentAtkData.Value.GroundReaction)
	{
		case EAttackGroundReact::Down:
			//AnimationFlag.Down = true;
			break;
		case EAttackGroundReact::Bounce:
			CharacterOwner->LaunchCharacter(FVector::UpVector * CurrentAtkData.Value.BounceVelocity, false, true);
			break;
		case EAttackGroundReact::Skid:
			//AnimationFlag.Skid = true;
			break;
		case EAttackGroundReact::Tumble:
			//AnimationFlag.Tumble = true;
			break;
	}
}

void UCombatActionSystemComponent::OnWallHitStunImpact(AActor* SelfActor, AActor* OtherActor, FVector NormalImpulse,
	const FHitResult& Hit)
{
	// What we hit can constitute a floor, ignore it (maybe some more tests here on what constitutes a 'wall'
	if (ActionActorInfo.MovementComponent->IsFloorStable(Hit)) return;
	if (CurrentAtkData.Value.WallReaction == EAttackWallReact::None) return;

	//HitStunInfo.WallImpact();
	
	// Zero out velocity
	ActionActorInfo.MovementComponent->SetVelocity(FVector::ZeroVector);
	//SpawnWallImpactParticle();
	//DoWallImpactGameFeel();
	
	switch (CurrentAtkData.Value.WallReaction)
	{
		case EAttackWallReact::Splat:
			// We'll fall forward due to gravity, if we were facing the wall, we fall face up. Otherwise face down.
			break;
		case EAttackWallReact::Slide:
			// Increase air Z friction to mimick a slide (needs tweaking but magic number for now
			CurrentAtkData.Value.AirFriction.Y = 10.f;
			break;
		case EAttackWallReact::Bounce:
			CharacterOwner->LaunchCharacter(Hit.ImpactNormal * 900.f, true, true);
			// or, if we don't actually modify character rotation during the getting hit stage and all that just happens in the animator
			//CharacterOwner->LaunchCharacter(CharacterOwner->GetActorForwardVector() * 700.f, true, true);
			break;
	}

}

/*--------------------------------------------------------------------------------------------------------------
* Recovery
*--------------------------------------------------------------------------------------------------------------*/

void UCombatActionSystemComponent::StartRecovery(EHitStunRecoveryTypes RecoveryType)
{
	SetupPhysics(false);
	
	// Broadcast events
	if (!RecoveryMontages.Contains(RecoveryType))
	{
		EndHitStun();
		return;
	}

	HitStunInfo.StartRecovery();

	UAnimMontage* RecoveryMontage = RecoveryMontages[RecoveryType];

	OnRecoveryStartedDelegate.Broadcast(RecoveryMontage, CurrentAtkData.Key);
	
	CharacterOwner->PlayAnimMontage(RecoveryMontage);
	ActiveRecoveryMontage = CharacterOwner->GetMesh()->GetAnimInstance()->GetActiveInstanceForMontage(RecoveryMontage);
	if (!ActiveRecoveryMontage) return;

	CharacterOwner->GetMesh()->GetAnimInstance()->OnMontageEnded.AddDynamic(this, &UCombatActionSystemComponent::RecoveryMontageEnded);
	HitAnimData.HitAnim = FVector::ZeroVector; // NOTE: Temp so blend out doesnt go into a hit pose because thats what happens
}

void UCombatActionSystemComponent::EndHitStun()
{
	// Setup physics after given how dumb my anim notify is for rotation. Really need to figure out a good solution for priority binding, a stack?
	HitStunInfo.Initialize(-1.f);

	// If we were cancelling out of recovery, end the montage
	if (ActiveRecoveryMontage)
	{
		CharacterOwner->StopAnimMontage(ActiveRecoveryMontage->Montage);
	}
	
	CurrentAtkData = {};
}

void UCombatActionSystemComponent::RecoveryMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	ActiveRecoveryMontage = nullptr;
	if (IsInRecovery()) EndHitStun();

	CharacterOwner->GetMesh()->GetAnimInstance()->OnMontageEnded.RemoveDynamic(this, &UCombatActionSystemComponent::RecoveryMontageEnded);
}


/*--------------------------------------------------------------------------------------------------------------
* Utility/Helpers
*--------------------------------------------------------------------------------------------------------------*/

void UCombatActionSystemComponent::FaceAttacker()
{
	if (CurrentAtkData.Value.bReverseHit)
	{
		FVector KnockbackVector = CurrentAtkData.Value.GetKnockBackVector(CurrentAtkData.Key, CharacterOwner);
		const FRotator TargetRot = UKismetMathLibrary::MakeRotFromZX(FVector::UpVector, -KnockbackVector.GetSafeNormal2D());
		CharacterOwner->SetActorRotation(TargetRot);
		return;
	}
	const FVector ToAttacker = (CurrentAtkData.Key->GetActorLocation() - CharacterOwner->GetActorLocation()).GetSafeNormal();
	const FRotator TargetRot = UKismetMathLibrary::MakeRotFromZX(FVector::UpVector, ToAttacker);
	CharacterOwner->SetActorRotation(TargetRot);
}

void UCombatActionSystemComponent::ApplyKnockBack()
{
	const FVector KnockBackVector = CurrentAtkData.Value.GetKnockBackVector(CurrentAtkData.Key, GetCharacterOwner());
	CharacterOwner->LaunchCharacter(KnockBackVector, true, true);
}

void UCombatActionSystemComponent::SetupPhysics(bool bHitStateOn)
{
	// If we're being put in the HitStun state, bind the hit functions
	if (bHitStateOn)
	{
		// Bind to collision & Landing
		CharacterOwner->LandedDelegate.AddUniqueDynamic(this, &UCombatActionSystemComponent::OnGroundHitStunImpact);
		CharacterOwner->OnActorHit.AddUniqueDynamic(this, &UCombatActionSystemComponent::OnWallHitStunImpact);
	}
	else // Else we bind to our regular action update loop functions
	{
		// Unbind collision & landing
		CharacterOwner->LandedDelegate.RemoveDynamic(this, &UCombatActionSystemComponent::OnGroundHitStunImpact);
		CharacterOwner->OnActorHit.RemoveDynamic(this, &UCombatActionSystemComponent::OnWallHitStunImpact);
	}
}
