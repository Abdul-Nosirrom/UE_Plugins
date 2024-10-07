// Copyright 2023 CoC All rights reserved


#include "StaticLibraries/CharacterSweepLibrary.h"

#include "RadicalCharacter.h"

namespace SweepLibCVars
{
#if ALLOW_CONSOLE && !NO_LOGGING 
	int32 DisableDebugDraws = 0;
	FAutoConsoleVariableRef CVarDisableDebugDraw
	(
		TEXT("sweepLib.DisableDebugDraw"),
		DisableDebugDraws,
		TEXT("Disable Debug Draws. 0: Enable, 1: Disable"),
		ECVF_Default
	);
#endif
}

bool UCharacterSweepLibrary::ShapeSweepSingle(ARadicalCharacter* Character, FVector LocationOffset, FQuat Rotation, FVector Direction, float Distance,
                                              const FCollisionShape Shape, FHitResult& OutHit, float DebugTime)
{
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ShapeSweep), false, Character);
	FCollisionResponseParams ResponseParam;
	Character->GetCapsuleComponent()->InitSweepCollisionParams(QueryParams, ResponseParam);
	const ECollisionChannel CollisionChannel = Character->GetCapsuleComponent()->GetCollisionObjectType();

	const FVector StartLocation = Character->GetActorLocation() + LocationOffset;
	const FVector EndLocation = StartLocation + Direction.GetSafeNormal() * Distance;

	const bool bSweepResult = Character->GetWorld()->SweepSingleByChannel(OutHit, StartLocation, EndLocation, FQuat::Identity, CollisionChannel, Shape, QueryParams, ResponseParam);

	DrawSweepDebug(Character->GetWorld(), StartLocation, EndLocation, Rotation, OutHit, Shape, DebugTime);

	return bSweepResult;
}

bool UCharacterSweepLibrary::CharacterSweepSingle(ARadicalCharacter* Character, FVector LocationOffset, FQuat Rotation,
	FVector Direction, float Distance, FHitResult& OutHit, float Inflate, float DebugTime)
{
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ShapeSweep), false, Character);
	FCollisionResponseParams ResponseParam;
	Character->GetCapsuleComponent()->InitSweepCollisionParams(QueryParams, ResponseParam);
	const ECollisionChannel CollisionChannel = Character->GetCapsuleComponent()->GetCollisionObjectType();
	const FCollisionShape Shape = Character->GetCapsuleComponent()->GetCollisionShape(Inflate);
	const FVector StartLocation = Character->GetActorLocation() + LocationOffset;
	const FVector EndLocation = StartLocation + Direction.GetSafeNormal() * Distance;

	const bool bSweepResult = Character->GetWorld()->SweepSingleByChannel(OutHit, StartLocation, EndLocation, Rotation, CollisionChannel, Shape, QueryParams, ResponseParam);

	DrawSweepDebug(Character->GetWorld(), StartLocation, EndLocation, Rotation, OutHit, Shape, DebugTime);

	return bSweepResult;
}

bool UCharacterSweepLibrary::SphereSweepSingle(ARadicalCharacter* Character, FVector LocationOffset, FVector Direction,
	float Distance, float Radius, FHitResult& OutHit, float DebugTime)
{
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ShapeSweep), false, Character);
	FCollisionResponseParams ResponseParam;
	Character->GetCapsuleComponent()->InitSweepCollisionParams(QueryParams, ResponseParam);
	const ECollisionChannel CollisionChannel = Character->GetCapsuleComponent()->GetCollisionObjectType();

	const FCollisionShape Shape = FCollisionShape::MakeSphere(Radius);
	const FVector StartLocation = Character->GetActorLocation() + LocationOffset;
	const FVector EndLocation = StartLocation + Direction.GetSafeNormal() * Distance;

	const bool bSweepResult = Character->GetWorld()->SweepSingleByChannel(OutHit, StartLocation, EndLocation, FQuat::Identity, CollisionChannel, Shape, QueryParams, ResponseParam);

	DrawSweepDebug(Character->GetWorld(), StartLocation, EndLocation, FQuat::Identity, OutHit, Shape, DebugTime);

	return bSweepResult;
}

bool UCharacterSweepLibrary::LineSweepSingle(ARadicalCharacter* Character, FVector LocationOffset, FVector Direction,
	float Distance, FHitResult& OutHit, float DebugTime)
{
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ShapeSweep), false, Character);
	FCollisionResponseParams ResponseParam;
	Character->GetCapsuleComponent()->InitSweepCollisionParams(QueryParams, ResponseParam);
	const ECollisionChannel CollisionChannel = Character->GetCapsuleComponent()->GetCollisionObjectType();
	const FVector StartLocation = Character->GetActorLocation() + LocationOffset;
	const FVector EndLocation = StartLocation + Direction.GetSafeNormal() * Distance;

	const bool bSweepResult = Character->GetWorld()->LineTraceSingleByChannel(OutHit, StartLocation, EndLocation, CollisionChannel, QueryParams, ResponseParam);

	FCollisionShape DebugShape;
	DebugShape.ShapeType = ECollisionShape::Line;
	DrawSweepDebug(Character->GetWorld(), StartLocation, EndLocation, FQuat::Identity, OutHit, DebugShape, DebugTime);

	return bSweepResult;
}

void UCharacterSweepLibrary::DrawSweepDebug(const UWorld* World, const FVector& StartLocation,
                                            const FVector& EndLocation, const FQuat& Rotation, const FHitResult& HitResult, const FCollisionShape& Shape, float DebugTime)
{
#if ALLOW_CONSOLE && !NO_LOGGING 
	// Debug Draw
	if (DebugTime >= 0.f && SweepLibCVars::DisableDebugDraws > 0)
	{
		FVector FinalLoc = HitResult.bBlockingHit ? HitResult.Location : EndLocation;
		FColor HitColor = HitResult.bBlockingHit ? FColor::Green : FColor::Red;
		DrawDebugLine(World, StartLocation, FinalLoc, HitColor, false, DebugTime, 0, 5.f);

		if (Shape.IsCapsule())
		{
			DrawDebugCapsule(World, StartLocation, Shape.GetCapsuleHalfHeight(), Shape.GetCapsuleRadius(), Rotation, FColor::Red, false, DebugTime, 0, 5.f);
			DrawDebugCapsule(World, FinalLoc, Shape.GetCapsuleHalfHeight(), Shape.GetCapsuleRadius(), Rotation, HitColor, false, DebugTime, 0, 5.f);
		}
		else if (Shape.IsSphere())
		{
			DrawDebugSphere(World, StartLocation, Shape.GetSphereRadius(), 16, FColor::Red, false, DebugTime, 0, 5.f);
			DrawDebugSphere(World, FinalLoc, Shape.GetSphereRadius(), 16, HitColor, false, DebugTime, 0, 5.f);
		}
		else if (Shape.IsBox())
		{
			DrawDebugBox(World, StartLocation, Shape.GetExtent(), FColor::Red, false, DebugTime, 0, 5.f);
			DrawDebugBox(World, FinalLoc, Shape.GetExtent(), HitColor, false, DebugTime, 0, 5.f);
		}
		else if (Shape.IsLine())
		{
			DrawDebugPoint(World, FinalLoc, 15.f, HitColor, false, DebugTime, 0);
		}
	}
#endif 
}
