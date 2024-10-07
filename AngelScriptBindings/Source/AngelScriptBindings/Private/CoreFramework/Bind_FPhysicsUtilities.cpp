#pragma once
#include "CoreMinimal.h"

//#ifdef WITH_ANGELSCRIPT_HAZE
#include "AngelscriptBinds.h"
#include "AngelscriptManager.h"
#include "Helpers/PhysicsUtilities.h"

AS_FORCE_LINK const FAngelscriptBinds::FBind Bind_FCurveMovementParams(FAngelscriptBinds::EOrder::Late, []
{
	auto FCurveMovementParams_ = FAngelscriptBinds::ExistingClass("FCurveMovementParams");

	FCurveMovementParams_.Method("void Init(URadicalMovementComponent MovementComponent, float32 InDuration)",
		METHODPR_TRIVIAL(void, FCurveMovementParams, Init, (URadicalMovementComponent*, float)));
	FCurveMovementParams_.Method("void DeInit(URadicalMovementComponent MovementComponent)",
		METHODPR_TRIVIAL(void, FCurveMovementParams, DeInit, (URadicalMovementComponent*)));

	// Bind properties
	FCurveMovementParams_.Property("FVector EntryVelocity", &FCurveMovementParams::EntryVelocity);
	FCurveMovementParams_.Property("FQuat EntryRotation", &FCurveMovementParams::EntryRotation);
	FCurveMovementParams_.Property("FVector TargetVelForward", &FCurveMovementParams::TargetVelForward);
	FCurveMovementParams_.Property("FVector TargetVelUp", &FCurveMovementParams::TargetVelUp);
	FCurveMovementParams_.Property("FQuat RotationReferenceFrame", &FCurveMovementParams::RotationReferenceFrame);
	FCurveMovementParams_.Property("EMovementState InitialMovementState", &FCurveMovementParams::InitialMovementState);
	FCurveMovementParams_.Property("bool bActive", &FCurveMovementParams::bActive);
	FCurveMovementParams_.Property("float32 TotalDuration", &FCurveMovementParams::TotalDuration);
	FCurveMovementParams_.Property("float32 CurrentDuration", &FCurveMovementParams::CurrentDuration);
	
	// Custom method for binding to non-dynamic event
	
	FCurveMovementParams_.Method("void BindOnEvalCompleted(UObject object, FName function_name)", [](FCurveMovementParams* curveParams, UObject* object, FName function_name)
	{
		// Verify function name exists on the class
		if (object)
		{
			UFunction* func = object->FindFunction(function_name);
			if (!func || func->NumParms != 0)
			{
				FAngelscriptManager::Throw("No matching function signature found for CurveEvalCompleted [signature void f()]");
			}
			else
			{
				curveParams->CurveEvalComplete.BindUFunction(object, function_name);
			}
		}
	});
	
	FCurveMovementParams_.Method("void UnBindOnEvalCompleted()", [](FCurveMovementParams* curveParams)
	{
		curveParams->CurveEvalComplete.Unbind();
	});
});

AS_FORCE_LINK const FAngelscriptBinds::FBind Bind_FPhysicsSplineFollower(FAngelscriptBinds::EOrder::Late, []
{
	auto FPhysicsSplineFollower_ = FAngelscriptBinds::ExistingClass("FPhysicsSplineFollower");

	FPhysicsSplineFollower_.Constructor("void f(const float32 InMaxSpeed, const float32 InUpGrav, const float32 InDownGrav)",
		[](FPhysicsSplineFollower* Address, const float InMaxSpeed, const float InUpGrav, const float InDownGrav)
	{
		new(Address)FPhysicsSplineFollower(InMaxSpeed, InUpGrav, InDownGrav);
	});
	FAngelscriptBinds::SetPreviousBindNoDiscard(true);
	SCRIPT_TRIVIAL_NATIVE_CONSTRUCTOR(FPhysicsSplineFollower_, "FPhysicsSplineFollower");

	FPhysicsSplineFollower_.Method("void Init(USplineComponent TargetSpline, AActor TargetActor, const FVector& StartLocation, float32 InitialSpeed)",
		METHODPR_TRIVIAL(void, FPhysicsSplineFollower, Init, (USplineComponent*, AActor*, const FVector&, float)));
	FPhysicsSplineFollower_.Method("void UpdateFollower(float32 DeltaTime)",
		METHODPR_TRIVIAL(void, FPhysicsSplineFollower, UpdateFollower, (float)));
	FPhysicsSplineFollower_.Method("bool IsAtEndOfSpline() const",
		METHODPR_TRIVIAL(bool, FPhysicsSplineFollower, IsAtEndOfSpline, () const));

	// Properties
	FPhysicsSplineFollower_.Property("float32 FollowSpeed", &FPhysicsSplineFollower::FollowSpeed);
	FPhysicsSplineFollower_.Property("float32 CurrentDistance", &FPhysicsSplineFollower::CurrentDistance);
	FPhysicsSplineFollower_.Property("float32 TotalDistance", &FPhysicsSplineFollower::TotalDistance);
	FPhysicsSplineFollower_.Property("FVector CurrentTangent", &FPhysicsSplineFollower::CurrentTangent);
	FPhysicsSplineFollower_.Property("FTransform FollowerTransform", &FPhysicsSplineFollower::FollowerTransform);
	FPhysicsSplineFollower_.Property("FVector LocalOffset", &FPhysicsSplineFollower::LocalOffset);
	FPhysicsSplineFollower_.Property("float32 MaxSpeedConstraint", &FPhysicsSplineFollower::MaxSpeedConstraint);
	FPhysicsSplineFollower_.Property("float32 UpGravityScale", &FPhysicsSplineFollower::UpGravityScale);
	FPhysicsSplineFollower_.Property("float32 DownGravityScale", &FPhysicsSplineFollower::DownGravityScale);

});
