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

	UFUNCTION(Category="Input Calculations", Blueprintable)
	static FQuat ComputeRelativeInputVector(const APawn* Pawn);

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
	* Equation Solving
	*--------------------------------------------------------------------------------------------------------------*/

	/// @brief  Given the parameters of a quadratic equation, solve it. Returns the number of solutions, solutions are
	///			put in @param Roots
	UFUNCTION(Category=Equations, BlueprintCallable)
	static FORCEINLINE uint8 FindQuadraticRoots(float a, float b, float c, FVector2D& Roots)
	{
		const float discriminant = b * b - 4 * a * c;

		if (discriminant > 0)
		{
			Roots.X = (-b + sqrt(discriminant)) / (2 * a);
			Roots.Y = (-b - sqrt(discriminant)) / (2 * a);
			return 2;
		}
		if (discriminant == 0)
		{
			Roots.X = Roots.Y = -b / (2 * a);
			return 1;
		}
		// no real solution
		return 0;
	}

	/*--------------------------------------------------------------------------------------------------------------
	* General Vector Maths
	*--------------------------------------------------------------------------------------------------------------*/
	
	/// @brief  Given some surface normal and a direction, will compute the component of that direction tangent to the surface
	UFUNCTION(Category="Vector Math", BlueprintPure)
	static FORCEINLINE FVector GetDirectionTangentToSurface(const FVector& Direction, const FVector& LocalUp, const FVector& SurfaceNormal)
	{
		// Cross with an arbitrary vector
		const FVector DirectionRight = Direction ^ (LocalUp.GetSafeNormal());
		return (SurfaceNormal ^ DirectionRight).GetSafeNormal();
	}

	/// @brief  Computes the distance between two points along a specified axis
	UFUNCTION(Category="Vector Math", BlueprintPure)
	static FORCEINLINE float DistanceAlongAxis(const FVector& From, const FVector& To, const FVector& Axis)
	{
		return (To - From) | Axis;
	}

	// From Jonathan, Shaun & Geof [https://forums.unrealengine.com/t/there-is-any-way-to-do-a-slerp-between-two-fvectors/381560/6]
	UFUNCTION(Category="Vector Math", BlueprintPure)
	static FORCEINLINE FVector VectorSlerp(const FVector& From, const FVector& To, float Amount)
	{
		// Clamp alpha to not overshoot
		Amount = FMath::Clamp(Amount, 0, 1);
		
		// Dot product - the cosine of the angle between 2 vectors.
		float dot = FMath::Clamp(From.GetSafeNormal() | To.GetSafeNormal(), -1.f, 1.f);     
		// And multiplying that by percent returns the angle between
		// start and the final result.
		float theta = (FMath::Acos(dot)) * Amount;
		FVector RelativeVec = To - From * dot;
		RelativeVec.Normalize();     // Orthonormal basis
		// The final result.
		return ((From*FMath::Cos(theta)) + (RelativeVec*FMath::Sin(theta)));
	}

	UFUNCTION(Category="Vector Math", BlueprintPure)
	static FORCEINLINE FVector ReflectionAlongPlane(const FVector& Vector, const FVector& PlaneNormal)
	{
		return Vector - 2 * (Vector | PlaneNormal.GetSafeNormal()) * PlaneNormal.GetSafeNormal();
	}

	UFUNCTION(Category="Vector Math", BlueprintCallable)
	static FORCEINLINE void SetVectorAxisValue(const FVector& Axis, FVector& Dest, const FVector& Src)
	{
		Dest = FVector::VectorPlaneProject(Dest, Axis.GetSafeNormal()) + Src.ProjectOnToNormal(Axis.GetSafeNormal());
	}

	UFUNCTION(Category="Vector Math", BlueprintPure)
	static FORCEINLINE FVector VInterpAxisValues(const FRotator& Basis, const FVector& From, const FVector& To, const FVector& Alpha)
	{
		const FVector X = FMath::VInterpTo(From.ProjectOnToNormal(Basis.Quaternion().GetAxisX()), To.ProjectOnToNormal(Basis.Quaternion().GetAxisX()), 1, Alpha.X);
		const FVector Y = FMath::VInterpTo(From.ProjectOnToNormal(Basis.Quaternion().GetAxisY()), To.ProjectOnToNormal(Basis.Quaternion().GetAxisY()), 1, Alpha.Y);
		const FVector Z = FMath::VInterpTo(From.ProjectOnToNormal(Basis.Quaternion().GetAxisZ()), To.ProjectOnToNormal(Basis.Quaternion().GetAxisZ()), 1, Alpha.Z);
		return X + Y + Z;
	}

	/*--------------------------------------------------------------------------------------------------------------
	* Rotation Helpers
	*--------------------------------------------------------------------------------------------------------------*/

	/// @brief  Given two vectors, returns the rotation that'll align From with To
	UFUNCTION(Category="Rotation Math", BlueprintPure)
	static FORCEINLINE FQuat FromToRotation(const FVector& From, const FVector& To)
	{
		const FVector Axis = FVector::CrossProduct(From, To).GetSafeNormal();
		const float Angle = FMath::Acos(From.GetSafeNormal() | To.GetSafeNormal());
		return FQuat(Axis, Angle);
	}

	/*--------------------------------------------------------------------------------------------------------------
	* Curve Evaluation
	*--------------------------------------------------------------------------------------------------------------*/

	//static FVector

	/*--------------------------------------------------------------------------------------------------------------
	* Interpolation Functions
	*--------------------------------------------------------------------------------------------------------------*/

	static FORCEINLINE float EasingQuartic(float Percent)
	{
		return 1.f - ((Percent -= 1.f)*Percent*Percent*Percent);
	}
	
	static FORCEINLINE FVector ParabolaInterp(FVector Start, FVector End, float Height, float Percent)
	{
		auto F = [] (float x, float H)
		{
			return -4 * H * x * x + 4 * H * x;	
		};
		
		FVector Mid = FMath::VInterpTo(Start, End, 1, Percent);

		return FVector(Mid.X, Mid.Y, F(Percent, Height) + FMath::FInterpTo(Start.Z, End.Z, 1, Percent));
	}

	static FORCEINLINE FVector ParabolaEasingInterp(FVector Start, FVector End, float Height, float Percent)
	{
		auto F = [] (float x, float H)
		{
			return -4 * H * x * x + 4 * H * x;	
		};

		FVector Mid = FMath::VInterpTo(Start, End, 1, EasingQuartic(Percent));

		return FVector(Mid.X, Mid.Y, F(Percent, Height) + FMath::FInterpTo(Start.Z, End.Z, 1, Percent));

	}
};