// Copyright 2023 CoC All rights reserved
#include "InputWindow_InputBuffer.h"
#include "StaticLibraries/InputBufferLibrary.h"
#include <imgui.h>



void FInputWindow_InputBuffer::RenderContent()
{
	UWorld* World = GetWorld();
	if (!World) return;

	auto PC = World->GetFirstPlayerController();

	if (!PC) return;

	auto IB = UInputBuffer::Get(PC->GetPawn());
	
	const TBufferContainer<FBufferFrame> InputData = IB->GetInputBufferData();//InputBuffer->GetInputBufferData();
	const int numInputs = InputData[0].InputsFrameState.Num();
	TArray<FGameplayTag> inputIDs; InputData[0].InputsFrameState.GetKeys(inputIDs);

	auto flags = ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_BordersV | ImGuiTableFlags_RowBg;
	if (ImGui::BeginTable("Input Buffer", numInputs, flags))
	{
		// Setup column names
		for (const FGameplayTag& input : inputIDs)
		{
			TArray<FString> TagDesignations;
			input.ToString().ParseIntoArray(TagDesignations, TEXT("."));
			auto name = StringCast<ANSICHAR>(*TagDesignations.Last());
			ImGui::TableSetupColumn(name.Get());
		}
		ImGui::TableHeadersRow();

		// Bump font size for button value, better readability
		ImGui::SetWindowFontScale(1.5f);

		const int numRows = InputData.GetSize();
		for (int i = 0; i < numRows; i++) // rows
		{
			ImGui::TableNextRow();
			for (const FGameplayTag& input : inputIDs) // columns
			{
				ImGui::TableNextColumn();
		
				const FInputFrameState element = InputData[i].InputsFrameState[input];
				int val = element.HoldTime;

				ImGui::Text("%d", val);
				{
					// Set color based on input state
					ImVec4 colorVal;
					ImU32 cell_bg_color;
					if (element.HoldTime != 0)
					{
						colorVal = ImVec4(0.2f, 0.2f, 0.2f, 1.f);
					}
					if (element.bUsed)
					{
						colorVal = ImVec4(0.f, 0.4f, 0.f, 1.f); // Green = Used
					}
					if (element.CanInvokePress())
					{
						colorVal = ImVec4(0.f, 0.f, 0.4f, 1.f); // Blue = Press
					}
					if (element.CanInvokeRelease())
					{
						colorVal = ImVec4(0.4f, 0.f, 0.f, 1.f); // Red = Release
					}
			
					cell_bg_color = ImGui::GetColorU32(colorVal);;
					ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, cell_bg_color);
				}
			}
		}
		
		ImGui::SetWindowFontScale(1.f);// Restore font size
		ImGui::EndTable();
	}
	
}
