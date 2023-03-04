// Copyright 2023 Abdulrahmen Almodaimegh. All Rights Reserved.

#pragma once

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


#define LOG(Category, Verbosity, Format, ...) UE_LOG(Category, Verbosity, Format, ##__VA_ARGS__)

#else
	#define DEBUG_PRINT_MSG(Duration, FormatStr, ...)

	#define RMC_LOG(Verbosity, Format, ...)
	#define RMC_CLOG(Condition, Verbosity, Format, ...)
	#define IB_LOG(Verbosity, Format, ...)
	#define IB_CLOG(Condition, Verbosity, Format, ...)

	#define LOG(Category, Verbosity, Format, ...)

	#define RMC_FLog(Verbosity, Format, ...)
	#define RMC_CFLog(Condition, Verbosity, Format, ...)
	#define IB_FLog(Verbosity, Format, ...)
	#define IB_CFLog(Condition, Verbosity, Format, ...)
#endif

/* ~~~~~~~~~~~~~~~~~~~~~ */

