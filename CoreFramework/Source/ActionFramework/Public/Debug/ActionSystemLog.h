// Copyright 2023 CoC All rights reserved

#pragma once

#include "CoreMinimal.h"
#include "EngineDefines.h"
#include "VisualLogger/VisualLogger.h"

ACTIONFRAMEWORK_API DECLARE_LOG_CATEGORY_EXTERN(LogActionSystem, Display, All)
ACTIONFRAMEWORK_API DECLARE_LOG_CATEGORY_EXTERN(VLogActionSystem, Display, All)

DECLARE_STATS_GROUP(TEXT("ActionSystem_Game"), STATGROUP_ActionSystem, STATCAT_Advanced)

#define ACTIONSYSTEM_LOG(Verbosity, Format, ...)\
{\
	FString int_time = FDateTime::Now().GetTimeOfDay().ToString();\
	FString className = StaticClass()->GetName();\
	UE_LOG(LogActionSystem, Verbosity, TEXT("[%s] %s::%s ")Format, *int_time, *className, *FString(__func__), ##__VA_ARGS__)\
}

#define ACTIONSYSTEM_VLOG(Actor, Verbosity, Format, ...)\
{\
	FString int_time = FDateTime::Now().GetTimeOfDay().ToString();\
	FString className = StaticClass()->GetName();\
	UE_LOG(LogActionSystem, Verbosity, TEXT("[%s] %s::%s ")Format, *int_time, *className, *FString(__func__), ##__VA_ARGS__);\
	UE_VLOG(Actor, VLogActionSystem, Verbosity, TEXT("[%s] %s::%s ")Format, *int_time, *className, *FString(__func__), ##__VA_ARGS__);\
}

