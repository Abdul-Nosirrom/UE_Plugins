#pragma once
#include "CoreMinimal.h"
#include "AngelscriptBinds.h"
#include "AngelscriptManager.h"

/*
AS_FORCE_LINK const FAngelscriptBinds::FBind Bind_FComponentVisualizerDrawing(FAngelscriptBinds::EOrder::Late, []
{
	// Utility Enums
	{
		auto ESceneDepthPriorityGroup_ = FAngelscriptBinds::Enum("ESceneDepthPriorityGroup");
		ESceneDepthPriorityGroup_["SDPG_World"] = SDPG_World;
		ESceneDepthPriorityGroup_["SDPG_Foreground"] = SDPG_Foreground;
		ESceneDepthPriorityGroup_["SDPG_MAX"] = SDPG_MAX;
	}
});
*/