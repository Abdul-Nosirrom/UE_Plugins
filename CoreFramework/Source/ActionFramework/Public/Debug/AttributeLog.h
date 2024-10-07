// Copyright 2023 CoC All rights reserved

#pragma once

#include "CoreMinimal.h"
#include "EngineDefines.h"
#include "VisualLogger/VisualLogger.h"

ACTIONFRAMEWORK_API DECLARE_LOG_CATEGORY_EXTERN(LogAttributeSystem, Display, All);
ACTIONFRAMEWORK_API DECLARE_LOG_CATEGORY_EXTERN(VLogAttributeSystem, Display, All);

#define ATTRIBUTE_LOG(Verbosity, Format, ...) \
{\
	UE_LOG(LogAttributeSystem, Verbosity, Format, ##__VA_ARGS__); \
}

#define ATTRIBUTE_VLOG(Actor, Verbosity, Format, ...) \
{\
	UE_LOG(LogAttributeSystem, Verbosity, Format, ##__VA_ARGS__); \
	UE_VLOG(Actor, VLogAttributeSystem, Verbosity, Format, ##__VA_ARGS__); \
}

#if ENABLE_VISUAL_LOG

#define ATTRIBUTE_VLOG_GRAPH(Actor, Verbosity, AttributeName, OldValue, NewValue) \
{ \
	if( FVisualLogger::IsRecording() ) \
	{ \
		static const FName GraphName("Attribute Graph"); \
		const float CurrentTime = Actor->GetWorld() ? Actor->GetWorld()->GetTimeSeconds() : 0.f; \
		const FVector2D OldPt(CurrentTime, OldValue); \
		const FVector2D NewPt(CurrentTime, NewValue); \
		const FName LineName(*AttributeName); \
		UE_VLOG_HISTOGRAM(Actor, VLogAttributeSystem, Log, GraphName, LineName, OldPt); \
		UE_VLOG_HISTOGRAM(OwnerActor, VLogAttributeSystem, Log, GraphName, LineName, NewPt); \
	} \
}

#else

#define ATTRIBUTE_VLOG_GRAPH(Actor, Verbosity, AttributeName, OldValue, NewValue)

#endif