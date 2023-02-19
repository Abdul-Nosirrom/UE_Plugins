// Copyright 2023 Abdulrahmen Almodaimegh. All Rights Reserved.

#pragma once

#define CFW_MOVEMENT_LOG

/*~~~~~~ General Logging Utility ~~~~~*/

// Get variable name as string
#define VAR2STR(Var) (#Var)

// Print value of a bool expression
#define BOOL2STR(Expr) ((Expr) ? TEXT("TRUE") : TEXT("FALSE"))

// Enables a nicer way to handle unused out parameters
template<typename T> T& Unused(T&& Var) { return Var; }
#define UNUSED(Type) (Unused(Type{}))

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */


/*~~~~~ Core Logging ~~~~~*/

// References to shit
#undef PAWN_REF
#undef ACTOR_REF
#undef LOG_CATEGORY


#define DEBUG_BREAK GEngine->DeferredCommands.Add(TEXT("pause"));

#if !NO_LOGGING

// Print a message on the screen for a specific duration
#ifdef __COUNTER__
	#define DEBUG_PRINT_MSG(Duration, FormatStr, ...)\
		if (GEngine) {\
			GEngine->AddOnScreenDebugMessage(\
			uint64(sizeof(__FILE__) + sizeof(__func__) + __LINE__ + __COUNTER__),\
			Duration,\
			FColor::Orange,\
			FString::Printf(TEXT(FormatStr), ##__VA_ARGS__),\
			false,\
			FVector2D(0.9, 0.9)\
			);\
		}
#else
	#define DEBUG_PRINT_MSG(Duration, FormatStr, ...)
#endif

#define LOG_HIT(InHit, Duration)\
				{\
					FLog(Warning, "Hit Logged With Vel = %s", *Velocity.ToCompactString());\
					DrawDebugString(GetWorld(), InHit.Location + GetUpOrientation(MODE_PawnUp) * (GetCapsuleHalfHeight() + 1.1f * GetCapsuleRadius()), FString(__func__), 0, FColor::White, Duration, true);\
    				DrawDebugCapsule(GetWorld(), InHit.Location, GetCapsuleHalfHeight(), GetCapsuleRadius(), UpdatedComponent->GetComponentQuat(), FColor::Purple, false, Duration);\
    				DrawDebugSphere(GetWorld(), InHit.ImpactPoint, 5.f, 3, FColor::Red, false, Duration, 0, 2.f);\
    \
    				DrawDebugString(GetWorld(), InHit.ImpactPoint + 110.f * InHit.Normal, FString("Normal"), 0, FColor::Yellow, Duration, true);\
    				DrawDebugDirectionalArrow(GetWorld(), InHit.ImpactPoint, InHit.ImpactPoint + 100.f*InHit.Normal, 10.f, FColor::Yellow, false, Duration, 0, 5.f);\
    \
    				DrawDebugString(GetWorld(), InHit.ImpactPoint + 120.f * InHit.ImpactNormal, FString("Impact Normal"), 0, FColor::Orange, Duration, true);\
    				DrawDebugDirectionalArrow(GetWorld(), InHit.ImpactPoint, InHit.ImpactPoint + 100.f*InHit.ImpactNormal, 10.f, FColor::Orange, false, Duration, 0, 5.f);\
    			}

// Specific Logging
#ifdef CFW_MOVEMENT_LOG
	#define PAWN_REF PawnOwner
	#define LOG_CATEGORY LogRMCMovement
#endif

#if defined PAWN_REF && defined LOG_CATEGORY

	// Logging macro wrapper
	#define RMC_LOG(Verbosity, Format, ...)\
		{\
			if (PAWN_REF) {\
				FString\
				rmc_Time = FDateTime::Now().GetTimeOfDay().ToString(),\
				rmc_ActorName = PAWN_REF->GetName();\
				LOG(LOG_CATEGORY, Verbosity, OBJECT_INFO_TEXT(Format),\
					OBJECT_INFO(rmc_Time, rmc_ActorName), ##__VA_ARGS__)\
			}\
			else {\
				LOG(LOG_CATEGORY, Verbosity, TEXT("| No debug info available |") Format, ##__VA_ARGS__)\
			}\
		}

	// Conditional Logging
	#define RMC_CLOG(Condition, Verbosity, Format, ...)\
		if (Condition) {\
			RMC_LOG(Verbosity, Format, ##__VA_ARGS__)\
		}

	// Object info logs
	#define OBJECT_INFO_TEXT(Text) TEXT("%s %-20s: " Text)
	#define OBJECT_INFO(Time, ActorName) *Time, *ActorName

	#define LOG(Category, Verbosity, Format, ...) UE_LOG(Category, Verbosity, Format, ##__VA_ARGS__)

	// Function logs for movement component
	#define FLog(Verbosity, Format, ...)\
		RMC_LOG(Verbosity, TEXT("URadicalMovementComponent::%s: ") TEXT(Format), *FString(__func__), ##__VA_ARGS__)
	#define CFLog(Condition, Verbosity, Format, ...)\
		RMC_LOG(Condition, Verbosity, TEXT("URadicalMovementComponent::%s: ") TEXT(Format), *FString(__func__), ##__VA_ARGS__)

#endif

#else
	#define DEBUG_PRINT_MSG(Duration, FormatStr, ...)
	#define RMC_LOG(Verbosity, Format, ...)
	#define RMC_CLOG(Condition, Verbosity, Format, ...)
	#define LOG(Category, Verbosity, Format, ...)
	#define FLog(Verbosity, Format, ...)
	#define CFLog(Condition, Verbosity, Format, ...)
#endif

/* ~~~~~~~~~~~~~~~~~~~~~ */

