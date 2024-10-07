// Copyright 2023 Abdulrahmen Almodaimegh. All Rights Reserved.


#include "InputData.h"

#include "IB_LOG.h"
#include "Debug/CFW_Log.h"
#include "Subsystems/InputBufferSubsystem.h"

void UInputBufferMap::GenerateInputActions()
{
	InputActions.Empty();

	for (auto AMap : InputActionMap->GetMappings())
	{
		if (!InputActions.Contains(AMap.Action))
		{
			const auto BufferedInput = Cast<UBufferedInputAction>(AMap.Action);
			if (ensureMsgf(BufferedInput, TEXT("Provided input [%s] is not a BufferedInputAction"), *AMap.Action->ActionDescription.ToString()))
				InputActions.Add(BufferedInput);
		}
	}
}


FGameplayTagContainer UInputBufferMap::GetActionIDs()
{
	// Check cached value, if not there then we regenerate the IDs
	if (!ActionIDs.IsEmpty() && ActionIDs.Num() == InputActions.Num())
	{
		return ActionIDs;
	}

	// Regenerate
	ActionIDs = FGameplayTagContainer();

	// Safety check
	if (InputActions.IsEmpty()) GenerateInputActions();
	
	for (auto Action : InputActions)
	{
		ensureMsgf(!ActionIDs.HasTag(Action->GetID()), TEXT("INPUT BUFFER INITIALIZATION ERROR! Tag exists in multiple input actions [%s]"), *Action->GetID().ToString());
		ensureMsgf(Action->GetID().MatchesTag(TAG_Input_Axis) || Action->GetID().MatchesTag(TAG_Input_Button), TEXT("INPUT BUFFER INITIALIZATION ERROR! Action Input Tag has wrong parent specification [%s]"), *Action->GetID().ToString());
		ActionIDs.AddTag(Action->GetID());
	}

	return ActionIDs;
}

FGameplayTagContainer UInputBufferMap::GetDirectionalIDs()
{
	if (DirectionalActionMap)
	{
		// Check cached value, if not there then we regenerate the IDs
		if (!DirectionalIDs.IsEmpty() && DirectionalIDs.Num() == DirectionalActionMap->GetMappings().Num())
		{
			return DirectionalIDs;
		}

		// Regenerate
		DirectionalIDs = FGameplayTagContainer();
		
		for (auto DMap : DirectionalActionMap->GetMappings())
		{
			ensureMsgf(!DirectionalIDs.HasTag(DMap->GetID()), TEXT("INPUT BUFFER INITIALIZATION ERROR! Tag exists in multiple directional actions [%s]"), *DMap->GetID().ToString());
			ensureMsgf(DMap->GetID().MatchesTag(TAG_Input_Directional), TEXT("INPUT BUFFER INITIALIZATION ERROR! Action Input Tag has wrong parent specification [%s]"), *DMap->GetID().ToString());
			DirectionalIDs.AddTag(DMap->GetID());
		}
	}

	return DirectionalIDs;
}


bool UMotionAction::CheckMotionDirection(const FVector2D& AxisInput, const FVector& ProcessedInput, const FVector& PlayerForward, const FVector& PlayerRight)
{
	// Directional Input Describes An Angle Change
	if (bAngleChange)
	{
		// This call will update CurAngle, ensure angle change was initialized first (Meaning an earlier frame aligned with FromDirection)
		if (bInputAlignmentInitialized) // Progression should go [NEUTRAL->ALIGNED->Start Checking Angle Deltas]
		{
			if (AxisInput.IsZero())
			{
				Reset();
				return false;
			}
			UpdateCurrentAngle(AxisInput);
			return CurAngle >= AngleDelta;
		}
		else if (bAngleChangeInitialized && IsAlignedWithFacingDirection(AxisInput))
		{
			bInputAlignmentInitialized = true;
			PrevVector = GetFromDirection(); // Initialize it like this
		}
		else if (AxisInput.IsZero())
		{
			bAngleChangeInitialized = true;
		}
		return false;
	}
	else if (!MotionCommandSequence.IsEmpty()) // Ensure command sequence isn't empty, otherwise we don't have a valid seq to check for 
	{
		// We've already evaluated all sequence parts successfully here
		if (CheckStep >= MotionCommandSequence.Num())
		{
			return true;
		}

		// Check if current command step corresponds to the current input seq and increment the step for next time
		if (MotionCommandSequence[CheckStep] == GetAxisDirection(AxisInput, ProcessedInput, PlayerForward, PlayerRight))
		{
			// We increment the step check count, but don't return true yet as we still haven't satisfied the whole sequence
			CheckStep++;
		}
	}

	return false;
}

bool UMotionAction::IsAlignedWithFacingDirection(const FVector2D& AxisInput) const
{
	const FVector2D EvaluationDirection = GetFromDirection();
	
	const float Alignment = AxisInput.GetSafeNormal() | EvaluationDirection;

	return Alignment > DOT_PRODUCT_45; 
}


EMotionCommandDirection UMotionAction::GetAxisDirection(const FVector2D& AxisInput, const FVector& ProcessedInput, const FVector& PlayerForward, const FVector& PlayerRight) const
{
	if (AxisInput.IsZero())
	{
		return EMotionCommandDirection::NEUTRAL;
	}
	
	FVector ProperAxisInput(AxisInput.X, AxisInput.Y, 0);
	FVector EvaluationDirection(0, 1, 0);
	FVector RightDirection(1, 0, 0);

	if (bRelativeToPlayer)
	{
		ProperAxisInput = ProcessedInput;
		EvaluationDirection = PlayerForward;
		RightDirection = PlayerRight;
	}

	const float StickAngle = FMath::RadiansToDegrees(FMath::Acos(ProperAxisInput.GetSafeNormal() | EvaluationDirection.GetSafeNormal()));
	const bool bRightDirection = (ProperAxisInput | RightDirection) > 0;
	
	// Now evaluate the stick angle to return a command sequence step
	if (StickAngle < 45)							return EMotionCommandDirection::FORWARD;
	else if (StickAngle < 135 && bRightDirection)	return EMotionCommandDirection::RIGHT;
	else if (StickAngle < 135 && !bRightDirection)	return EMotionCommandDirection::LEFT;
	else											return EMotionCommandDirection::BACK;
}

void UMotionAction::UpdateCurrentAngle(const FVector2D& AxisInput)
{
	// Angle changes are never done relative to the player, they're always done relative to the stick input
	const FVector2D FromDirectionVector = GetFromDirection();

	// Now we're done with the set up, so compute the current angle and evaluate it against our previously computed angle
	const float StickAngle = FMath::RadiansToDegrees(FMath::Acos(AxisInput.GetSafeNormal() | FromDirectionVector));
	const float CurAngleDelta = FMath::Abs(StickAngle - PrevAngle);

	// Ensure rotation is going in the proper direction (dont increment otherwise, but still want to cache values)
	const bool bProperTurn = ProperRotation(AxisInput);
	
	PrevAngle = StickAngle;
	PrevVector = AxisInput;
	

	// Equal so D-Pad is also registered
	if (bProperTurn && CurAngleDelta <= 90) CurAngle += CurAngleDelta;
}


