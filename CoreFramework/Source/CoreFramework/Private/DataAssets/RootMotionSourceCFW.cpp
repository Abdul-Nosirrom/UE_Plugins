﻿// Fill out your copyright notice in the Description page of Project Settings.

#include "RootMotionSourceCFW.h"
#include "Actors/RadicalCharacter.h"
#include "DrawDebugHelpers.h"
#include "Components/RadicalMovementComponent.h"
#include "Curves/CurveVector.h"
#include "Curves/CurveFloat.h"
#include "RootMotionTasks/RootMotionTask_Base.h"

#pragma region Utility

#if ROOT_MOTION_DEBUG
TAutoConsoleVariable<int32> RootMotionSourceCFWDebug::CVarDebugRootMotionSources(
	TEXT("cfw.RootMotion.Debug"),
	0,
	TEXT("Whether to draw root motion source debug information.\n")
	TEXT("0: Disable, 1: Enable"),
	ECVF_Cheat);

static TAutoConsoleVariable<float> CVarDebugRootMotionSourcesLifetime(
	TEXT("cfw.RootMotion.DebugSourceLifeTime"),
	6.f,
	TEXT("How long a visualized root motion source persists.\n")
	TEXT("Time in seconds each visualized root motion source persists."),
	ECVF_Cheat);

void RootMotionSourceCFWDebug::PrintOnScreen(const ARadicalCharacter& InCharacter, const FString& InString)
{
	// Skip bots, debug player networking.
	if (InCharacter.IsPlayerControlled())
	{
		const FString AdjustedDebugString = FString::Printf(TEXT("[%d] [%s] %s"), (uint64)GFrameCounter, *InCharacter.GetName(), *InString);
		
		{
			const FColor DebugColor = (InCharacter.IsLocallyControlled()) ? FColor::Green : FColor::Purple;
			GEngine->AddOnScreenDebugMessage(INDEX_NONE, 0.f, DebugColor, AdjustedDebugString, false, FVector2D::UnitVector * 1.5f);

			UE_LOG(LogRootMotion, Verbose, TEXT("%s"), *AdjustedDebugString);
		}
	}
}

void RootMotionSourceCFWDebug::PrintOnScreenServerMsg(const FString& InString)
{
	const FColor DebugColor = FColor::Red;
	GEngine->AddOnScreenDebugMessage(INDEX_NONE, 0.f, DebugColor, InString, false, FVector2D::UnitVector * 1.5f);

	UE_LOG(LogRootMotion, Verbose, TEXT("%s"), *InString);
}

#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST)


const float RootMotionSource_InvalidStartTime = -UE_BIG_NUMBER;


static float EvaluateFloatCurveAtFraction(const UCurveFloat& Curve, const float Fraction)
{
	float MinCurveTime(0.f);
	float MaxCurveTime(1.f);

	Curve.GetTimeRange(MinCurveTime, MaxCurveTime);
	return Curve.GetFloatValue(FMath::GetRangeValue(FVector2f(MinCurveTime, MaxCurveTime), Fraction));
}

static FVector EvaluateVectorCurveAtFraction(const UCurveVector& Curve, const float Fraction)
{
	float MinCurveTime(0.f);
	float MaxCurveTime(1.f);

	Curve.GetTimeRange(MinCurveTime, MaxCurveTime);
	return Curve.GetVectorValue(FMath::GetRangeValue(FVector2f(MinCurveTime, MaxCurveTime), Fraction));
}

#pragma endregion Utility

void FRootMotionSourceCFW::PrepareCustomRootMotion(float SimulationTime, float MovementTickTime, const ARadicalCharacter& Character, const URadicalMovementComponent& MoveComponent)
{
	RootMotionParams.Clear();
}

#pragma region Source Group

void FRootMotionSourceGroupCFW::CleanUpInvalidRootMotion(float DeltaTime, const ARadicalCharacter& Character, URadicalMovementComponent& MoveComponent)
{
	// Remove active sources marked for removal or that are invalid
	RootMotionSources.RemoveAll([this, DeltaTime, &Character, &MoveComponent](const TSharedPtr<FRootMotionSource>& RootSource)
	{
		if (RootSource.IsValid())
		{
			if (!RootSource->Status.HasFlag(ERootMotionSourceStatusFlags::MarkedForRemoval) &&
				!RootSource->Status.HasFlag(ERootMotionSourceStatusFlags::Finished))
			{
				return false;
			}

			// When additive root motion sources are removed we add their effects back to Velocity
			// so that any maintained momentum/velocity that they were contributing affects character
			// velocity and it's not a sudden stop
			if (RootSource->AccumulateMode == ERootMotionAccumulateMode::Additive)
			{
				if (bIsAdditiveVelocityApplied)
				{
					const FVector PreviousLastPreAdditiveVelocity = LastPreAdditiveVelocity;
					AccumulateRootMotionVelocityFromSource(*RootSource, DeltaTime, Character, MoveComponent, LastPreAdditiveVelocity);

#if ROOT_MOTION_DEBUG
					if (RootMotionSourceCFWDebug::CVarDebugRootMotionSources.GetValueOnAnyThread() == 1)
					{
						FString AdjustedDebugString = FString::Printf(TEXT("PrepareRootMotion RemovingAdditiveSource LastPreAdditiveVelocity(%s) Old(%s)"),
							*LastPreAdditiveVelocity.ToCompactString(), *PreviousLastPreAdditiveVelocity.ToCompactString());
						RootMotionSourceCFWDebug::PrintOnScreen(Character, AdjustedDebugString);
					}
#endif
				}
			}

			// Process FinishVelocity options when RootMotionSource is removed.
			if (RootSource->FinishVelocityParams.Mode == ERootMotionFinishVelocityMode::ClampVelocity)
			{
				// For Z, only clamp positive values to prevent shooting off, we don't want to slow down a fall.
				MoveComponent.Velocity = MoveComponent.Velocity.GetClampedToMaxSize2D(RootSource->FinishVelocityParams.ClampVelocity);
				MoveComponent.Velocity.Z = FMath::Min<FVector::FReal>(MoveComponent.Velocity.Z, RootSource->FinishVelocityParams.ClampVelocity);

				// if we have additive velocity applied, LastPreAdditiveVelocity will stomp velocity, so make sure it gets clamped too.
				if (bIsAdditiveVelocityApplied)
				{
					// For Z, only clamp positive values to prevent shooting off, we don't want to slow down a fall.
					LastPreAdditiveVelocity = LastPreAdditiveVelocity.GetClampedToMaxSize2D(RootSource->FinishVelocityParams.ClampVelocity);
					LastPreAdditiveVelocity.Z = FMath::Min<FVector::FReal>(LastPreAdditiveVelocity.Z, RootSource->FinishVelocityParams.ClampVelocity);
				}
			}
			else if (RootSource->FinishVelocityParams.Mode == ERootMotionFinishVelocityMode::SetVelocity)
			{
				MoveComponent.Velocity = RootSource->FinishVelocityParams.SetVelocity;
				// if we have additive velocity applied, LastPreAdditiveVelocity will stomp velocity, so make sure this gets set too.
				if (bIsAdditiveVelocityApplied)
				{
					LastPreAdditiveVelocity = RootSource->FinishVelocityParams.SetVelocity;
				}
			}

			UE_LOG(LogRootMotion, VeryVerbose, TEXT("RootMotionSource being removed: %s"), *RootSource->ToSimpleString());

#if ROOT_MOTION_DEBUG
			if (RootMotionSourceCFWDebug::CVarDebugRootMotionSources.GetValueOnAnyThread() == 1)
			{
				FString AdjustedDebugString = FString::Printf(TEXT("PrepareRootMotion Removing RootMotionSource(%s)"),
					*RootSource->ToSimpleString());
				RootMotionSourceCFWDebug::PrintOnScreen(Character, AdjustedDebugString);
			}
#endif
		}
		return true;
	});

	// Remove pending sources that could have been marked for removal before they were made active
	PendingAddRootMotionSources.RemoveAll([&Character](const TSharedPtr<FRootMotionSource>& RootSource)
	{
		if (RootSource.IsValid())
		{
			if (!RootSource->Status.HasFlag(ERootMotionSourceStatusFlags::MarkedForRemoval) &&
				!RootSource->Status.HasFlag(ERootMotionSourceStatusFlags::Finished))
			{
				return false;
			}

			UE_LOG(LogRootMotion, VeryVerbose, TEXT("Pending RootMotionSource being removed: %s"), *RootSource->ToSimpleString());

#if ROOT_MOTION_DEBUG
			if (RootMotionSourceCFWDebug::CVarDebugRootMotionSources.GetValueOnAnyThread() == 1)
			{
				FString AdjustedDebugString = FString::Printf(TEXT("PrepareRootMotion Removing PendingAddRootMotionSource(%s)"),
					*RootSource->ToSimpleString());
				RootMotionSourceCFWDebug::PrintOnScreen(Character, AdjustedDebugString);
			}
#endif
		}
		return true;
	});
}

void FRootMotionSourceGroupCFW::PrepareRootMotion(float DeltaTime, const ARadicalCharacter& Character, const URadicalMovementComponent& MoveComponent, bool bForcePrepareAll)
{
	// Add pending sources
	{
		RootMotionSources.Append(PendingAddRootMotionSources);
		PendingAddRootMotionSources.Empty();
	}

	// Sort by priority
	if (RootMotionSources.Num() > 1)
	{
		RootMotionSources.StableSort([](const TSharedPtr<FRootMotionSource>& SourceL, const TSharedPtr<FRootMotionSource>& SourceR)
			{
				if (SourceL.IsValid() && SourceR.IsValid())
				{
					return SourceL->Priority > SourceR->Priority;
				}
				checkf(false, TEXT("RootMotionSources being sorted are invalid pointers"));
				return true;
			});
	}

	// Prepare active sources
	{
		bHasOverrideSources = false;
		bHasOverrideSourcesWithIgnoreZAccumulate = false;
		bHasAdditiveSources = false;
		LastAccumulatedSettings.Clear();

		// Go through all sources, prepare them so that they each save off how much they're going to contribute this frame
		for (TSharedPtr<FRootMotionSource>& RootMotionSource : RootMotionSources)
		{
			if (RootMotionSource.IsValid())
			{
				TSharedPtr<FRootMotionSourceCFW> OPRootMotionSource = StaticCastSharedPtr<FRootMotionSourceCFW>(RootMotionSource);

				if (!OPRootMotionSource) return;
				
				if (!OPRootMotionSource->Status.HasFlag(ERootMotionSourceStatusFlags::Prepared) || bForcePrepareAll)
				{
					float SimulationTime = DeltaTime;

					// Do the Preparation (calculates root motion transforms to be applied)

					OPRootMotionSource->bSimulatedNeedsSmoothing = false;
					OPRootMotionSource->PrepareCustomRootMotion(SimulationTime, DeltaTime, Character, MoveComponent);
					LastAccumulatedSettings += OPRootMotionSource->Settings;
					OPRootMotionSource->Status.SetFlag(ERootMotionSourceStatusFlags::Prepared);	
					

#if ROOT_MOTION_DEBUG
					if (RootMotionSourceCFWDebug::CVarDebugRootMotionSources.GetValueOnAnyThread() == 1)
					{
						FString AdjustedDebugString = FString::Printf(TEXT("PrepareRootMotion Prepared RootMotionSource(%s)"),
							*OPRootMotionSource->ToSimpleString());
						RootMotionSourceCFWDebug::PrintOnScreen(Character, AdjustedDebugString);
					}
#endif

					OPRootMotionSource->bNeedsSimulatedCatchup = false;
				}
				else // if (!RootMotionSource->Status.HasFlag(ERootMotionSourceStatusFlags::Prepared) || bForcePrepareAll)
				{
#if ROOT_MOTION_DEBUG
					if (RootMotionSourceCFWDebug::CVarDebugRootMotionSources.GetValueOnAnyThread() == 1)
					{
						FString AdjustedDebugString = FString::Printf(TEXT("PrepareRootMotion AlreadyPrepared RootMotionSource(%s)"),
							*OPRootMotionSource->ToSimpleString());
						RootMotionSourceCFWDebug::PrintOnScreen(Character, AdjustedDebugString);
					}
#endif
				}

				if (OPRootMotionSource->AccumulateMode == ERootMotionAccumulateMode::Additive)
				{
					bHasAdditiveSources = true;
				}
				else if (OPRootMotionSource->AccumulateMode == ERootMotionAccumulateMode::Override)
				{
					bHasOverrideSources = true;

					if (OPRootMotionSource->Settings.HasFlag(ERootMotionSourceSettingsFlags::IgnoreZAccumulate))
					{
						bHasOverrideSourcesWithIgnoreZAccumulate = true;
					}
				}
			}
		}
	}
}

void FRootMotionSourceGroupCFW::AccumulateOverrideRootMotionVelocity
	(
		float DeltaTime, 
		const ARadicalCharacter& Character, 
		const URadicalMovementComponent& MoveComponent, 
		FVector& InOutVelocity
	) const
{
	AccumulateRootMotionVelocity(ERootMotionAccumulateMode::Override, DeltaTime, Character, MoveComponent, InOutVelocity);
}

void FRootMotionSourceGroupCFW::AccumulateAdditiveRootMotionVelocity
	(
		float DeltaTime, 
		const ARadicalCharacter& Character, 
		const URadicalMovementComponent& MoveComponent, 
		FVector& InOutVelocity
	) const
{
	AccumulateRootMotionVelocity(ERootMotionAccumulateMode::Additive, DeltaTime, Character, MoveComponent, InOutVelocity);
}

void FRootMotionSourceGroupCFW::AccumulateRootMotionVelocity
	(
		ERootMotionAccumulateMode RootMotionType,
		float DeltaTime, 
		const ARadicalCharacter& Character, 
		const URadicalMovementComponent& MoveComponent, 
		FVector& InOutVelocity
	) const
{
	check(RootMotionType == ERootMotionAccumulateMode::Additive || RootMotionType == ERootMotionAccumulateMode::Override);

	// Go through all sources, accumulate their contribution to root motion
	for (const auto& RootMotionSource : RootMotionSources)
	{
		if (RootMotionSource.IsValid() && RootMotionSource->AccumulateMode == RootMotionType)
		{
			AccumulateRootMotionVelocityFromSource(*RootMotionSource, DeltaTime, Character, MoveComponent, InOutVelocity);

			// For Override root motion, we apply the highest priority override and ignore the rest
			if (RootMotionSource->AccumulateMode == ERootMotionAccumulateMode::Override)
			{
				break;
			}
		}
	}
}

void FRootMotionSourceGroupCFW::AccumulateRootMotionVelocityFromSource
	(
		const FRootMotionSource& RootMotionSource,
		float DeltaTime, 
		const ARadicalCharacter& Character, 
		const URadicalMovementComponent& MoveComponent, 
		FVector& InOutVelocity
	) const
{
	FRootMotionMovementParams RootMotionParams = RootMotionSource.RootMotionParams;

	// Transform RootMotion if needed (world vs local space)
	if (RootMotionSource.bInLocalSpace && MoveComponent.UpdatedComponent)
	{
		RootMotionParams.Set( RootMotionParams.GetRootMotionTransform() * MoveComponent.UpdatedComponent->GetComponentToWorld().GetRotation() );
	}

	const FVector RootMotionVelocity = RootMotionParams.GetRootMotionTransform().GetTranslation();

	const FVector InputVelocity = InOutVelocity;
	if (RootMotionSource.AccumulateMode == ERootMotionAccumulateMode::Override)
	{
		InOutVelocity = RootMotionVelocity;
	}
	else if (RootMotionSource.AccumulateMode == ERootMotionAccumulateMode::Additive)
	{
		InOutVelocity += RootMotionVelocity;
	}
	if (RootMotionSource.Settings.HasFlag(ERootMotionSourceSettingsFlags::IgnoreZAccumulate))
	{
		InOutVelocity.Z = InputVelocity.Z;
	}
}

bool FRootMotionSourceGroupCFW::GetOverrideRootMotionRotation
	(
		float DeltaTime, 
		const ARadicalCharacter& Character, 
		const URadicalMovementComponent& MoveComponent, 
		FQuat& OutRotation
	) const
{
	for (const auto& RootMotionSource : RootMotionSources)
	{
		if (RootMotionSource.IsValid() && RootMotionSource->AccumulateMode == ERootMotionAccumulateMode::Override)
		{
			OutRotation = RootMotionSource->RootMotionParams.GetRootMotionTransform().GetRotation();
			return !OutRotation.IsIdentity();
		}
	}
	return false;
}

void FRootMotionSourceGroupCFW::ClearAndDestroy()
{
	/* Delete tasks */
	for (auto Source : RootMotionSources)
	{
		if (auto Cast = StaticCastSharedPtr<FRootMotionSourceCFW>(Source))
		{
			if (Cast->AssociatedTask) Cast->AssociatedTask->OnDestroy();
		}
	}

	Clear();
}


#pragma endregion Source Group

#pragma region Constant Force

FRootMotionSourceCFW_ConstantForce::FRootMotionSourceCFW_ConstantForce()
	: Force(ForceInitToZero)
	, StrengthOverTime(nullptr)
{
	// Disable Partial End Tick for Constant Forces.
	// Otherwise we end up with very inconsistent velocities on the last frame.
	// This ensures that the ending velocity is maintained and consistent.
	Settings.SetFlag(ERootMotionSourceSettingsFlags::DisablePartialEndTick);
}

FRootMotionSourceCFW* FRootMotionSourceCFW_ConstantForce::Clone() const
{
	FRootMotionSourceCFW_ConstantForce* CopyPtr = new FRootMotionSourceCFW_ConstantForce(*this);
	return CopyPtr;
}

bool FRootMotionSourceCFW_ConstantForce::Matches(const FRootMotionSource* Other) const
{
	if (!FRootMotionSource::Matches(Other))
	{
		return false;
	}

	// We can cast safely here since in FRootMotionSource::Matches() we ensured ScriptStruct equality
	const FRootMotionSourceCFW_ConstantForce* OtherCast = static_cast<const FRootMotionSourceCFW_ConstantForce*>(Other);

	return FVector::PointsAreNear(Force, OtherCast->Force, 0.1f) &&
		StrengthOverTime == OtherCast->StrengthOverTime;
}

bool FRootMotionSourceCFW_ConstantForce::MatchesAndHasSameState(const FRootMotionSource* Other) const
{
	// Check that it matches
	if (!FRootMotionSource::MatchesAndHasSameState(Other))
	{
		return false;
	}

	return true; // ConstantForce has no unique state
}

bool FRootMotionSourceCFW_ConstantForce::UpdateStateFrom(const FRootMotionSource* SourceToTakeStateFrom, bool bMarkForSimulatedCatchup)
{
	if (!FRootMotionSource::UpdateStateFrom(SourceToTakeStateFrom, bMarkForSimulatedCatchup))
	{
		return false;
	}

	return true; // ConstantForce has no unique state other than Time which is handled by FRootMotionSource
}

void FRootMotionSourceCFW_ConstantForce::PrepareCustomRootMotion
	(
		float SimulationTime, 
		float MovementTickTime,
		const ARadicalCharacter& Character, 
		const URadicalMovementComponent& MoveComponent
	)
{
	RootMotionParams.Clear();

	FTransform NewTransform(Force);

	// Scale strength of force over time
	if (StrengthOverTime)
	{
		const float TimeValue = Duration > 0.f ? FMath::Clamp(GetTime() / Duration, 0.f, 1.f) : GetTime();
		const float TimeFactor = StrengthOverTime->GetFloatValue(TimeValue);
		NewTransform.ScaleTranslation(TimeFactor);
	}

	// Scale force based on Simulation/MovementTime differences
	// Ex: Force is to go 200 cm per second forward.
	//     To catch up with server state we need to apply
	//     3 seconds of this root motion in 1 second of
	//     movement tick time -> we apply 600 cm for this frame
	const float Multiplier = (MovementTickTime > UE_SMALL_NUMBER) ? (SimulationTime / MovementTickTime) : 1.f;
	NewTransform.ScaleTranslation(Multiplier);

#if ROOT_MOTION_DEBUG
	if (RootMotionSourceCFWDebug::CVarDebugRootMotionSources.GetValueOnAnyThread() == 1)
	{
		FString AdjustedDebugString = FString::Printf(TEXT("FRootMotionSource_ConstantForce::PrepareRootMotion NewTransform(%s) Multiplier(%f)"),
			*NewTransform.GetTranslation().ToCompactString(), Multiplier);
		RootMotionSourceCFWDebug::PrintOnScreen(Character, AdjustedDebugString);
	}
#endif

	RootMotionParams.Set(NewTransform);

	SetTime(GetTime() + SimulationTime);
}


UScriptStruct* FRootMotionSourceCFW_ConstantForce::GetScriptStruct() const
{
	return FRootMotionSource_ConstantForce::StaticStruct();
}

FString FRootMotionSourceCFW_ConstantForce::ToSimpleString() const
{
	return FString::Printf(TEXT("[ID:%u]FRootMotionSource_ConstantForce %s"), LocalID, *InstanceName.GetPlainNameString());
}

void FRootMotionSourceCFW_ConstantForce::AddReferencedObjects(class FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(StrengthOverTime);

	FRootMotionSource::AddReferencedObjects(Collector);
}


#pragma endregion Constant Force


#pragma region Radial Force

FRootMotionSourceCFW_RadialForce::FRootMotionSourceCFW_RadialForce()
	: Location(ForceInitToZero)
	, LocationActor(nullptr)
	, Radius(1.f)
	, Strength(0.f)
	, bIsPush(true)
	, bNoZForce(false)
	, StrengthDistanceFalloff(nullptr)
	, StrengthOverTime(nullptr)
	, bUseFixedWorldDirection(false)
	, FixedWorldDirection(ForceInitToZero)
{
}

FRootMotionSourceCFW* FRootMotionSourceCFW_RadialForce::Clone() const
{
	FRootMotionSourceCFW_RadialForce* CopyPtr = new FRootMotionSourceCFW_RadialForce(*this);
	return CopyPtr;
}

bool FRootMotionSourceCFW_RadialForce::Matches(const FRootMotionSource* Other) const
{
	if (!FRootMotionSource::Matches(Other))
	{
		return false;
	}

	// We can cast safely here since in FRootMotionSource::Matches() we ensured ScriptStruct equality
	const FRootMotionSourceCFW_RadialForce* OtherCast = static_cast<const FRootMotionSourceCFW_RadialForce*>(Other);

	return bIsPush == OtherCast->bIsPush &&
		bNoZForce == OtherCast->bNoZForce &&
		bUseFixedWorldDirection == OtherCast->bUseFixedWorldDirection &&
		StrengthDistanceFalloff == OtherCast->StrengthDistanceFalloff &&
		StrengthOverTime == OtherCast->StrengthOverTime &&
		(LocationActor == OtherCast->LocationActor ||
		FVector::PointsAreNear(Location, OtherCast->Location, 1.0f)) &&
		FMath::IsNearlyEqual(Radius, OtherCast->Radius, UE_SMALL_NUMBER) &&
		FMath::IsNearlyEqual(Strength, OtherCast->Strength, UE_SMALL_NUMBER) &&
		FixedWorldDirection.Equals(OtherCast->FixedWorldDirection, 3.0f);
}

bool FRootMotionSourceCFW_RadialForce::MatchesAndHasSameState(const FRootMotionSource* Other) const
{
	// Check that it matches
	if (!FRootMotionSourceCFW::MatchesAndHasSameState(Other))
	{
		return false;
	}

	return true; // RadialForce has no unique state
}

bool FRootMotionSourceCFW_RadialForce::UpdateStateFrom(const FRootMotionSource* SourceToTakeStateFrom, bool bMarkForSimulatedCatchup)
{
	if (!FRootMotionSource::UpdateStateFrom(SourceToTakeStateFrom, bMarkForSimulatedCatchup))
	{
		return false;
	}

	return true; // RadialForce has no unique state other than Time which is handled by FRootMotionSource
}

void FRootMotionSourceCFW_RadialForce::PrepareCustomRootMotion
	(
		float SimulationTime, 
		float MovementTickTime,
		const ARadicalCharacter& Character, 
		const URadicalMovementComponent& MoveComponent
	)
{
	RootMotionParams.Clear();

	const FVector CharacterLocation = Character.GetActorLocation();
	FVector Force = FVector::ZeroVector;
	const FVector ForceLocation = LocationActor ? LocationActor->GetActorLocation() : Location;
	float Distance = FVector::Dist(ForceLocation, CharacterLocation);
	if (Distance < Radius)
	{
		// Calculate strength
		float CurrentStrength = Strength;
		{
			float AdditiveStrengthFactor = 1.f;
			if (StrengthDistanceFalloff)
			{
				const float DistanceFactor = StrengthDistanceFalloff->GetFloatValue(FMath::Clamp(Distance / Radius, 0.f, 1.f));
				AdditiveStrengthFactor -= (1.f - DistanceFactor);
			}

			if (StrengthOverTime)
			{
				const float TimeValue = Duration > 0.f ? FMath::Clamp(GetTime() / Duration, 0.f, 1.f) : GetTime();
				const float TimeFactor = StrengthOverTime->GetFloatValue(TimeValue);
				AdditiveStrengthFactor -= (1.f - TimeFactor);
			}

			CurrentStrength = Strength * FMath::Clamp(AdditiveStrengthFactor, 0.f, 1.f);
		}

		if (bUseFixedWorldDirection)
		{
			Force = FixedWorldDirection.Vector() * CurrentStrength;
		}
		else
		{
			Force = (ForceLocation - CharacterLocation).GetSafeNormal() * CurrentStrength;
			
			if (bIsPush)
			{
				Force *= -1.f;
			}
		}
	}

	if (bNoZForce)
	{
		Force.Z = 0.f;
	}

	FTransform NewTransform(Force);

	// Scale force based on Simulation/MovementTime differences
	// Ex: Force is to go 200 cm per second forward.
	//     To catch up with server state we need to apply
	//     3 seconds of this root motion in 1 second of
	//     movement tick time -> we apply 600 cm for this frame
	if (SimulationTime != MovementTickTime && MovementTickTime > UE_SMALL_NUMBER)
	{
		const float Multiplier = SimulationTime / MovementTickTime;
		NewTransform.ScaleTranslation(Multiplier);
	}

	RootMotionParams.Set(NewTransform);

	SetTime(GetTime() + SimulationTime);
}


UScriptStruct* FRootMotionSourceCFW_RadialForce::GetScriptStruct() const
{
	return FRootMotionSource_RadialForce::StaticStruct();
}

FString FRootMotionSourceCFW_RadialForce::ToSimpleString() const
{
	return FString::Printf(TEXT("[ID:%u]FRootMotionSource_RadialForce %s"), LocalID, *InstanceName.GetPlainNameString());
}

void FRootMotionSourceCFW_RadialForce::AddReferencedObjects(class FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(LocationActor);
	Collector.AddReferencedObject(StrengthDistanceFalloff);
	Collector.AddReferencedObject(StrengthOverTime);

	FRootMotionSource::AddReferencedObjects(Collector);
}


#pragma endregion Radial Force

#pragma region Move To Force

FRootMotionSourceCFW_MoveToForce::FRootMotionSourceCFW_MoveToForce()
	: StartLocation(ForceInitToZero)
	, TargetLocation(ForceInitToZero)
	, bRestrictSpeedToExpected(false)
	, PathOffsetCurve(nullptr)
{
}

FRootMotionSource* FRootMotionSourceCFW_MoveToForce::Clone() const
{
	FRootMotionSourceCFW_MoveToForce* CopyPtr = new FRootMotionSourceCFW_MoveToForce(*this);
	return CopyPtr;
}

bool FRootMotionSourceCFW_MoveToForce::Matches(const FRootMotionSource* Other) const
{
	if (!FRootMotionSource::Matches(Other))
	{
		return false;
	}

	// We can cast safely here since in FRootMotionSource::Matches() we ensured ScriptStruct equality
	const FRootMotionSourceCFW_MoveToForce* OtherCast = static_cast<const FRootMotionSourceCFW_MoveToForce*>(Other);

	return bRestrictSpeedToExpected == OtherCast->bRestrictSpeedToExpected &&
		PathOffsetCurve == OtherCast->PathOffsetCurve &&
		FVector::PointsAreNear(TargetLocation, OtherCast->TargetLocation, 0.1f);
}

bool FRootMotionSourceCFW_MoveToForce::MatchesAndHasSameState(const FRootMotionSource* Other) const
{
	// Check that it matches
	if (!FRootMotionSource::MatchesAndHasSameState(Other))
	{
		return false;
	}

	return true; // MoveToForce has no unique state
}

bool FRootMotionSourceCFW_MoveToForce::UpdateStateFrom(const FRootMotionSource* SourceToTakeStateFrom, bool bMarkForSimulatedCatchup)
{
	if (!FRootMotionSource::UpdateStateFrom(SourceToTakeStateFrom, bMarkForSimulatedCatchup))
	{
		return false;
	}

	return true; // MoveToForce has no unique state other than Time which is handled by FRootMotionSource
}

void FRootMotionSourceCFW_MoveToForce::SetTime(float NewTime)
{
	FRootMotionSource::SetTime(NewTime);

	// TODO-RootMotionSource: Check if reached destination?
}

FVector FRootMotionSourceCFW_MoveToForce::GetPathOffsetInWorldSpace(const float MoveFraction, const FRotator& PawnRotation) const
{
	if (PathOffsetCurve)
	{
		// Calculate path offset
		const FVector PathOffsetInFacingSpace = EvaluateVectorCurveAtFraction(*PathOffsetCurve, MoveFraction);
		FRotator FacingRotation((TargetLocation-StartLocation).Rotation());
		FacingRotation.Pitch = 0.f; // By default we don't include pitch in the offset, but an option could be added if necessary
		FVector TargetOffset = FacingRotation.RotateVector(PathOffsetInFacingSpace);

		if (bApplyCurveInLocalSpace)
		{
			TargetOffset = TargetOffset.X * FRotationMatrix(PawnRotation).GetUnitAxis(EAxis::X) + TargetOffset.Y * FRotationMatrix(PawnRotation).GetUnitAxis(EAxis::Y) + TargetOffset.Z * FRotationMatrix(PawnRotation).GetUnitAxis(EAxis::Z);
		}

		return TargetOffset;
	}

	return FVector::ZeroVector;
}

void FRootMotionSourceCFW_MoveToForce::PrepareCustomRootMotion
	(
		float SimulationTime, 
		float MovementTickTime,
		const ARadicalCharacter& Character, 
		const URadicalMovementComponent& MoveComponent
	)
{
	RootMotionParams.Clear();

	if (Duration > UE_SMALL_NUMBER && MovementTickTime > UE_SMALL_NUMBER)
	{
		const float MoveFraction = (GetTime() + SimulationTime) / Duration;

		FVector CurrentTargetLocation = FMath::Lerp<FVector, float>(StartLocation, TargetLocation, MoveFraction);
		
		CurrentTargetLocation += GetPathOffsetInWorldSpace(MoveFraction, Character.GetActorRotation());

		const FVector CurrentLocation = Character.GetActorLocation();

		FVector Force = (CurrentTargetLocation - CurrentLocation) / MovementTickTime;

		if (bRestrictSpeedToExpected && !Force.IsNearlyZero(UE_KINDA_SMALL_NUMBER))
		{
			// Calculate expected current location (if we didn't have collision and moved exactly where our velocity should have taken us)
			const float PreviousMoveFraction = GetTime() / Duration;
			FVector CurrentExpectedLocation = FMath::Lerp<FVector, float>(StartLocation, TargetLocation, PreviousMoveFraction);
			
			CurrentExpectedLocation += GetPathOffsetInWorldSpace(PreviousMoveFraction, Character.GetActorRotation());

			// Restrict speed to the expected speed, allowing some small amount of error
			const FVector ExpectedForce = (CurrentTargetLocation - CurrentExpectedLocation) / MovementTickTime;
			const float ExpectedSpeed = ExpectedForce.Size();
			const float CurrentSpeedSqr = Force.SizeSquared();

			const float ErrorAllowance = 0.5f; // in cm/s
			if (CurrentSpeedSqr > FMath::Square(ExpectedSpeed + ErrorAllowance))
			{
				Force.Normalize();
				Force *= ExpectedSpeed;
			}
		}

		// Debug
#if ROOT_MOTION_DEBUG
		if (RootMotionSourceCFWDebug::CVarDebugRootMotionSources.GetValueOnGameThread() != 0)
		{
			const FVector LocDiff = MoveComponent.UpdatedComponent->GetComponentLocation() - CurrentLocation;
			const float DebugLifetime = CVarDebugRootMotionSourcesLifetime.GetValueOnGameThread();

			// Current
			DrawDebugCapsule(Character.GetWorld(), MoveComponent.UpdatedComponent->GetComponentLocation(), Character.GetSimpleCollisionHalfHeight(), Character.GetSimpleCollisionRadius(), FQuat::Identity, FColor::Red, true, DebugLifetime);

			// Current Target
			DrawDebugCapsule(Character.GetWorld(), CurrentTargetLocation + LocDiff, Character.GetSimpleCollisionHalfHeight(), Character.GetSimpleCollisionRadius(), FQuat::Identity, FColor::Green, true, DebugLifetime);

			// Target
			DrawDebugCapsule(Character.GetWorld(), TargetLocation + LocDiff, Character.GetSimpleCollisionHalfHeight(), Character.GetSimpleCollisionRadius(), FQuat::Identity, FColor::Blue, true, DebugLifetime);

			// Force
			DrawDebugLine(Character.GetWorld(), CurrentLocation, CurrentLocation+Force, FColor::Blue, true, DebugLifetime);
		}
#endif

		FTransform NewTransform(Force);
		RootMotionParams.Set(NewTransform);
	}
	else
	{
		checkf(Duration > UE_SMALL_NUMBER, TEXT("FRootMotionSource_MoveToForce prepared with invalid duration."));
	}

	SetTime(GetTime() + SimulationTime);
}


UScriptStruct* FRootMotionSourceCFW_MoveToForce::GetScriptStruct() const
{
	return FRootMotionSourceCFW_MoveToForce::StaticStruct();
}

FString FRootMotionSourceCFW_MoveToForce::ToSimpleString() const
{
	return FString::Printf(TEXT("[ID:%u]FRootMotionSource_MoveToForce %s"), LocalID, *InstanceName.GetPlainNameString());
}

void FRootMotionSourceCFW_MoveToForce::AddReferencedObjects(class FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(PathOffsetCurve);

	FRootMotionSource::AddReferencedObjects(Collector);
}


#pragma endregion Move To Force

#pragma region Move To Dynamic Force

FRootMotionSourceCFW_MoveToDynamicForce::FRootMotionSourceCFW_MoveToDynamicForce()
	: StartLocation(ForceInitToZero)
	, InitialTargetLocation(ForceInitToZero)
	, TargetLocation(ForceInitToZero)
	, bRestrictSpeedToExpected(false)
	, PathOffsetCurve(nullptr)
	, TimeMappingCurve(nullptr)
{
}

void FRootMotionSourceCFW_MoveToDynamicForce::SetTargetLocation(FVector NewTargetLocation)
{
	TargetLocation = NewTargetLocation;
}

FRootMotionSource* FRootMotionSourceCFW_MoveToDynamicForce::Clone() const
{
	FRootMotionSourceCFW_MoveToDynamicForce* CopyPtr = new FRootMotionSourceCFW_MoveToDynamicForce(*this);
	return CopyPtr;
}

bool FRootMotionSourceCFW_MoveToDynamicForce::Matches(const FRootMotionSource* Other) const
{
	if (!FRootMotionSource::Matches(Other))
	{
		return false;
	}

	// We can cast safely here since in FRootMotionSource::Matches() we ensured ScriptStruct equality
	const FRootMotionSourceCFW_MoveToDynamicForce* OtherCast = static_cast<const FRootMotionSourceCFW_MoveToDynamicForce*>(Other);

	return bRestrictSpeedToExpected == OtherCast->bRestrictSpeedToExpected &&
		PathOffsetCurve == OtherCast->PathOffsetCurve &&
		TimeMappingCurve == OtherCast->TimeMappingCurve;
}

bool FRootMotionSourceCFW_MoveToDynamicForce::MatchesAndHasSameState(const FRootMotionSource* Other) const
{
	// Check that it matches
	if (!FRootMotionSource::MatchesAndHasSameState(Other))
	{
		return false;
	}

	return true; // MoveToDynamicForce has no unique state
}

bool FRootMotionSourceCFW_MoveToDynamicForce::UpdateStateFrom(const FRootMotionSource* SourceToTakeStateFrom, bool bMarkForSimulatedCatchup)
{
	if (!FRootMotionSource::UpdateStateFrom(SourceToTakeStateFrom, bMarkForSimulatedCatchup))
	{
		return false;
	}

	return true; // MoveToDynamicForce has no unique state other than Time which is handled by FRootMotionSource
}

void FRootMotionSourceCFW_MoveToDynamicForce::SetTime(float NewTime)
{
	FRootMotionSource::SetTime(NewTime);

	// TODO-RootMotionSource: Check if reached destination?
}

FVector FRootMotionSourceCFW_MoveToDynamicForce::GetPathOffsetInWorldSpace(const float MoveFraction, const FRotator& PawnRotation) const
{
	if (PathOffsetCurve)
	{
		// Calculate path offset
		const FVector PathOffsetInFacingSpace = EvaluateVectorCurveAtFraction(*PathOffsetCurve, MoveFraction);
		FRotator FacingRotation((TargetLocation-StartLocation).Rotation());
		FacingRotation.Pitch = 0.f; // By default we don't include pitch in the offset, but an option could be added if necessary
		FVector TargetOffset = FacingRotation.RotateVector(PathOffsetInFacingSpace);

		if (bApplyCurveInLocalSpace)
		{
			TargetOffset = TargetOffset.X * FRotationMatrix(PawnRotation).GetUnitAxis(EAxis::X) + TargetOffset.Y * FRotationMatrix(PawnRotation).GetUnitAxis(EAxis::Y) + TargetOffset.Z * FRotationMatrix(PawnRotation).GetUnitAxis(EAxis::Z);
		}

		return TargetOffset;
	}

	return FVector::ZeroVector;
}

void FRootMotionSourceCFW_MoveToDynamicForce::PrepareCustomRootMotion
	(
		float SimulationTime, 
		float MovementTickTime,
		const ARadicalCharacter& Character, 
		const URadicalMovementComponent& MoveComponent
	)
{
	RootMotionParams.Clear();

	if (Duration > UE_SMALL_NUMBER && MovementTickTime > UE_SMALL_NUMBER)
	{
		float MoveFraction = (GetTime() + SimulationTime) / Duration;
		
		if (TimeMappingCurve)
		{
			MoveFraction = EvaluateFloatCurveAtFraction(*TimeMappingCurve, MoveFraction);
		}

		FVector CurrentTargetLocation = FMath::Lerp<FVector, float>(StartLocation, TargetLocation, MoveFraction);
		CurrentTargetLocation += GetPathOffsetInWorldSpace(MoveFraction, Character.GetActorRotation());

		const FVector CurrentLocation = Character.GetActorLocation();

		FVector Force = (CurrentTargetLocation - CurrentLocation) / MovementTickTime;

		if (bRestrictSpeedToExpected && !Force.IsNearlyZero(UE_KINDA_SMALL_NUMBER))
		{
			// Calculate expected current location (if we didn't have collision and moved exactly where our velocity should have taken us)
			float PreviousMoveFraction = GetTime() / Duration;
			if (TimeMappingCurve)
			{
				PreviousMoveFraction = EvaluateFloatCurveAtFraction(*TimeMappingCurve, PreviousMoveFraction);
			}

			FVector CurrentExpectedLocation = FMath::Lerp<FVector, float>(StartLocation, TargetLocation, PreviousMoveFraction);
			CurrentExpectedLocation += GetPathOffsetInWorldSpace(PreviousMoveFraction, Character.GetActorRotation());

			// Restrict speed to the expected speed, allowing some small amount of error
			const FVector ExpectedForce = (CurrentTargetLocation - CurrentExpectedLocation) / MovementTickTime;
			const float ExpectedSpeed = ExpectedForce.Size();
			const float CurrentSpeedSqr = Force.SizeSquared();

			const float ErrorAllowance = 0.5f; // in cm/s
			if (CurrentSpeedSqr > FMath::Square(ExpectedSpeed + ErrorAllowance))
			{
				Force.Normalize();
				Force *= ExpectedSpeed;
			}
		}

		// Debug
#if ROOT_MOTION_DEBUG
		if (RootMotionSourceCFWDebug::CVarDebugRootMotionSources.GetValueOnGameThread() != 0)
		{
			const FVector LocDiff = MoveComponent.UpdatedComponent->GetComponentLocation() - CurrentLocation;
			const float DebugLifetime = CVarDebugRootMotionSourcesLifetime.GetValueOnGameThread();

			// Current
			DrawDebugCapsule(Character.GetWorld(), MoveComponent.UpdatedComponent->GetComponentLocation(), Character.GetSimpleCollisionHalfHeight(), Character.GetSimpleCollisionRadius(), FQuat::Identity, FColor::Red, true, DebugLifetime);

			// Current Target
			DrawDebugCapsule(Character.GetWorld(), CurrentTargetLocation + LocDiff, Character.GetSimpleCollisionHalfHeight(), Character.GetSimpleCollisionRadius(), FQuat::Identity, FColor::Green, true, DebugLifetime);

			// Target
			DrawDebugCapsule(Character.GetWorld(), TargetLocation + LocDiff, Character.GetSimpleCollisionHalfHeight(), Character.GetSimpleCollisionRadius(), FQuat::Identity, FColor::Blue, true, DebugLifetime);

			// Force
			DrawDebugLine(Character.GetWorld(), CurrentLocation, CurrentLocation+Force, FColor::Blue, true, DebugLifetime);
		}
#endif

		FTransform NewTransform(Force);
		RootMotionParams.Set(NewTransform);
	}
	else
	{
		checkf(Duration > UE_SMALL_NUMBER, TEXT("FRootMotionSource_MoveToDynamicForce prepared with invalid duration."));
	}

	SetTime(GetTime() + SimulationTime);
}

UScriptStruct* FRootMotionSourceCFW_MoveToDynamicForce::GetScriptStruct() const
{
	return FRootMotionSource_MoveToDynamicForce::StaticStruct();
}

FString FRootMotionSourceCFW_MoveToDynamicForce::ToSimpleString() const
{
	return FString::Printf(TEXT("[ID:%u]FRootMotionSource_MoveToDynamicForce %s"), LocalID, *InstanceName.GetPlainNameString());
}

void FRootMotionSourceCFW_MoveToDynamicForce::AddReferencedObjects(class FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(PathOffsetCurve);
	Collector.AddReferencedObject(TimeMappingCurve);

	FRootMotionSource::AddReferencedObjects(Collector);
}


#pragma endregion Move To Dynamic Force

#pragma region Jump Force

FRootMotionSourceCFW_JumpForce::FRootMotionSourceCFW_JumpForce()
	: Rotation(ForceInitToZero)
	, Distance(-1.0f)
	, Height(-1.0f)
	, bDisableTimeout(false)
	, PathOffsetCurve(nullptr)
	, TimeMappingCurve(nullptr)
	, SavedHalfwayLocation(FVector::ZeroVector)
{
	// Don't allow partial end ticks. Jump forces are meant to provide velocity that
	// carries through to the end of the jump, and if we do partial ticks at the very end,
	// it means the provided velocity can be significantly reduced on the very last tick,
	// resulting in lost momentum. This is not desirable for jumps.
	Settings.SetFlag(ERootMotionSourceSettingsFlags::DisablePartialEndTick);
}

bool FRootMotionSourceCFW_JumpForce::IsTimeOutEnabled() const
{
	if (bDisableTimeout)
	{
		return false;
	}
	return FRootMotionSourceCFW_JumpForce::IsTimeOutEnabled();
}

FRootMotionSource* FRootMotionSourceCFW_JumpForce::Clone() const
{
	FRootMotionSourceCFW_JumpForce* CopyPtr = new FRootMotionSourceCFW_JumpForce(*this);
	return CopyPtr;
}

bool FRootMotionSourceCFW_JumpForce::Matches(const FRootMotionSource* Other) const
{
	if (!FRootMotionSource::Matches(Other))
	{
		return false;
	}

	// We can cast safely here since in FRootMotionSource::Matches() we ensured ScriptStruct equality
	const FRootMotionSourceCFW_JumpForce* OtherCast = static_cast<const FRootMotionSourceCFW_JumpForce*>(Other);

	return bDisableTimeout == OtherCast->bDisableTimeout &&
		PathOffsetCurve == OtherCast->PathOffsetCurve &&
		TimeMappingCurve == OtherCast->TimeMappingCurve &&
		FMath::IsNearlyEqual(Distance, OtherCast->Distance, UE_SMALL_NUMBER) &&
		FMath::IsNearlyEqual(Height, OtherCast->Height, UE_SMALL_NUMBER) &&
		Rotation.Equals(OtherCast->Rotation, 1.0f);
}

bool FRootMotionSourceCFW_JumpForce::MatchesAndHasSameState(const FRootMotionSource* Other) const
{
	// Check that it matches
	if (!FRootMotionSource::MatchesAndHasSameState(Other))
	{
		return false;
	}

	return true; // JumpForce has no unique state
}

bool FRootMotionSourceCFW_JumpForce::UpdateStateFrom(const FRootMotionSource* SourceToTakeStateFrom, bool bMarkForSimulatedCatchup)
{
	if (!FRootMotionSource::UpdateStateFrom(SourceToTakeStateFrom, bMarkForSimulatedCatchup))
	{
		return false;
	}

	return true; // JumpForce has no unique state other than Time which is handled by FRootMotionSource
}

FVector FRootMotionSourceCFW_JumpForce::GetPathOffset(const float MoveFraction) const
{
	FVector PathOffset(FVector::ZeroVector);
	if (PathOffsetCurve)
	{
		// Calculate path offset
		PathOffset = EvaluateVectorCurveAtFraction(*PathOffsetCurve, MoveFraction);
	}
	else
	{
		// Default to "jump parabola", a simple x^2 shifted to be upside-down and shifted
		// to get [0,1] X (MoveFraction/Distance) mapping to [0,1] Y (height)
		// Height = -(2x-1)^2 + 1
		const float Phi = 2.f*MoveFraction - 1;
		const float Z = -(Phi*Phi) + 1;
		PathOffset.Z = Z;
	}

	// Scale Z offset to height. If Height < 0, we use direct path offset values
	if (Height >= 0.f)
	{
		PathOffset.Z *= Height;
	}

	return PathOffset;
}

FVector FRootMotionSourceCFW_JumpForce::GetRelativeLocation(float MoveFraction) const
{
	// Given MoveFraction, what relative location should a character be at?
	FRotator FacingRotation(Rotation);
	FacingRotation.Pitch = 0.f; // By default we don't include pitch, but an option could be added if necessary

	FVector RelativeLocationFacingSpace = FVector(MoveFraction * Distance, 0.f, 0.f) + GetPathOffset(MoveFraction);

	return FacingRotation.RotateVector(RelativeLocationFacingSpace);
}

void FRootMotionSourceCFW_JumpForce::PrepareCustomRootMotion
	(
		float SimulationTime, 
		float MovementTickTime,
		const ARadicalCharacter& Character, 
		const URadicalMovementComponent& MoveComponent
	)
{
	RootMotionParams.Clear();

	if (Duration > UE_SMALL_NUMBER && MovementTickTime > UE_SMALL_NUMBER && SimulationTime > UE_SMALL_NUMBER)
	{
		float CurrentTimeFraction = GetTime() / Duration;
		float TargetTimeFraction = (GetTime() + SimulationTime) / Duration;

		// If we're beyond specified duration, we need to re-map times so that
		// we continue our desired ending velocity
		if (TargetTimeFraction > 1.f)
		{
			float TimeFractionPastAllowable = TargetTimeFraction - 1.0f;
			TargetTimeFraction -= TimeFractionPastAllowable;
			CurrentTimeFraction -= TimeFractionPastAllowable;
		}

		float CurrentMoveFraction = CurrentTimeFraction;
		float TargetMoveFraction = TargetTimeFraction;

		if (TimeMappingCurve)
		{
			CurrentMoveFraction = EvaluateFloatCurveAtFraction(*TimeMappingCurve, CurrentMoveFraction);
			TargetMoveFraction  = EvaluateFloatCurveAtFraction(*TimeMappingCurve, TargetMoveFraction);
		}

		const FVector CurrentRelativeLocation = GetRelativeLocation(CurrentMoveFraction);
		const FVector TargetRelativeLocation = GetRelativeLocation(TargetMoveFraction);

		const FVector Force = (TargetRelativeLocation - CurrentRelativeLocation) / MovementTickTime;

		// Debug
#if ROOT_MOTION_DEBUG
		if (RootMotionSourceCFWDebug::CVarDebugRootMotionSources.GetValueOnGameThread() != 0)
		{
			const FVector CurrentLocation = Character.GetActorLocation();
			const FVector CurrentTargetLocation = CurrentLocation + (TargetRelativeLocation - CurrentRelativeLocation);
			const FVector LocDiff = MoveComponent.UpdatedComponent->GetComponentLocation() - CurrentLocation;
			const float DebugLifetime = CVarDebugRootMotionSourcesLifetime.GetValueOnGameThread();

			// Current
			DrawDebugCapsule(Character.GetWorld(), MoveComponent.UpdatedComponent->GetComponentLocation(), Character.GetSimpleCollisionHalfHeight(), Character.GetSimpleCollisionRadius(), FQuat::Identity, FColor::Red, true, DebugLifetime);

			// Current Target
			DrawDebugCapsule(Character.GetWorld(), CurrentTargetLocation + LocDiff, Character.GetSimpleCollisionHalfHeight(), Character.GetSimpleCollisionRadius(), FQuat::Identity, FColor::Green, true, DebugLifetime);

			// Target
			DrawDebugCapsule(Character.GetWorld(), CurrentTargetLocation + LocDiff, Character.GetSimpleCollisionHalfHeight(), Character.GetSimpleCollisionRadius(), FQuat::Identity, FColor::Blue, true, DebugLifetime);

			// Force
			DrawDebugLine(Character.GetWorld(), CurrentLocation, CurrentLocation+Force, FColor::Blue, true, DebugLifetime);

			// Halfway point
			const FVector HalfwayLocation = CurrentLocation + (GetRelativeLocation(0.5f) - CurrentRelativeLocation);
			if (SavedHalfwayLocation.IsNearlyZero())
			{
				SavedHalfwayLocation = HalfwayLocation;
			}
			if (FVector::DistSquared(SavedHalfwayLocation, HalfwayLocation) > 50.f*50.f)
			{
				UE_LOG(LogRootMotion, Verbose, TEXT("RootMotion JumpForce drifted from saved halfway calculation!"));
				SavedHalfwayLocation = HalfwayLocation;
			}
			DrawDebugCapsule(Character.GetWorld(), HalfwayLocation + LocDiff, Character.GetSimpleCollisionHalfHeight(), Character.GetSimpleCollisionRadius(), FQuat::Identity, FColor::White, true, DebugLifetime);

			// Destination point
			const FVector DestinationLocation = CurrentLocation + (GetRelativeLocation(1.0f) - CurrentRelativeLocation);
			DrawDebugCapsule(Character.GetWorld(), DestinationLocation + LocDiff, Character.GetSimpleCollisionHalfHeight(), Character.GetSimpleCollisionRadius(), FQuat::Identity, FColor::White, true, DebugLifetime);

			UE_LOG(LogRootMotion, VeryVerbose, TEXT("RootMotionJumpForce preparing from %f to %f from (%s) to (%s) resulting force %s"), 
				GetTime(), GetTime() + SimulationTime, 
				*CurrentLocation.ToString(), *CurrentTargetLocation.ToString(), 
				*Force.ToString());

			{
				FString AdjustedDebugString = FString::Printf(TEXT("    FRootMotionSource_JumpForce::Prep Force(%s) SimTime(%.3f) MoveTime(%.3f) StartP(%.3f) EndP(%.3f)"),
					*Force.ToCompactString(), SimulationTime, MovementTickTime, CurrentMoveFraction, TargetMoveFraction);
				RootMotionSourceCFWDebug::PrintOnScreen(Character, AdjustedDebugString);
			}
		}
#endif

		const FTransform NewTransform(Force);
		RootMotionParams.Set(NewTransform);
	}
	else
	{
		checkf(Duration > UE_SMALL_NUMBER, TEXT("FRootMotionSource_JumpForce prepared with invalid duration."));
	}

	SetTime(GetTime() + SimulationTime);
}


UScriptStruct* FRootMotionSourceCFW_JumpForce::GetScriptStruct() const
{
	return FRootMotionSourceCFW_JumpForce::StaticStruct();
}

FString FRootMotionSourceCFW_JumpForce::ToSimpleString() const
{
	return FString::Printf(TEXT("[ID:%u]FRootMotionSource_JumpForce %s"), LocalID, *InstanceName.GetPlainNameString());
}

void FRootMotionSourceCFW_JumpForce::AddReferencedObjects(class FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(PathOffsetCurve);
	Collector.AddReferencedObject(TimeMappingCurve);

	FRootMotionSource::AddReferencedObjects(Collector);
}


#pragma endregion Jump Force