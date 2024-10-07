// Copyright 2023 CoC All rights reserved


#include "Animation/AnimNotify_CombatMagnetism.h"

#include "RadicalMovementComponent.h"
#include "Actors/RadicalCharacter.h"
#include "Components/TargetingQueryComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "StaticLibraries/TargetingStatics.h"

void UAnimNotify_CombatMagnetism::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);
	
	if (PhysicsInfo[MeshComp].Character.IsValid())
	{
		FTargetingResult OutResult;
		UTargetingStatics::PerformTargetingFromPreset(PhysicsInfo[MeshComp].Character.Get(), Targeting, OutResult);
		TargetedActor = OutResult.TargetingComponent->GetOwner();
	}
}

void UAnimNotify_CombatMagnetism::NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	float FrameDeltaTime, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyTick(MeshComp, Animation, FrameDeltaTime, EventReference);

	if (!PhysicsInfo.Contains(MeshComp)) return;
	
	const auto AnimData = PhysicsInfo[MeshComp];
	if (!AnimData.Character.IsValid()) return;

	if (FrameDeltaTime <= UE_SMALL_NUMBER) return; // Zero frame delta, skip this frame
	
	if (TargetedActor)
	{
		const float Alpha = InterpolationCurve.Sample(AnimData.NormalizedTime);
		const float Dist = bOnlyInterpGroundPlane ? FVector::Dist2D(AnimData.Character->GetActorLocation(), TargetedActor->GetActorLocation()) : FVector::Dist(AnimData.Character->GetActorLocation(), TargetedActor->GetActorLocation());
		if (Dist >= MinDistance)
		{
			FVector InterpToPos = TargetedActor->GetActorLocation();
			if (bOnlyInterpGroundPlane) InterpToPos.Z = AnimData.Character->GetActorLocation().Z;
			const FVector TargetPos = FMath::Lerp(AnimData.InitialTransform.GetLocation(), InterpToPos, Alpha);
			//const FVector NewVel = Alpha != 1.f ? (TargetPos - AnimData.Character->GetActorLocation()) / FrameDeltaTime : FVector::ZeroVector;
			//AnimData.Character->GetCharacterMovement()->SetVelocity(NewVel);
			AnimData.Character->SetActorLocation(TargetPos, true);
		}
		FQuat TargetRot = UKismetMathLibrary::MakeRotFromX((TargetedActor->GetActorLocation() - AnimData.Character->GetActorLocation()).GetSafeNormal2D()).Quaternion();
		TargetRot = FQuat::Slerp(AnimData.InitialTransform.GetRotation(), TargetRot, Alpha);
		AnimData.Character->SetActorRotation(TargetRot);
	}
	else if (bUseFallback)
	{
		const float Speed = FallbackSpeedCurve.Sample(AnimData.NormalizedTime);
		const FVector NewVel = AnimData.Character->GetActorForwardVector() * Speed;
		AnimData.Character->GetCharacterMovement()->SetVelocity(NewVel);
	}
}