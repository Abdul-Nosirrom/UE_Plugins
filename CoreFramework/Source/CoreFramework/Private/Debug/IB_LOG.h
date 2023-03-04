#pragma once

#include "Debug/CFW_LOG.h"

#undef IB_LOG_CATEGORY

#if !NO_LOGGING

#define IB_OBJECT_INFO_TEXT(Text) TEXT("%s %-20s: " Text)
#define IB_OBJECT_INFO(Time, ActorName) *Time, *ActorName

// Input Buffer Logging Category
#ifdef CFW_BUFFER_LOG
	#define IB_LOG_CATEGORY LogInputBuffer
	#define PLAYER_HANDLE (GetWorld() && GetLocalPlayer<ULocalPlayer>()->GetPlayerController(GetWorld()) ? GetLocalPlayer<ULocalPlayer>()->GetPlayerController(GetWorld())->GetPawn() : nullptr)
#endif

#if defined IB_LOG_CATEGORY

	// Logging macro wrapper
	#define IB_LOG(Verbosity, Format, ...)\
	{\
		if (PLAYER_HANDLE){\
			FString ib_Time = FDateTime::Now().GetTimeOfDay().ToString();\
			LOG(IB_LOG_CATEGORY, Verbosity, IB_OBJECT_INFO_TEXT(Format),\
			IB_OBJECT_INFO(ib_Time, PLAYER_HANDLE->GetName()), ##__VA_ARGS__)\
		}\
	}

	// Conditional Logging
	#define IB_CLOG(Condition, Verbosity, Format, ...)\
		if (Condition) {\
			IB_LOG(Verbosity, Format, ##__VA_ARGS__)\
			}

	// Function logs for movement component
	#define IB_FLog(Verbosity, Format, ...)\
		IB_LOG(Verbosity, TEXT("UInputBufferSubsystem::%s: ") TEXT(Format), *FString(__func__), ##__VA_ARGS__)
	#define IB_CFLog(Condition, Verbosity, Format, ...)\
		IB_CLOG(Condition, Verbosity, TEXT("UInputBufferSubsystem::%s: ") TEXT(Format), *FString(__func__), ##__VA_ARGS__)

#endif

#endif