// Copyright 2023 CoC All rights reserved


#include "MeshExtrusionHelpers.h"
#include "Components/SplineComponent.h"

/*--------------------------------------------------------------------------------------------------------------
* Straight
*--------------------------------------------------------------------------------------------------------------*/


FVector FStraightExtrusion::Transform(const FVector& InVector) const
{
	// Straight extrusion, no modifications
	return InVector + FVector::UpVector * CurrentHeight;
}

void FStraightExtrusion::UpdateBasis(float Percent)
{
	Xn = FVector3f::ForwardVector;
	Yn = FVector3f::RightVector;
	Zn = FVector3f::UpVector;

	CurrentHeight = HeightSpiral * (bUseSpiralCurve && SpiralCurve.GetRichCurveConst()->HasAnyData() ? SpiralCurve.GetRichCurveConst()->Eval(Percent * SpiralCurve.GetRichCurveConst()->GetLastKey().Time) : Percent);
}

/*--------------------------------------------------------------------------------------------------------------
*  Rotation
*--------------------------------------------------------------------------------------------------------------*/

void FRotationExtrusion::Initialize(FVector2D InRadius, float TotalAngle, float InRotationOffset, float InSpiralHeight, bool bInUseSpiralCurve, FRuntimeFloatCurve InSpiralCurve, bool bCenterPivot)
{
	Radius = InRadius;
	RotationAngle = FMath::Abs(TotalAngle);
	SpiralHeight = InSpiralHeight;
	bUseSpiralCurve = bInUseSpiralCurve;
	SpiralCurve = InSpiralCurve;
	AngleOffset = InRotationOffset + (TotalAngle < 0.f ? TotalAngle : 0.f);
	
	bRotatingInwards = TotalAngle < 0.f;
	if (bRotatingInwards) RotationSign = -1.f;
	else RotationSign = 1.f;

	
	if (bCenterPivot)	RotationCenter = FVector::ZeroVector;
	else				RotationCenter = RotationSign * FVector::ForwardVector * Radius.X;
}

FVector FRotationExtrusion::Transform(const FVector& InVector) const
{
	// The below represents the offset from the circle perimeter, so we take the point this is supposed to be + this! Then we can subtract the pivot to reorient it
	if (RotationAngle == 0.f)
		return X * InVector.X + Y * InVector.Y + Z * InVector.Z;

	return X * InVector.X + Z * InVector.Z + PointOnArc - RotationCenter; // Thank you Ani!
}

void FRotationExtrusion::UpdateBasis(float Percent)
{
	const float Angle = FMath::Abs(RotationAngle) * Percent + AngleOffset;
	
	X = FVector::ForwardVector * FMath::Cos(Angle) + FVector::RightVector * FMath::Sin(Angle);
	Y = -FVector::ForwardVector * FMath::Sin(Angle) + FVector::RightVector * FMath::Cos(Angle);
	Z = FVector::UpVector;
	
	Xn = FVector3f(X);
	Yn = FVector3f(Y);
	Zn = FVector3f(Z);

	PointOnArc = RotationSign * X * (Radius.X * FVector::ForwardVector + Radius.Y * FVector::RightVector) + Z * SpiralHeight * (bUseSpiralCurve && SpiralCurve.GetRichCurveConst()->HasAnyData() ? SpiralCurve.GetRichCurveConst()->Eval(Percent * SpiralCurve.GetRichCurveConst()->GetLastKey().Time) : Percent);;
}

/*--------------------------------------------------------------------------------------------------------------
* Splines
*--------------------------------------------------------------------------------------------------------------*/


FVector FSplineExtrusion::Transform(const FVector& InVector) const
{
	return X * InVector.X + Z * InVector.Z + CurrentSplinePoint;
}

void FSplineExtrusion::UpdateBasis(float Percent)
{
	const float CurrentDist = Spline->GetSplineLength() * Percent;

	const FTransform Transform = LocalOffset * Spline->GetTransformAtDistanceAlongSpline(CurrentDist, ESplineCoordinateSpace::Local);
	const FQuat Quat = Transform.GetRotation();

	X = Quat.GetAxisY();
	Y = Quat.GetAxisX();
	Z = Quat.GetAxisZ();

	Xn = FVector3f(X);
	Yn = FVector3f(Y);
	Zn = FVector3f(Z);

	CurrentSplinePoint = Transform.GetLocation();

	//CachedPointTransform = Transform * LocalOffset;
}
