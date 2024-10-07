// Copyright 2023 CoC All rights reserved


#include "EngineWindow_Plots.h"

#include "ImGuiDebugPlot.h"
#include "ImGuiWidgets.h"
#include "Private/implot_internal.h"

//--------------------------------------------------------------------------------------------------------------------------
void FEngineWindow_Plots::Initialize()
{
    bHasMenu = true;
    bNoPadding = true;

    Config = GetConfig<UEngineConfig_Plots>();
}

//--------------------------------------------------------------------------------------------------------------------------
void FEngineWindow_Plots::RenderHelp()
{
    ImGui::Text(
        "This window plots values and events overtime. When applicable, only the values of the selected actor are displayed."
    );
}

//--------------------------------------------------------------------------------------------------------------------------
void FEngineWindow_Plots::ResetConfig()
{
    Config->Reset();
}


//--------------------------------------------------------------------------------------------------------------------------
void FEngineWindow_Plots::Tick(float DeltaTime)
{
    FImGuiDebugPlot::IsVisible = IsVisible();
}

//--------------------------------------------------------------------------------------------------------------------------
void FEngineWindow_Plots::RenderContent()
{
    RenderMenu();
    
    if (ImGui::BeginTable("PlotTable", 2, ImGuiTableFlags_RowBg 
                                        //| ImGuiTableFlags_Resizable 
                                        | ImGuiTableFlags_BordersV
                                        | ImGuiTableFlags_NoPadInnerX
                                        | ImGuiTableFlags_NoPadOuterX))
    {

        ImGui::TableSetupColumn("PlotsList", ImGuiTableColumnFlags_WidthFixed, UImGuiHelperWidgets::GetFontWidth() * 20.0f);
        ImGui::TableSetupColumn("Plots", ImGuiTableColumnFlags_WidthStretch, 0.0f);
        ImGui::TableNextRow();

        TArray<FImGuiDebugPlotEntry*> VisiblePlots;

        ImGui::TableNextColumn();
        RenderPlotsList(VisiblePlots);

        ImGui::TableNextColumn();
        RenderPlots(VisiblePlots);

        ImGui::EndTable();
    }

    bApplyTimeScale = false;
}

//--------------------------------------------------------------------------------------------------------------------------
void FEngineWindow_Plots::RenderMenu()
{
    if (ImGui::BeginMenuBar())
    {
        if (ImGui::BeginMenu("Options"))
        {
            if (ImGui::MenuItem("Clear Data"))
            {
                FImGuiDebugPlot::Clear();
            }

            if (ImGui::MenuItem("Reset"))
            {
                FImGuiDebugPlot::Pause = false;
                FImGuiDebugPlot::Reset();
                ResetConfig();
            }

            ImGui::Separator();

            UImGuiHelperWidgets::SetNextItemToShortWidth();
            if (ImGui::SliderInt("Rows", &Config->Rows, 1, 5))
            {
                bApplyTimeScale = true;
            }
            
            UImGuiHelperWidgets::SetNextItemToShortWidth();
            if (ImGui::SliderFloat("Time Range", &Config->TimeRange, 1.0f, 30.0f, "%0.1f"))
            {
                bApplyTimeScale = true;
            }
            
            ImGui::EndMenu();
        }

        UImGuiHelperWidgets::ToggleMenuButton(FImGuiDebugPlot::Pause, "Pause", FVector4(1.0f, 0.0f, 0.0f, 1.0f));

        ImGui::EndMenuBar();
    }
}

//--------------------------------------------------------------------------------------------------------------------------
void FEngineWindow_Plots::RenderPlotsList(TArray<FImGuiDebugPlotEntry*>& VisiblePlots)
{
    if (ImGui::BeginChild("Plots", ImVec2(0, -1)))
    {
        int Index = 0;

        ImGui::Indent(6);

        for (FImGuiDebugPlotEntry& Entry : FImGuiDebugPlot::Plots)
        {
            if (Entry.CurrentYAxis != ImAxis_COUNT && Entry.CurrentRow != INDEX_NONE)
            {
                VisiblePlots.Add(&Entry);
            }

            ImGui::PushID(Index);

            ImGui::PushStyleColor(ImGuiCol_Text, Entry.IsEventPlot ? IM_COL32(128, 128, 255, 255) : IM_COL32(255, 255, 255, 255));
            ImGui::Selectable(TCHAR_TO_ANSI(*Entry.Name.ToString()), false, 0);
            ImGui::PopStyleColor();

            if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None))
            {
                const auto EntryName = StringCast<ANSICHAR>(*Entry.Name.ToString());
                ImGui::SetDragDropPayload("DragAndDrop", EntryName.Get(), EntryName.Length() + 1);
                ImGui::Text("%s", EntryName.Get());
                ImGui::EndDragDropSource();
            }

            ImGui::PopID();
            Index++;
        }

        ImGui::Unindent();
    }
    ImGui::EndChild();

    if (ImGui::BeginDragDropTarget())
    {
        if (const ImGuiPayload* Payload = ImGui::AcceptDragDropPayload("DragAndDrop"))
        {
            if (FImGuiDebugPlotEntry* Plot = FImGuiDebugPlot::FindPlot(FName((const char*)Payload->Data)))
            {
                Plot->ResetAxis();
            }
        }
        ImGui::EndDragDropTarget();
    }
}

//--------------------------------------------------------------------------------------------------------------------------
void FEngineWindow_Plots::RenderPlots(const TArray<FImGuiDebugPlotEntry*>& VisiblePlots) const
{
    if (ImGui::BeginChild("Graph", ImVec2(0, -1)))
    {
        static float RowRatios[] = { 1, 1, 1, 1, 1, 1 };
        static float ColRatios[] = { 1 };
        static ImPlotSubplotFlags SubplotsFlags = ImPlotSubplotFlags_LinkCols;
        if (ImPlot::BeginSubplots("", Config->Rows, 1, ImVec2(-1, -1), SubplotsFlags, RowRatios, ColRatios))
        {
            for (int PlotIndex = 0; PlotIndex < Config->Rows; ++PlotIndex)
            {
                if (ImPlot::BeginPlot("##Plot", ImVec2(-1, 250)))
                {
                    ImPlotAxisFlags HasPlotOnAxisY1 = false;
                    ImPlotAxisFlags HasPlotOnAxisY2 = false;
                    ImPlotAxisFlags HasPlotOnAxisY3 = false;

                    for (const FImGuiDebugPlotEntry* PlotPtr : VisiblePlots)
                    {
                        HasPlotOnAxisY1 |= PlotPtr->CurrentYAxis == ImAxis_Y1 && PlotPtr->CurrentRow == PlotIndex;
                        HasPlotOnAxisY2 |= PlotPtr->CurrentYAxis == ImAxis_Y2 && PlotPtr->CurrentRow == PlotIndex;
                        HasPlotOnAxisY3 |= PlotPtr->CurrentYAxis == ImAxis_Y3 && PlotPtr->CurrentRow == PlotIndex;
                    }

                    ImPlot::SetupAxis(ImAxis_X1, nullptr, ImPlotAxisFlags_NoTickLabels | ImPlotAxisFlags_NoGridLines);
                    ImPlot::SetupAxis(ImAxis_Y1, HasPlotOnAxisY1 ? "" : "[drop here]", (HasPlotOnAxisY1 ? ImPlotAxisFlags_None : (ImPlotAxisFlags_NoTickLabels | ImPlotAxisFlags_NoGridLines)) | ImPlotAxisFlags_AutoFit);
                    ImPlot::SetupAxis(ImAxis_Y2, HasPlotOnAxisY2 ? "" : "[drop here]", (HasPlotOnAxisY2 ? ImPlotAxisFlags_None : (ImPlotAxisFlags_NoTickLabels | ImPlotAxisFlags_NoGridLines)) | ImPlotAxisFlags_AutoFit | ImPlotAxisFlags_Opposite);
                    ImPlot::SetupAxis(ImAxis_Y3, HasPlotOnAxisY3 ? "" : "[drop here]", (HasPlotOnAxisY3 ? ImPlotAxisFlags_None : (ImPlotAxisFlags_NoTickLabels | ImPlotAxisFlags_NoGridLines)) | ImPlotAxisFlags_AutoFit | ImPlotAxisFlags_Opposite);

                    //--------------------------------------------------------------------------------------------------
                    // Set the initial X axis range. After, it is automatically updated to move with the current time.
                    //--------------------------------------------------------------------------------------------------
                    ImPlot::SetupAxisLimits(ImAxis_X1, 0, Config->TimeRange, ImGuiCond_Appearing);

                    const ImPlotRange& PlotRange = ImPlot::GetCurrentPlot()->Axes[ImAxis_X1].Range;
                    const float TimeRange = PlotRange.Max - PlotRange.Min;

                    if (bApplyTimeScale == false)
                    {
                        Config->TimeRange = TimeRange;
                    }

                    const ImVec2 PlotMin = ImPlot::GetPlotPos();
                    const ImVec2 PlotSize = ImPlot::GetPlotSize();
                    const ImVec2 PlotMax = PlotMin + PlotSize;

                    //----------------------------------------------------------------
                    // Draw a vertical lines representing the current time and the mouse time
                    //----------------------------------------------------------------
                    RenderTimeMarker();

                    for (FImGuiDebugPlotEntry* PlotPtr : VisiblePlots)
                    {
                        if (PlotPtr == nullptr)
                        {
                            continue;
                        }

                        FImGuiDebugPlotEntry& Entry = *PlotPtr;

                        if (Entry.Values.empty()) 
                        {
                            continue;
                        }

                        if (Entry.CurrentRow == PlotIndex)
                        {
                            //--------------------------------------------------------------------------------
                            // Make the time axis move forward automatically, unless the user pauses or zoom.
                            //--------------------------------------------------------------------------------
                            if (FImGuiDebugPlot::Pause == false && ImGui::GetIO().MouseWheel == 0)
                            {
                                ImPlot::SetupAxisLimits(ImAxis_X1, Entry.Time - TimeRange, Entry.Time, ImGuiCond_Always);
                            }

                            if (bApplyTimeScale)
                            {
                                ImPlot::SetupAxisLimits(ImAxis_X1, Entry.Time - Config->TimeRange, Entry.Time, ImGuiCond_Always);
                            }

                            ImPlot::SetAxis(Entry.CurrentYAxis);

                            ImPlot::SetNextLineStyle(IMPLOT_AUTO_COL);
                            const auto Label = StringCast<ANSICHAR>(*Entry.Name.ToString());

                            //----------------------------------------------------------------
                            // Pause the scrolling if the user drag inside
                            //----------------------------------------------------------------
                            const ImVec2 Mouse = ImGui::GetMousePos();
                            if (Mouse.x > PlotMin.x
                                && Mouse.y > PlotMin.y
                                && Mouse.x < PlotMax.x
                                && Mouse.y < PlotMax.y
                                && ImGui::GetDragDropPayload() == nullptr)
                            {
	                            const ImVec2 Drag = ImGui::GetMouseDragDelta(0);
                                if (FMath::Abs(Drag.x) > 10)
                                {
                                    FImGuiDebugPlot::Pause = true;
                                }
                            }

                            //-------------------------------------------------------
                            // Plot Events
                            //-------------------------------------------------------
                            const bool IsEventPlot = Entry.Events.Num() > 0;
                            if (IsEventPlot)
                            {
                                RenderEvents(Entry, Label.Get(), PlotMin, PlotMax);
                            }
                            //-------------------------------------------------------
                            // Plot Values
                            //-------------------------------------------------------
                            else
                            {
                                RenderValues(Entry, Label.Get());
                            }

                            //-------------------------------------------------------
                            // Allow legend item labels to be drag and drop sources
                            //-------------------------------------------------------
                            if (ImPlot::BeginDragDropSourceItem(Label.Get()))
                            {
                                const auto EntryName = StringCast<ANSICHAR>(*Entry.Name.ToString());
                                ImGui::SetDragDropPayload("DragAndDrop", EntryName.Get(), EntryName.Length() + 1);
                                ImGui::TextUnformatted(EntryName.Get());
                                ImPlot::EndDragDropSource();
                            }
                        }

                    }

                    //-------------------------------------------------------
                    // Allow the main plot area to be a drag and drop target
                    //-------------------------------------------------------
                    if (ImPlot::BeginDragDropTargetPlot())
                    {
                        if (const ImGuiPayload* Payload = ImGui::AcceptDragDropPayload("DragAndDrop"))
                        {
                            if (FImGuiDebugPlotEntry* Plot = FImGuiDebugPlot::FindPlot(FName((const char*)Payload->Data)))
                            {
                                Plot->AssignAxis(PlotIndex, ImAxis_Y1);
                            }
                        }
                        ImPlot::EndDragDropTarget();
                    }

                    //-------------------------------------------------------
                    // Allow each y-axis to be a drag and drop target
                    //-------------------------------------------------------
                    for (int y = ImAxis_Y1; y <= ImAxis_Y3; ++y)
                    {
                        if (ImPlot::BeginDragDropTargetAxis(y))
                        {
                            if (const ImGuiPayload* Payload = ImGui::AcceptDragDropPayload("DragAndDrop"))
                            {
                                if (FImGuiDebugPlotEntry* Plot = FImGuiDebugPlot::FindPlot(FName((const char*)Payload->Data)))
                                {
                                    Plot->AssignAxis(PlotIndex, y);
                                }
                            }
                            ImPlot::EndDragDropTarget();
                        }
                    }

                    //-------------------------------------------------------
                    // Allow the legend to be a drag and drop target
                    //-------------------------------------------------------
                    if (ImPlot::BeginDragDropTargetLegend())
                    {
                        if (const ImGuiPayload* Payload = ImGui::AcceptDragDropPayload("DragAndDrop"))
                        {
                            if (FImGuiDebugPlotEntry* Plot = FImGuiDebugPlot::FindPlot(FName((const char*)Payload->Data)))
                            {
                                Plot->AssignAxis(PlotIndex, ImAxis_Y1);
                            }
                        }
                        ImPlot::EndDragDropTarget();
                    }

                    ImPlot::EndPlot();
                }
            }
            ImPlot::EndSubplots();
        }
    }
    ImGui::EndChild();
}

//--------------------------------------------------------------------------------------------------------------------------
void FEngineWindow_Plots::RenderTimeMarker() const
{
    const ImVec2 PlotMin = ImPlot::GetPlotPos();
    const ImVec2 PlotSize = ImPlot::GetPlotSize();
    ImDrawList* PlotDrawList = ImPlot::GetPlotDrawList();

    const float PlotTop = PlotMin.y;
    const float TimeBarBottom = PlotTop + PlotSize.y;

    ImPlot::PushPlotClipRect();
    PlotDrawList->AddLine(ImVec2(ImGui::GetMousePos().x, PlotTop), ImVec2(ImGui::GetMousePos().x, TimeBarBottom), IM_COL32(128, 128, 128, 64));
    if (FImGuiDebugPlot::Pause)
    {
        const float Time = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
        const float TimeBarX = ImPlot::PlotToPixels(Time, 0.0f).x;
        PlotDrawList->AddLine(ImVec2(TimeBarX, PlotTop), ImVec2(TimeBarX, TimeBarBottom), IM_COL32(255, 255, 255, 64));
    }
    ImPlot::PopPlotClipRect();
}

//--------------------------------------------------------------------------------------------------------------------------
void FEngineWindow_Plots::RenderValues(FImGuiDebugPlotEntry& Entry, const char* Label)
{
    //----------------------------------------------------------------
    // Custom tooltip
    //----------------------------------------------------------------
    if (ImPlot::IsPlotHovered())
    {
        float Value;
        if (Entry.FindValue(ImPlot::GetPlotMousePos().x, Value))
        {
            ImGui::BeginTooltip();
            ImGui::Text("%s: %0.2f", Label, Value);
            ImGui::EndTooltip();
        }
    }

    if (Entry.ShowValuesMarkers)
    {
        ImPlot::SetNextMarkerStyle(ImPlotMarker_Circle);
    }

    ImPlot::PlotLine(Label, &Entry.Values[0].x, &Entry.Values[0].y, Entry.Values.size(), ImPlotLineFlags_None, Entry.ValueOffset, 2 * sizeof(float));

    if (ImPlot::BeginLegendPopup(Label))
    {
        if (ImGui::Button("Clear"))
        {
            Entry.Clear();
        }
        ImGui::Checkbox("Show Markers", &Entry.ShowValuesMarkers);
        ImPlot::EndLegendPopup();
    }
}

//--------------------------------------------------------------------------------------------------------------------------
void FEngineWindow_Plots::RenderEvents(FImGuiDebugPlotEntry& Entry, const char* Label, const ImVec2& PlotMin, const ImVec2& PlotMax) const
{
    const ImVec2 Mouse = ImGui::GetMousePos();
    ImDrawList* PlotDrawList = ImPlot::GetPlotDrawList();

    //--------------------------------------------------------------------
    // Update plot time for events as events are not pushed every frames
    //--------------------------------------------------------------------
    Entry.UpdateTime(GetWorld());

    ImPlot::SetupAxisLimits(Entry.CurrentYAxis, 0, Entry.MaxRow + 2, ImGuiCond_Always);

    ImPlot::PushPlotClipRect();


    //----------------------------------------------------------------
    // Plot line only to make the plotter move in time and auto scale
    //----------------------------------------------------------------
    ImVector<ImVec2> DummyData;
    DummyData.push_back(ImVec2(0, 0));
    DummyData.push_back(ImVec2(0, 8));
    ImPlot::PlotLine(Label, &DummyData[0].x, &DummyData[0].y, DummyData.size(), Entry.ValueOffset, 2 * sizeof(float));

    const FImGuiDebugPlotEvent* HoveredEvent = nullptr;

    for (const FImGuiDebugPlotEvent& Event : Entry.Events)
    {
        const ImVec2 PosBot = ImPlot::PlotToPixels(ImPlotPoint(Event.StartTime, Event.Row + 0.8f));
        const ImVec2 PosTop = ImPlot::PlotToPixels(ImPlotPoint(Event.StartTime, Event.Row + 0.2f));
        const ImVec2 PosMid(PosBot.x, PosBot.y + (PosTop.y - PosBot.y) * 0.5f);

        const bool IsInstant = Event.StartTime == Event.EndTime;
        if (IsInstant)
        {
	        constexpr float Radius = 10.0f;
            PlotDrawList->AddNgon(PosMid, 10, Event.BorderColor, 4);
            PlotDrawList->AddNgonFilled(PosMid, 10, Event.FillColor, 4);
            PlotDrawList->AddText(ImVec2(PosMid.x + 15, PosMid.y - 6), IM_COL32(255, 255, 255, 255), TCHAR_TO_ANSI(*Event.DisplayName));

            if ((Mouse.x > PosMid.x - Radius) && (Mouse.x < PosMid.x + Radius) && (Mouse.y > PosMid.y - Radius) && (Mouse.y < PosMid.y + Radius))
            {
                HoveredEvent = &Event;
            }
        }
        else
        {
            const float ActualEndTime = Event.GetActualEndTime(Entry);
            const ImVec2 PosEnd = ImPlot::PlotToPixels(ImPlotPoint(ActualEndTime, 0));
            const ImVec2 Min = ImVec2(PosBot.x, PosBot.y);
            const ImVec2 Max = ImVec2(PosEnd.x, PosTop.y);

            const ImDrawFlags Flags = Event.EndTime == 0.0f ? ImDrawFlags_RoundCornersLeft : ImDrawFlags_RoundCornersAll;
            PlotDrawList->AddRect(Min, Max, Event.BorderColor, 6.0f, Flags);
            PlotDrawList->AddRectFilled(Min, Max, Event.FillColor, 6.0f, Flags);
            PlotDrawList->PushClipRect(ImMax(Min, PlotMin), ImMin(Max, PlotMax));
            PlotDrawList->AddText(ImVec2(PosMid.x + 5, PosMid.y - 7), IM_COL32(255, 255, 255, 255), TCHAR_TO_ANSI(*Event.DisplayName));
            PlotDrawList->PopClipRect();

            if (Mouse.x > Min.x && Mouse.x < Max.x && Mouse.y > Min.y && Mouse.y < Max.y)
            {
                HoveredEvent = &Event;
            }
        }
    }

    //-------------------------------------------------------
    // Write info on the graph to help debugging
    //-------------------------------------------------------
    //char Buffer[64];
    //ImFormatString(Buffer, 64, "%0.1f %0.1f", Mouse.x, Mouse.y);
    //PlotDrawList->AddText(ImVec2(PlotMin.x + 50, PlotMin.y + 100), IM_COL32(255, 255, 255, 255), Buffer);

    //-------------------------------------------------------
    // Hovered event tooltip
    //-------------------------------------------------------
    RenderEventTooltip(HoveredEvent, Entry);

    ImPlot::PopPlotClipRect();
}

//--------------------------------------------------------------------------------------------------------------------------
void FEngineWindow_Plots::RenderEventTooltip(const FImGuiDebugPlotEvent* HoveredEvent, FImGuiDebugPlotEntry& Entry)
{
    if (ImPlot::IsPlotHovered() && HoveredEvent != nullptr)
    {
        UImGuiHelperWidgets::BeginTableTooltip();
        if (ImGui::BeginTable("Params", 2, ImGuiTableFlags_Borders))
        {
            //------------------------
            // Event Name
            //------------------------
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("Name");
            ImGui::TableNextColumn();
            ImGui::Text("%s", TCHAR_TO_ANSI(*HoveredEvent->DisplayName));

            //------------------------
            // Owner Name
            //------------------------
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("Owner");
            ImGui::TableNextColumn();
            ImGui::Text("%s", TCHAR_TO_ANSI(*HoveredEvent->OwnerName));

            //------------------------
            // Times
            //------------------------
            if (HoveredEvent->EndTime != HoveredEvent->StartTime)
            {
                const float ActualEndTime = HoveredEvent->GetActualEndTime(Entry);
                const uint64 ActualEndFrame = HoveredEvent->GetActualEndFrame(Entry);

                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::Text("Duration");
                ImGui::TableNextColumn();
                ImGui::Text("%0.2fs", ActualEndTime - HoveredEvent->StartTime);

                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::Text("Frames");
                ImGui::TableNextColumn();
                ImGui::Text("%d  [%d-%d]",
                    (int32)(ActualEndFrame - HoveredEvent->StartFrame),
                    (int32)(HoveredEvent->StartFrame % 1000),
                    (int32)(ActualEndFrame % 1000));
            }
            else
            {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::Text("Frame");
                ImGui::TableNextColumn();
                ImGui::Text("%d", (int32)(HoveredEvent->StartFrame % 1000));
            }

            //------------------------
            // Params
            //------------------------
            for (FImGuiDebugPlotEventParams Param : HoveredEvent->Params)
            {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::Text("%s", TCHAR_TO_ANSI(*Param.Name.ToString()));
                ImGui::TableNextColumn();
                ImGui::Text("%s", TCHAR_TO_ANSI(*Param.Value));
            }
            ImGui::EndTable();
        }
        UImGuiHelperWidgets::EndTableTooltip();
    }
}
