// Copyright 2023 CoC All rights reserved

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CharacterSweepLibrary.generated.h"

class ARadicalCharacter;

UCLASS()
class COREFRAMEWORK_API UCharacterSweepLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	
	static bool ShapeSweepSingle(ARadicalCharacter* Character, FVector LocationOffset, FQuat Rotation, FVector Direction, float Distance, const FCollisionShape Shape, FHitResult& OutHit, float DebugTime = -1.f);

	UFUNCTION(Category="Character|Sweep", BlueprintCallable)
	static bool CharacterSweepSingle(ARadicalCharacter* Character, FVector LocationOffset, FQuat Rotation, FVector Direction, float Distance, FHitResult& OutHit, float Inflate = 0.f, float DebugTime = -1.f);

	UFUNCTION(Category="Character|Sweep", BlueprintCallable)
	static bool SphereSweepSingle(ARadicalCharacter* Character, FVector LocationOffset, FVector Direction, float Distance, float Radius, FHitResult& OutHit, float DebugTime = -1.f);

	UFUNCTION(Category="Character|Sweep", BlueprintCallable)
	static bool LineSweepSingle(ARadicalCharacter* Character, FVector LocationOffset, FVector Direction, float Distance, FHitResult& OutHit, float DebugTime = -1.f);

private:
	static void DrawSweepDebug(const UWorld* World, const FVector& StartLocation, const FVector& EndLocation, const FQuat& Rotation, const FHitResult& HitResult, const FCollisionShape& Shape, float DebugTime);
};
