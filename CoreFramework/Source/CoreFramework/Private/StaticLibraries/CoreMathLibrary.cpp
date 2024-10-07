// Fill out your copyright notice in the Description page of Project Settings.


#include "StaticLibraries/CoreMathLibrary.h"

#include "RadicalMovementComponent.h"

FQuat UCoreMathLibrary::ComputeRelativeInputVector(const APawn* Pawn)
{
	FVector TargetNormal = FVector::UpVector;
	FRotator Rotation = Pawn->GetActorRotation();

	if (auto PC = Cast<APlayerController>(Pawn->GetController()))
	{
		const auto PCM = PC->PlayerCameraManager;
		Rotation = PCM->GetCameraRotation();
	}
	if (auto RCM = Cast<URadicalMovementComponent>(Pawn->GetMovementComponent()))
	{
		TargetNormal = RCM->IsFalling() ? FVector::UpVector : RCM->CurrentFloor.HitResult.ImpactNormal;
	}

	const FQuat PlaneRotation = UKismetMathLibrary::Quat_FindBetweenNormals(Rotation.Quaternion().GetUpVector(), TargetNormal);
	const FQuat OrientedCamRotation = PlaneRotation * Rotation.Quaternion();
	const FVector Forward = FVector::VectorPlaneProject(OrientedCamRotation.GetForwardVector(), TargetNormal).GetSafeNormal();
	const FVector Right = FVector::VectorPlaneProject(OrientedCamRotation.GetRightVector(), TargetNormal).GetSafeNormal();

	return UKismetMathLibrary::MakeRotFromXY(Forward, Right).Quaternion();
}
