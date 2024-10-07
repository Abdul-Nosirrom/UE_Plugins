// Copyright 2023 CoC All rights reserved

#pragma once

#include "CoreMinimal.h"


UENUM(BlueprintType)
enum class EExtrusionMethods : uint8
{
	Straight,
	Rotation,
	Spline
};

struct FExtrusionBasis
{
	FExtrusionBasis() = default;
	virtual ~FExtrusionBasis() = default;
			
	virtual FVector Transform(const FVector& InVector) const { return InVector; }
	virtual FVector Transform(float inX, float inY, float inZ) const { return Transform(FVector(inX,inY,inZ));}

	virtual void UpdateBasis(float Percent) {}

	FVector X, Y, Z; // Point on extrusion path
	FVector3f Xn, Yn, Zn; // Normals
};
		
struct FStraightExtrusion : FExtrusionBasis
{
	void Initialize(float TotalLength, float InHeightSpiral, bool bInUseSpiralCurve, FRuntimeFloatCurve InSpiralCurve)
	{
		HeightSpiral = InHeightSpiral;
		bUseSpiralCurve = bInUseSpiralCurve;
		SpiralCurve = InSpiralCurve;
		ExtrusionLength = TotalLength;
	}

	virtual FVector Transform(const FVector& InVector) const override;
	virtual void UpdateBasis(float Percent) override;
	virtual FVector Transform(float inX, float inY, float inZ) const override { return Transform(FVector(inX,inY,inZ));}

	float ExtrusionLength;
	float HeightSpiral;
	bool bUseSpiralCurve;
	FRuntimeFloatCurve SpiralCurve;

	float CurrentHeight = 0.f;
};
		
struct FRotationExtrusion : FExtrusionBasis
{
	void Initialize(FVector2D InRadius, float TotalAngle, float InRotationOffset, float InSpiralHeight, bool bInUseSpiralCurve, FRuntimeFloatCurve InSpiralCurve, bool bCenterPivot);

	virtual FVector Transform(const FVector& InVector) const override;
	virtual void UpdateBasis(float Percent) override;
	virtual FVector Transform(float inX, float inY, float inZ) const override { return Transform(FVector(inX,inY,inZ));}


	float RotationAngle;
	float AngleOffset;
	float SpiralHeight;
	FVector2D Radius;

	FVector PointOnArc;
	FVector RotationCenter;
	bool bRotatingInwards;
	float RotationSign;

	bool bUseSpiralCurve;
	FRuntimeFloatCurve SpiralCurve;
};

struct FSplineExtrusion : FExtrusionBasis
{
	void Initialize(class USplineComponent* InSpline, const FTransform& InOffset) { Spline = InSpline; LocalOffset = InOffset; }

	virtual FVector Transform(const FVector& InVector) const override;
	virtual void UpdateBasis(float Percent) override;
	virtual FVector Transform(float inX, float inY, float inZ) const override { return Transform(FVector(inX,inY,inZ));}

	FVector CurrentSplinePoint;
	FTransform LocalOffset;
	FTransform CachedPointTransform;
	USplineComponent* Spline;
};
	
