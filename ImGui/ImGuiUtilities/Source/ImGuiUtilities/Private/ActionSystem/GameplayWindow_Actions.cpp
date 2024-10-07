// Copyright 2023 CoC All rights reserved


#include "GameplayWindow_Actions.h"

#include "ActionSystem/Conditions/ActionCondition_CoolDown.h"
#include "Components/ActionSystemComponent.h"
#include "Engine/EngineWindow_PropertyInspector.h"


void FGameplayWindow_Actions::Initialize()
{
	FImGuiWindow::Initialize();

	PropertyWindow = new FEngineWindow_PropertyInspector();
	PropertyWindow->SetOwner(GetOwner());
	PropertyWindow->Initialize();
}

void FGameplayWindow_Actions::RenderContent()
{
	FImGuiWindow::RenderContent();

	auto Player = GetLocalPlayerPawn();
	if (!Player) return;

	ACS = Player->GetComponentByClass<UActionSystemComponent>();
	if (!ACS)
	{
		ImGui::TextDisabled("Selection has no action system component");
		return;
	}

	// 3 headers [Tags / Running Action Info / All Actions Table]
	if (ImGui::CollapsingHeader("Gameplay Tags"))
	{
		RenderGameplayTags();
	}

	if (ImGui::CollapsingHeader("Active Actions"))
	{
		RenderActiveActions();
	}

	if (ImGui::CollapsingHeader("All Actions"))
	{
		// Button to force activation
		// Action Name
		// Viability (is ticking)
		// Is Blocked (color red if blocked)
		// Cooldown (color red if on cooldown)
		// Can be activated (color blue if can be activated, red if absolutely cant)
		// Time activated (activation info in general, DORMENT if inactive, time if is active, color row green when activated)
		auto flags = ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_BordersV | ImGuiTableFlags_RowBg;
		if (ImGui::BeginTable("Available Actions", 6, flags))
		{
			ImGui::TableSetupColumn("");
			ImGui::TableSetupColumn("Action");
			ImGui::TableSetupColumn("Viability");
			ImGui::TableSetupColumn("Blocked");
			ImGui::TableSetupColumn("Cooldown");
			ImGui::TableSetupColumn("Time Activated");
			ImGui::TableHeadersRow();

			TArray<UGameplayAction*> Actions;
			ACS->ActivatableActions.GenerateValueArray(Actions);

			int rowID = 0;
			for (auto* Action : Actions)
			{
				ImGui::TableNextRow();
				ImGui::PushID(rowID);
				RenderActionRow(Action);
				ImGui::PopID(); // Need this to get buttons working
				rowID++;
			}

			ImGui::EndTable();
		}
	}

	if (bDisplayPropertyInspector && InspectedAction.IsValid())
	{
		FString ActionName = InspectedAction->GetActionData()->GetName();
		ActionName.RemoveFromStart(TEXT("Default__"));
		ActionName.RemoveFromEnd(TEXT("_c"));
		PropertyWindow->SetInspectedObject(InspectedAction.Get());
		if (ImGui::BeginChild(TCHAR_TO_ANSI(*ActionName)))
		{
			ImGui::PushID(0);
			PropertyWindow->RenderContent();
			ImGui::PopID();
			ImGui::EndChild();
		}
	}
}

void FGameplayWindow_Actions::RenderGameplayTags()
{
	FGameplayTagCountContainer OwnerTags, BlockedActionTags;
	OwnerTags = ACS->GrantedTags; BlockedActionTags = ACS->BlockedTags;

}

void FGameplayWindow_Actions::RenderActiveActions()
{
}

void FGameplayWindow_Actions::RenderActionRow(UGameplayAction* Action)
{
	ImGui::TableNextColumn(); // First column skip needed

	constexpr ImVec4 activeColor = ImVec4(0.f, 0.4f, 0.f, 1.f);
	constexpr ImVec4 blockedColor = ImVec4(0.4f, 0.f, 0.f, 1.f);
	constexpr ImVec4 canActivateColor = ImVec4(0.f, 0.f, 0.4f, 1.f);
	
	ImVec4 rowColor;
	
	if (!Action) return;

	if (ACS->TryActivateAbilityByClass(Action->GetActionData(), true))
	{
		rowColor = canActivateColor;
	}
	
	if (ImGui::Button("Activate"))
	{
		ACS->InternalTryActivateAction(Action);
	}

	ImGui::TableNextColumn();

	FString ActionName = Action->GetActionData()->GetName();
	ActionName.RemoveFromStart(TEXT("Default__"));
	ActionName.RemoveFromEnd(TEXT("_c"));
	ImGui::Text(TCHAR_TO_ANSI(*ActionName));

	if (ImGui::Button("Params"))
	{
		if (Action != InspectedAction) InspectedAction = Action;
		else bDisplayPropertyInspector = !bDisplayPropertyInspector;
		//InspectedAction = 
	}

	ImGui::TableNextColumn();

	const bool bTicking = ACS->ActionsAwaitingActivation.Contains(Action);
	if (bTicking)
	{
		ImGui::Text("Ticking");
	}

	ImGui::TableNextColumn();

	const bool bBlocked = ACS->AreActionTagsBlocked(Action->GetActionData()->ActionTags) || ACS->BlockedActions.Contains(Action->GetActionData());
	if (bBlocked || !Action->DoesSatisfyTagRequirements())
	{
		ImGui::Text("BLOCKED");
		rowColor = blockedColor;
	}

	ImGui::TableNextColumn();

	// Find cooldown condition
	UActionCondition_CoolDown* CoolDownCondition = nullptr;
	for (auto* condi : Action->ActionConditions)
	{
		CoolDownCondition = Cast<UActionCondition_CoolDown>(condi);
		if (CoolDownCondition)
		{
			break;
		}
	}
	if (CoolDownCondition && CoolDownCondition->bIsOnCooldown)
	{
		float RemainingTime, CooldownDuration;

		FTimerHandle CooldownTimer = CoolDownCondition->CooldownTimerHandle;
		RemainingTime = GetWorld()->GetTimerManager().GetTimerRemaining(CooldownTimer);
		CooldownDuration = CoolDownCondition->CoolDown;
		
		ImGui::PushStyleColor(ImGuiCol_PlotHistogram, IM_COL32(100, 100, 100, 255));
		ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(0, 0, 0, 100));
		ImGui::ProgressBar(RemainingTime / CooldownDuration, ImVec2(-1, ImGui::GetTextLineHeightWithSpacing() * 0.8f), TCHAR_TO_ANSI(*FString::Printf(TEXT("%.2f / %.2f"), RemainingTime, CooldownDuration)));
		ImGui::PopStyleColor(2);
	}

	ImGui::TableNextColumn();

	if (Action->IsActive())
	{
		ImGui::Text("%.1f", Action->GetTimeInAction());
		rowColor = activeColor;
	}
	
	ImU32 cellColor = ImGui::GetColorU32(rowColor);;
	ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg1, cellColor);
}
