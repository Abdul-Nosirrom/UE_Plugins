#pragma once

#include "Debug/CFW_LOG.h"

#undef PAWN_REF
#undef ACTOR_REF
#undef RMC_LOG_CATEGORY

#if !NO_LOGGING

#define RMC_OBJECT_INFO_TEXT(Text) TEXT("%s %-20s: " Text)
#define RMC_OBJECT_INFO(Time, ActorName) *Time, *ActorName

// Movement Component Logging Category
#ifdef CFW_MOVEMENT_LOG
	#define PAWN_REF PawnOwner
	#define RMC_LOG_CATEGORY LogRMCMovement
#endif

#if defined PAWN_REF && defined RMC_LOG_CATEGORY

	// Logging macro wrapper
	#define RMC_LOG(Verbosity, Format, ...)\
	{\
		if (PAWN_REF) {\
			FString\
			rmc_Time = FDateTime::Now().GetTimeOfDay().ToString(),\
			rmc_ActorName = PAWN_REF->GetName();\
			LOG(RMC_LOG_CATEGORY, Verbosity, RMC_OBJECT_INFO_TEXT(Format),\
			RMC_OBJECT_INFO(rmc_Time, rmc_ActorName), ##__VA_ARGS__)\
		}\
		else {\
			LOG(RMC_LOG_CATEGORY, Verbosity, TEXT("| No debug info available |") Format, ##__VA_ARGS__)\
			}\
	}

	// Conditional Logging
	#define RMC_CLOG(Condition, Verbosity, Format, ...)\
		if (Condition) {\
			RMC_LOG(Verbosity, Format, ##__VA_ARGS__)\
			}

	// Function logs for movement component
	#define RMC_FLog(Verbosity, Format, ...)\
		RMC_LOG(Verbosity, TEXT("URadicalMovementComponent::%s: ") TEXT(Format), *FString(__func__), ##__VA_ARGS__)
	#define RMC_CFLog(Condition, Verbosity, Format, ...)\
		RMC_CLOG(Condition, Verbosity, TEXT("URadicalMovementComponent::%s: ") TEXT(Format), *FString(__func__), ##__VA_ARGS__)

#endif



#define LOG_HIT(InHit, Duration)\
	{\
		if (RMCCVars::LogHits) \
		{\
			float DHalfHeight = CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();\
			float DRadius = CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleRadius();\
			RMC_FLog(Warning, "Hit Logged With Vel = %s", *Velocity.ToCompactString());\
			DrawDebugString(GetWorld(), InHit.Location + GetUpOrientation(MODE_PawnUp) * (DHalfHeight + 1.1f * DRadius), FString(__func__), 0, FColor::White, Duration, true);\
			DrawDebugCapsule(GetWorld(), InHit.Location, DHalfHeight, DRadius, UpdatedComponent->GetComponentQuat(), FColor::Purple, false, Duration);\
			DrawDebugSphere(GetWorld(), InHit.ImpactPoint, 5.f, 3, FColor::Red, false, Duration, 0, 2.f);\
			\
			DrawDebugString(GetWorld(), InHit.ImpactPoint + 110.f * InHit.Normal, FString("Normal"), 0, FColor::Yellow, Duration, true);\
			DrawDebugDirectionalArrow(GetWorld(), InHit.ImpactPoint, InHit.ImpactPoint + 100.f*InHit.Normal, 10.f, FColor::Yellow, false, Duration, 0, 5.f);\
			\
			DrawDebugString(GetWorld(), InHit.ImpactPoint + 120.f * InHit.ImpactNormal, FString("Impact Normal"), 0, FColor::Orange, Duration, true);\
			DrawDebugDirectionalArrow(GetWorld(), InHit.ImpactPoint, InHit.ImpactPoint + 100.f*InHit.ImpactNormal, 10.f, FColor::Orange, false, Duration, 0, 5.f);\
		}\
	}

#endif