// Copyright 2023 CoC All rights reserved

#pragma once

#include "CoreMinimal.h"
#include "VisualLogger/VisualLogger.h"

INTERACTIONFRAMEWORK_API DECLARE_LOG_CATEGORY_EXTERN(LogInteractions, Display, All);
INTERACTIONFRAMEWORK_API DECLARE_LOG_CATEGORY_EXTERN(VLogInteractions, Display, All);

DECLARE_STATS_GROUP(TEXT("InteractionSystem_Game"), STATGROUP_InteractionSystem, STATCAT_Advanced)

#define INTERACTABLE_LOG(Verbosity, Format, ...)\
{\
	FString int_time = FDateTime::Now().GetTimeOfDay().ToString();\
	FString className = StaticClass()->GetName();\
	UE_LOG(LogInteractions, Verbosity, TEXT("[%s] %s::%s ")Format, *int_time, *className, *FString(__func__), ##__VA_ARGS__)\
}

#define INTERACTABLE_VLOG(Actor, Verbosity, Format, ...)\
{\
	FString int_time = FDateTime::Now().GetTimeOfDay().ToString();\
	FString className = StaticClass()->GetName();\
	UE_LOG(LogInteractions, Verbosity, TEXT("[%s] %s::%s ")Format, *int_time, *className, *FString(__func__), ##__VA_ARGS__);\
	UE_VLOG(Actor, VLogInteractions, Verbosity, TEXT("[%s] %s::%s ")Format, *int_time, *className, *FString(__func__), ##__VA_ARGS__);\
}

