// Copyright 2023 CoC All rights reserved

#pragma once

#include "CoreMinimal.h"
#include "EngineDefines.h"
#include "VisualLogger/VisualLogger.h"

COMBATFRAMEWORK_API DECLARE_LOG_CATEGORY_EXTERN(LogCombatSystem, Display, All);
COMBATFRAMEWORK_API DECLARE_LOG_CATEGORY_EXTERN(VLogCombatSystem, Display, All);

COMBATFRAMEWORK_API DECLARE_LOG_CATEGORY_EXTERN(LogTargetingSystem, Display, All);
COMBATFRAMEWORK_API DECLARE_LOG_CATEGORY_EXTERN(VLogTargetingSystem, Display, All);


#define COMBAT_LOG(Verbosity, Format, ...) \
{\
	UE_LOG(LogCombatSystem, Verbosity, Format, ##__VA_ARGS__);\
}

#define TARGETING_LOG(Verbosity, Format, ...) \
{\
	UE_LOG(LogTargetingSystem, Verbosity, Format, ##__VA_ARGS__);\
}

#if ENABLE_VISUAL_LOG

#define TARGETING_VLOG_LOCATION(Actor, Verbosity, Location, Format, ...) \
{ \
	UE_VLOG_LOCATION(Actor, VLogTargetingSystem, Verbose, Location, 25.f, FColor::Yellow, Format, ##__VA_ARGS__);\
}

#else

#define TARGETING_VLOG_LOCATION(Actor, Verbosity, Location, Format, ...)

#endif
