// Copyright 2023 CoC All rights reserved

#pragma once

#include "CoreMinimal.h"
#include "ImGuiWindow.h"
#include "ImGuiWindowManager.h"
#include "EngineWindow_Plots.generated.h"

struct ImVec2;
struct FImGuiDebugPlotEvent;
struct FImGuiDebugPlotEntry;
class UEngineConfig_Plots;

class FEngineWindow_Plots : public FImGuiWindow
{
protected:

	virtual void Initialize() override;

	virtual void RenderHelp() override;

	virtual void ResetConfig() override;

	virtual void Tick(float DeltaTime) override;

	virtual void RenderContent() override;

	static void RenderPlotsList(TArray<FImGuiDebugPlotEntry*>& VisiblePlots);

	void RenderPlots(const TArray<FImGuiDebugPlotEntry*>& VisiblePlots) const;

	void RenderMenu();

	void RenderTimeMarker() const;

	static void RenderValues(FImGuiDebugPlotEntry& Entry, const char* Label);

	void RenderEvents(FImGuiDebugPlotEntry& Entry, const char* Label, const ImVec2& PlotMin, const ImVec2& PlotMax) const;

	static void RenderEventTooltip(const FImGuiDebugPlotEvent* HoveredEvent, FImGuiDebugPlotEntry& Entry);

	TObjectPtr<UEngineConfig_Plots> Config = nullptr;

	bool bApplyTimeScale = false;
	
};

//--------------------------------------------------------------------------------------------------------------------------
UCLASS(Config = ImGui)
class UEngineConfig_Plots : public UImGuiWindowConfig
{
	GENERATED_BODY()

public:

	UPROPERTY(Config)
	int Rows = 1;

	UPROPERTY(Config)
	float TimeRange = 10.0f;

	virtual void Reset() override
	{
		Rows = 1;
		TimeRange = 10.0f;
	}
};