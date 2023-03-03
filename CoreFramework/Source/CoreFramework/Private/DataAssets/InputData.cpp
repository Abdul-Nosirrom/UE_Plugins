// Copyright 2023 Abdulrahmen Almodaimegh. All Rights Reserved.


#include "InputData.h"

#include "Subsystems/InputBufferSubsystem.h"

TArray<FName> UInputBufferMap::GetActionIDs()
{
	// Check cached value, if not there then we regenerate the IDs
	// BUG: Mappings aren't what we're looking for, we just wanna get the actions directly (Mappings don't represent the number of actions)
	// NOTE: Instead lets store the array of actions which we can get directly, better and more consistent handling...
	if (!ActionIDs.IsEmpty())// && ActionIDs.Num() == InputActionMap->GetMappings().Num())
	{
		return ActionIDs;
	}

	// Regenerate
	ActionIDs.Empty();
	
	for (auto AMap : InputActionMap->GetMappings())
	{
		if (ActionIDs.Contains(FName(AMap.Action->ActionDescription.ToString()))) continue;
		ActionIDs.Add(FName(AMap.Action->ActionDescription.ToString()));
	}

	return ActionIDs;
}

TArray<FName> UInputBufferMap::GetDirectionalIDs()
{
	// Check cached value, if not there then we regenerate the IDs
	if (!DirectionalIDs.IsEmpty() && DirectionalIDs.Num() == DirectionalActionMap->GetMappings().Num())
	{
		return DirectionalIDs;
	}

	// Regenerate
	DirectionalIDs.Empty();
	
	for (auto DMap : DirectionalActionMap->GetMappings())
	{
		DirectionalIDs.Add(DMap->GetID());
	}

	return DirectionalIDs;
}


bool UMotionAction::CheckMotionDirection(const FVector2D& AxisInput)
{
	// Directional Input Describes An Angle Change
	if (bAngleChange)
	{
		// This call will update CurAngle
		UpdateCurrentAngle(AxisInput);
		return CurAngle >= AngleDelta;
	}
	else if (!MotionCommandSequence.IsEmpty()) // Ensure command sequence isn't empty, otherwise we don't have a valid seq to check for 
	{
		// We've already evaluated all sequence parts successfully here
		if (CheckStep >= MotionCommandSequence.Num())
		{
			return true;
		}

		// Check if current command step corresponds to the current input seq and increment the step for next time
		if (MotionCommandSequence[CheckStep] == GetAxisDirection(AxisInput))
		{
			// We increment the step check count, but don't return true yet as we still haven't satisfied the whole sequence
			CheckStep++;
		}
	}

	return false;
}

EMotionCommandDirection UMotionAction::GetAxisDirection(const FVector2D& AxisInput)
{
	if (AxisInput.IsZero()) return EMotionCommandDirection::NEUTRAL;
	
	FVector2D ProperAxisInput(AxisInput);
	FVector2D EvaluationDirection(0, 1);
	FVector2D RightDirection(1, 0);

	if (bRelativeToPlayer)
	{
		// Need to get 2 thing: CameraRotator & PlayerActorRotator
		
		// Convert AxisInput to Camera Basis

		// Get PlayerForward and PlayerRight
		//auto InputBufferSubsystem = ULocalPlayer::GetSubsystem<UInputBufferSubsystem>();
	}

	const float StickAngle = FMath::RadiansToDegrees(FMath::Acos(ProperAxisInput.GetSafeNormal() | EvaluationDirection.GetSafeNormal()));
	const bool bRightDirection = (ProperAxisInput | RightDirection) > 0;
	
	// Now evaluate the stick angle to return a command sequence step
	//if (StickAngle == 0)							return EMotionCommandDirection::NEUTRAL;
	if (StickAngle < 45)						return EMotionCommandDirection::FORWARD;
	else if (StickAngle < 135 && bRightDirection)	return EMotionCommandDirection::RIGHT;
	else if (StickAngle < 135 && !bRightDirection)	return EMotionCommandDirection::LEFT;
	else											return EMotionCommandDirection::BACK;
}

void UMotionAction::UpdateCurrentAngle(const FVector2D& AxisInput)
{
	// Angle changes are never done relative to the player, they're always done relative to the stick input
	FVector2D FromDirectionVector(0, 1);

	// NOTE: This is wrong, we just *initialize* angle change stuff when the stick input is aligned with FromDirection
	// Determine from direction in which we evaluate the angle change against:
	switch (FromDirection)
	{
		case EFacingDirection::FROM_FORWARD:
			break;
		case EFacingDirection::FROM_BACK:
			FromDirectionVector = FVector2D(0,-1);
			break;
		case EFacingDirection::FROM_RIGHT:
			FromDirectionVector = FVector2D(1,0);
			break;
		case EFacingDirection::FROM_LEFT:
			FromDirectionVector = FVector2D(-1, 0);
			break;
		default:
			break;
	}

	// Determine whether its clockwise or anticlockwise
	switch (TurnDirection)
	{
		case ETurnDirection::CW:
			break;
		case ETurnDirection::CCW:
			break;
		default:
			break;
	}

	// Now we're done with the set up, so compute the current angle and evaluate it against our previously computed angle
	const float StickAngle = FMath::RadiansToDegrees(FMath::Acos(AxisInput.GetSafeNormal() | FromDirectionVector));
}


