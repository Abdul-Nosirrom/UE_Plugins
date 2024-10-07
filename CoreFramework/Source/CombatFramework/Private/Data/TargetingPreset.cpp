// Copyright 2023 CoC All rights reserved


#include "Data/TargetingPreset.h"

#include "Components/TargetingQueryComponent.h"
#include "Debug/CombatFrameworkLog.h"
#include "Engine/OverlapResult.h"
#include "Interfaces/DamageableInterface.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "StaticLibraries/CharacterSweepLibrary.h"
#include "VisualLogger/VisualLogger.h"

bool FTargetingSettings::ProcessTargets(const TArray<FOverlapResult>& PotentialTargets, FTargetingResult& OutTarget) const
{
	bool bFoundTarget = false;
	
	float CurNormDist, CurNormAngle, CurDist, CurAngle;
	float BestDist = -UE_BIG_NUMBER, BestAngle = -UE_BIG_NUMBER, BestWeight = -UE_BIG_NUMBER;
	FOverlapResult BestOverlap;
	for (const auto& Target : PotentialTargets)
	{
		if (!Target.Component.IsValid()) continue;
		
		if (!IsTargetValid(Target.Component.Get(), CurDist, CurAngle, CurNormDist, CurNormAngle))
		{
			UE_VLOG_LOCATION(Instigator.Get(), VLogTargetingSystem, Log, Target.Component->GetComponentLocation(), 10.f, FColor::Red, TEXT("Invalid Target"));
			continue;
		}

		const float CurWeight = DistanceWeight * CurNormDist + AngleWeight * CurNormAngle;
		bFoundTarget = true;

		if (CurWeight > BestWeight)
		{
			BestWeight = CurWeight;
			BestOverlap = Target;
			BestDist = CurDist;
			BestAngle = CurAngle;
		}
		
		UE_VLOG_LOCATION(Instigator.Get(), VLogTargetingSystem, Log, Target.Component->GetComponentLocation(), 10.f, FColor::Yellow, TEXT("Valid Target [Result %.2f (%.2f, %.2f)]"), CurWeight, CurDist, CurAngle);
	}

	if (bFoundTarget)
	{
		UE_VLOG_CONE(Instigator.Get(), VLogTargetingSystem, Log, BestOverlap.Component->GetComponentLocation() + FVector::UpVector * 40.f, FVector::UpVector, 75.f, 15.f, FColor::Green, TEXT("FOUND [%.2f]"), BestWeight);
		OutTarget = FTargetingResult(Cast<UTargetingQueryComponent>(BestOverlap.GetComponent()), BestDist, BestAngle);
	}

	DoVisualLog();

	return bFoundTarget;
}

bool FTargetingSettings::IsTargetValid(const UPrimitiveComponent* CheckTarget, float& Dist, float& Angle, float& NormalizedDist, float& NormalizedAngle) const
{
	const UTargetingQueryComponent* TargetingQueryComponent = Cast<UTargetingQueryComponent>(CheckTarget);
	if (!TargetingQueryComponent) return false;

	// Check identity tags
	if (!TargetingQueryComponent->EvaluateTargetingTags(*this)) return false;
	
	const FVector AngleCheckVector = GetBasis(AngleReferenceFrame).Vector();
	const FVector DistanceCheckCenter = GetCenterPosition();
	const FVector ToTarget = CheckTarget->GetComponentLocation() - DistanceCheckCenter;
	
	// Check additional angle filters
	for (const auto& AngleFilter : AdditionalAngleFilters)
	{
		const FVector HeadingVector = GetBasis(AngleFilter.ReferenceFrame).RotateVector(AngleFilter.HeadingVector);
		const float FilterAngles = FMath::RadiansToDegrees(FMath::Acos(HeadingVector | ToTarget.GetSafeNormal()));
		if ((FilterAngles < AngleFilter.AngleThresholds.GetMin()) || (FilterAngles > AngleFilter.AngleThresholds.GetMax()))
		{
			return false;
		}
	}
	
	// Do they lie in the right angle?
	Angle = FMath::RadiansToDegrees(FMath::Acos(AngleCheckVector | ToTarget.GetSafeNormal()));
	if ((Angle < AngleThresholds.GetMin()) || (Angle > AngleThresholds.GetMax())) return false;

	// Favor smaller angles
	NormalizedAngle = 1.f - (Angle - AngleThresholds.GetMin()) / (AngleThresholds.GetMax() - AngleThresholds.GetMin());
	
	// Are they within the right distance?
	Dist = FMath::Abs( BestDistanceOffset - ToTarget.Length() );
	if ((Dist < DistanceThresholds.GetMin()) || (Dist > DistanceThresholds.GetMax())) return false;

	// Favor smaller distances
	NormalizedDist = 1.f - (Dist - DistanceThresholds.GetMin()) / (DistanceThresholds.GetMax() - DistanceThresholds.GetMin());

	// Check if target is on screen
	if (bRequireOnScreen)
	{
		const FVector WorldLoc = CheckTarget->GetOwner() ? CheckTarget->GetOwner()->GetActorLocation() : CheckTarget->GetComponentLocation();
		FVector2D ScreenSpacePos;
		int32 X,Y;
		UGameplayStatics::GetPlayerController(Instigator->GetWorld(), 0)->ProjectWorldLocationToScreen(WorldLoc, ScreenSpacePos);
		UGameplayStatics::GetPlayerController(Instigator->GetWorld(), 0)->GetViewportSize(X,Y);
		if ((ScreenSpacePos.X < 0.f || ScreenSpacePos.X > X) || (ScreenSpacePos.Y < 0.f || ScreenSpacePos.Y > Y))
		{
			//CheckTarget->WasRecentlyRendered() Probably a bad choice since we may be checking for objects that are "invisible" and just wanna know if the location itself is on screen
			UE_VLOG_LOCATION(Instigator.Get(), VLogTargetingSystem, Log, CheckTarget->GetComponentLocation() + FVector::UpVector * 25.f, 10.f, FColor::Red, TEXT("Target Off Screen"));
			return false;
		}
	}
	
	// Check line of sight only after every other check passes
	if (bRequireLineOfSight)
	{
		const FVector StartPosition = Instigator->GetActorLocation();
		const FVector EndPosition = CheckTarget->GetComponentLocation();

		FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(TargetingLoS), false, Instigator.Get());
		QueryParams.AddIgnoredActor(CheckTarget->GetOwner());
		FCollisionResponseParams ResponseParam;
		constexpr ECollisionChannel CollisionChannel = ECC_Visibility;

		FHitResult OutHit;
		if (Instigator->GetWorld()->SweepSingleByChannel(OutHit, StartPosition, EndPosition, FQuat::Identity, CollisionChannel, FCollisionShape::MakeSphere(30.f), QueryParams, ResponseParam))
		{
			if (OutHit.GetActor() != CheckTarget->GetOwner())
			{
				UE_VLOG_LOCATION(Instigator.Get(), VLogTargetingSystem, Log, CheckTarget->GetComponentLocation() + FVector::UpVector * 50.f, 10.f, FColor::Red, TEXT("No LOS To Target"));
				return false;
			}
		}
	}

	return true;
}

FVector FTargetingSettings::GetCenterPosition() const
{
	const FVector Offset = GetBasis(DistanceOffsetReferenceFrame).RotateVector(DistanceCenterOffset);
	return Instigator->GetActorLocation() + Offset;
}

float FTargetingSettings::GetSphereRadius() const
{
	return DistanceThresholds.GetMax();
}

FRotator FTargetingSettings::GetBasis(ETargetingReferenceFrame RefFrame) const
{
	switch (RefFrame)
	{
		case ETargetingReferenceFrame::Actor:
			return Instigator->GetActorRotation();
		case ETargetingReferenceFrame::Camera:
			// Player controlled?
			if (Cast<APlayerController>(Instigator->GetInstigatorController()))
			{
				// Maybe a generic 'return control rotation' here
				const auto PCM = UGameplayStatics::GetPlayerCameraManager(Instigator.Get(), 0);
				return PCM->GetCameraRotation();
			}
			return Instigator->GetActorRotation();
		case ETargetingReferenceFrame::Input:
			if (auto PawnInstigator = Cast<APawn>(Instigator))
			{
				const FVector InputDir = PawnInstigator->GetLastMovementInputVector();
				if (!InputDir.IsZero())
				{
					return UKismetMathLibrary::MakeRotFromXZ(InputDir.GetSafeNormal(), FVector::UpVector);
				}
			}
			return GetBasis(BackupAngleReferenceFrame);	
	}

	return FRotator::ZeroRotator;
}


void FTargetingSettings::DoVisualLog() const
{
#if ENABLE_VISUAL_LOG
	if (!FVisualLogger::IsRecording()) return;

	const FVector CenterPos = GetCenterPosition();
	const FVector AngleBasis = GetBasis(AngleReferenceFrame).Vector();

	// For valid and invalid targets. Valid in green, invalid in red
	//UE_VLOG_LOCATION(Instigator, VLogTargetingSystem, Log, GetCenterPosition(), GetAngleBasis().Vector(), )

	// For the overlap, center, basis forward as direction, angle thresholds to calculate the angle
	// Full Valid Area
	UE_VLOG_LOCATION(Instigator.Get(), VLogTargetingSystem, Log, CenterPos, DistanceThresholds.GetMax(), FColor::Green, TEXT(""));
	//UE_VLOG_CONE(Instigator.Get(), VLogTargetingSystem, Log, CenterPos, AngleBasis, DistanceThresholds.GetMax(), AngleThresholds.GetMax(), FColor::Green, TEXT("Full Valid Area"));
	// Full Valid Area - Angle Invalid Area
	UE_VLOG_CONE(Instigator.Get(), VLogTargetingSystem, Log, CenterPos, AngleBasis, DistanceThresholds.GetMax(), AngleThresholds.GetMin(), FColor::Red, TEXT(""));
	// Full Valid Area - Distance Invalid Area
	//UE_VLOG_CONE(Instigator.Get(), VLogTargetingSystem, Log, CenterPos, AngleBasis, DistanceThresholds.GetMin(), AngleThresholds.GetMax(), FColor::Red, TEXT("Invalid Distance Area"));
	UE_VLOG_CONE(Instigator.Get(), VLogTargetingSystem, Log, CenterPos, AngleBasis, DistanceThresholds.GetMax(), AngleThresholds.GetMax(), FColor::Red, TEXT(""));

	// Draw 2 but diff distances, one in red one in green for distance threshold visualization
#endif
}
