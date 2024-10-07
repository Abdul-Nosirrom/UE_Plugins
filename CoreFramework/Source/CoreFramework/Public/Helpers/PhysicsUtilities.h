// Copyright 2023 CoC All rights reserved

#pragma once

#include "CoreMinimal.h"
#include "Animation/CharacterNotifyState.h"
#include "PhysicsUtilities.generated.h"

#pragma region Movement Curve Thing
enum ERotationMethod : int;
enum EMovementState : int;

DECLARE_DELEGATE(FPhysicsCurveEvalComplete)

UENUM()
enum class EPhysicsCurveType : uint8 
{
	Position,
	Velocity
};

/// @brief WorldUp is used in the case of the Input reference frames
UENUM()
enum class EPhysicsCurveRefFrame : uint8 
{
	Local			UMETA(DisplayName="Local"),
	Input			UMETA(DisplayName="Input"),
	InitialLocal	UMETA(DisplayName="Local To Activation Frame"),
	InitialInput	UMETA(DisplayName="Input In Activation Frame"),
	World			UMETA(DisplayName="World")
};

UENUM()
enum class EPhysicsCurveRotationMethod : uint8
{
	None,
	Physics,
	Curve
};

USTRUCT(BlueprintType)
struct COREFRAMEWORK_API FCurveMovementParams
{
	GENERATED_BODY()

	UPROPERTY(Category=Settings, EditAnywhere, ScriptReadWrite)
	EPhysicsCurveType CurveType;
	UPROPERTY(Category=Settings, EditAnywhere, ScriptReadWrite)
	EPhysicsCurveRefFrame ReferenceFrame;
	/// @brief	If true, will make it so the Z values on the movement curve are not applied when grounded where they'd normally result in the target going aerial
	UPROPERTY(Category=Settings, EditAnywhere, ScriptReadWrite)
	bool bMaintainPhysicsState = false;
	
	UPROPERTY(Category=Movement, EditAnywhere, ScriptReadWrite)
	FAnimVectorCurve MovementCurve;
	/// @brief	Blend time from our actual velocity to whatever is specified by the curve. 0 For no blending on that axis
	UPROPERTY(Category=Movement, EditAnywhere, ScriptReadWrite)
	FVector BlendInTime;
	/// @brief	When we end, we multiply our final velocity calculation by this amount.
	UPROPERTY(Category=Movement, EditAnywhere, ScriptReadWrite, meta=(UIMin=0, ClampMin=0, UIMax=1, ClampMax=1))
	float ExitVelocityDampen = 1.f;
	/// @brief	Dampen velocity to this when ending if we're over it, if negative this is ignored.
	UPROPERTY(Category=Movement, EditAnywhere, ScriptReadWrite, meta=(UIMin=0, ClampMin=0))
	float MaxExitSpeed = -1.f;
	/// @brief  Rate at which we spherically interpolate to the target input
	UPROPERTY(Category=Movement, EditAnywhere, ScriptReadWrite, meta=(UIMin=0, ClampMin=0, EditCondition="ReferenceFrame==EPhysicsCurveRefFrame::Input || ReferenceFrame==EPhysicsCurveRefFrame::InitialInput", EditConditionHides))
	float InputInterpSpeed = -1.f;
	/// @brief	Speed of which we return our up vector back to its target direction when we leave ground
	UPROPERTY(Category=Movement, EditAnywhere, ScriptReadWrite, meta=(UIMin=0, ClampMin=0))
	float AerialUpVectorReturnSpeed = 1.f;
	/// @brief	At what time for a given axis should we stop sampling the curve?
	UPROPERTY(Category=Movement, EditAnywhere, ScriptReadWrite, meta=(UIMin=0, ClampMin=0, UIMax=1, ClampMax=1))
	FVector AxisIgnore = FVector::OneVector;

	UPROPERTY(Category=Rotation, EditAnywhere, ScriptReadWrite)
	EPhysicsCurveRotationMethod RotationType;
	UPROPERTY(Category=Rotation, EditAnywhere, ScriptReadWrite, meta=(EditCondition="RotationType==EPhysicsCurveRotationMethod::Physics", EditConditionHides))
	TEnumAsByte<ERotationMethod> RotationMethodOverride;
	UPROPERTY(Category=Rotation, EditAnywhere, ScriptReadWrite, meta=(EditCondition="RotationType==EPhysicsCurveRotationMethod::Physics", EditConditionHides, UIMin=0, ClampMin=0))
	float RotationRate;
	UPROPERTY(Category=Rotation, EditAnywhere, ScriptReadWrite, meta=(EditCondition="RotationType==EPhysicsCurveRotationMethod::Curve", EditConditionHides))
	FAnimScalarCurve RotationCurve;

	/// @brief	Delegate invoked when curve evaluation is complete
	FPhysicsCurveEvalComplete CurveEvalComplete;
	
	FVector EntryVelocity;
	FQuat EntryRotation;

	FVector TargetVelForward;
	FVector TargetVelUp;
	
	FQuat RotationReferenceFrame;

	EMovementState InitialMovementState;

	bool bActive = false;
	float TotalDuration;
	float CurrentDuration;

	// Get initial input vector so we can interpolate it to the target later but initialize it to this (?)
	void Init(class URadicalMovementComponent* MovementComponent, float InDuration);
	void DeInit(URadicalMovementComponent* MovementComponent);
	
	void CalculateVelocity(URadicalMovementComponent* MovementComponent, float DeltaTime);
	void UpdateRotation(URadicalMovementComponent* MovementComponent, float DeltaTime);

	void UpdateReferenceFrame(URadicalMovementComponent* MovementComponent, float DeltaTime);
	
	bool HasNaNs(float InDuration, float& MaxOutputSpeed) const;
	FVector IntegrateVelocityCurve(float InDuration) const;

	FString ValidateCurve(float InDuration) const;
};

#pragma endregion

#pragma region Physics Spline Follower

USTRUCT(BlueprintType)
struct COREFRAMEWORK_API FPhysicsSplineFollower
{
	GENERATED_BODY()

	UPROPERTY(Transient, ScriptReadWrite)
	class USplineComponent* Spline = nullptr;
	UPROPERTY(Transient, ScriptReadWrite)
	AActor* OwnerActor = nullptr;
	
	float FollowSpeed = 0.f;
	float CurrentDistance = 0.f;
	float TotalDistance = 0.f;
	FVector CurrentTangent = FVector::ZeroVector;
	FTransform FollowerTransform = FTransform::Identity;

	FVector LocalOffset = FVector::ZeroVector;
	float MaxSpeedConstraint = 1500.f;
	float UpGravityScale = 0.f;
	float DownGravityScale = 0.f;

	FPhysicsSplineFollower() {}
	FPhysicsSplineFollower(const float InMaxSpeed, const float InUpGrav, const float InDownGrav)
		: MaxSpeedConstraint(InMaxSpeed), UpGravityScale(InUpGrav), DownGravityScale(InDownGrav) {}

	void Init(class USplineComponent* TargetSpline, AActor* TargetActor, const FVector& StartLocation, float InitialSpeed);
	void UpdateFollower(float DeltaTime);
	bool IsAtEndOfSpline() const;
};

#pragma endregion
