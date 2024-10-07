// Copyright 2023 CoC All rights reserved


#include "Animation/AnimNotifyState_Attack.h"

#include "Components/CapsuleComponent.h"
#include "Debug/CombatFrameworkLog.h"
#include "Interfaces/DamageableInterface.h"
#include "Kismet/KismetSystemLibrary.h"
#include "StaticLibraries/CombatStatics.h"
#include "VisualLogger/VisualLogger.h"


void UAnimNotifyState_Attack::BranchingPointNotifyBegin(FBranchingPointNotifyPayload& BranchingPointPayload)
{
	const auto MeshComp = BranchingPointPayload.SkelMeshComponent;

	if (!MeshComp->GetOwner()) return;

	MeshComp->GetAnimInstance()->OnPlayMontageNotifyBegin.Broadcast(CombatNotifies::ClearHitListNotify, BranchingPointPayload);
}

void UAnimNotifyState_Attack::BranchingPointNotifyEnd(FBranchingPointNotifyPayload& BranchingPointPayload)
{
	const auto MeshComp = BranchingPointPayload.SkelMeshComponent;

	if (!MeshComp->GetOwner()) return;

	MeshComp->GetAnimInstance()->OnPlayMontageNotifyBegin.Broadcast(CombatNotifies::ClearHitListNotify, BranchingPointPayload);
}

void UAnimNotifyState_Attack::BranchingPointNotifyTick(FBranchingPointNotifyPayload& BranchingPointPayload,
	float FrameDeltaTime)
{
	const auto MeshComp = BranchingPointPayload.SkelMeshComponent;
	if (!MeshComp->GetOwner()) return;
	
	const FQuat OwnerQuat = MeshComp->GetOwner()->GetActorQuat();
	FVector Pos = MeshComp->GetOwner()->GetActorLocation() + OwnerQuat * HitBoxPos;
	FVector Size = OwnerQuat.GetForwardVector().GetAbs() * HitBoxSize.X + OwnerQuat.GetRightVector().GetAbs() * HitBoxSize.Y + OwnerQuat.GetUpVector().GetAbs() * HitBoxSize.Z;

	TArray<AActor*> Overlaps;

	//FCollisionShape BoxShape = FCollisionShape::MakeBox(HitBoxSize);
	//GetWorld()->OverlapMultiByChannel(Overlaps, Pos, MeshComp->GetOwner()->GetActorQuat(), CollisionChannel, BoxShape);
	// NOTE: This is bad and doesn't respect orientation, should use the above version to get the correct box orientation but idk collision channels are wwwwwwwack
	UKismetSystemLibrary::BoxOverlapActors(MeshComp->GetWorld(), Pos, Size, {UEngineTypes::ConvertToObjectType(ECC_Pawn)}, nullptr, {MeshComp->GetOwner()}, Overlaps);
	const FBox DrawBox = FBox(-Size, Size);

	for (auto& OverlapActor : Overlaps)
	{
		if (UCombatStatics::CombatHit(AttackData, MeshComp->GetOwner(), OverlapActor))
		{
			UE_VLOG_OBOX(MeshComp->GetOwner(), VLogCombatSystem, Log, DrawBox, FQuatRotationTranslationMatrix::Make(MeshComp->GetOwner()->GetActorQuat(), Pos), FColor::Red, TEXT("AttackHitBox"));
		}
	}

	
#if WITH_EDITOR
	// Editor helper visualizer for the hitbox (adjusted because mesh rotation is diff Y is forward so we swizzle)
	if (!MeshComp->GetWorld()->IsPlayInEditor())
	{
		const FVector AdjustedPos = MeshComp->GetComponentLocation() + FVector(HitBoxPos.Y, HitBoxPos.X, HitBoxPos.Z) + FVector::UpVector * 88.f; // NOTE: Assumed capsule halfheight offset
		const FVector AdjustedSize = FVector(HitBoxSize.Y, HitBoxSize.X, HitBoxSize.Z);
		DrawDebugBox(MeshComp->GetWorld(), AdjustedPos, AdjustedSize, FColor::Red, false, 0.f, 0.f, 5.f);
		DrawDebugSolidBox(MeshComp->GetWorld(), AdjustedPos, AdjustedSize, FColor(255, 0, 0, 100), false, 0.f);
	}
#endif
}
