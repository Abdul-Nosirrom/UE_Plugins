// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CoreMathLibrary.generated.h"

/**
 * 
 */
UCLASS()
class COREFRAMEWORK_API UCoreMathLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	/*--------------------------------------------------------------------------------------------------------------
	* General Physics
	*--------------------------------------------------------------------------------------------------------------*/

	/// @brief  Get the velocity required to move [MoveDelta] amount in [DeltaTime] time.
	/// @param  MoveDelta Movement Delta To Compute Velocity
	/// @param  DeltaTime Delta Time In Which Move Would Be Performed
	/// @return Velocity corresponding to MoveDelta/DeltaTime if valid delta time was passed through
	UFUNCTION(Category="Physics Calculations", BlueprintCallable)
	static FORCEINLINE FVector GetVelocityFromMovement(FVector MoveDelta, float DeltaTime) 
	{
		if (DeltaTime <= 0.f) return FVector::ZeroVector;
		return MoveDelta / DeltaTime;
	}

	/*--------------------------------------------------------------------------------------------------------------
	* General Vector Maths
	*--------------------------------------------------------------------------------------------------------------*/
	
	/// @brief  Given some surface normal and a direction, will compute the component of that direction tangent to the surface
	UFUNCTION(Category="Vector Math", BlueprintPure)
	static FORCEINLINE FVector GetDirectionTangentToSurface(const FVector& Direction, const FVector& SurfaceNormal)
	{
		// Cross with an arbitrary vector
		const FVector DirectionRight = Direction ^ (FVector(0.2f, 0.1f, 0.8f).GetSafeNormal());
		return (SurfaceNormal ^ DirectionRight).GetSafeNormal();
	}

	/// @brief  Computes the distance between two points along a specified axis
	UFUNCTION(Category="Vector Math", BlueprintPure)
	static FORCEINLINE float DistanceAlongAxis(const FVector& From, const FVector& To, const FVector& Axis)
	{
		return (To - From) | Axis;
	}

	/*--------------------------------------------------------------------------------------------------------------
	* Curve Evaluation
	*--------------------------------------------------------------------------------------------------------------*/

	//static FVector 
};
