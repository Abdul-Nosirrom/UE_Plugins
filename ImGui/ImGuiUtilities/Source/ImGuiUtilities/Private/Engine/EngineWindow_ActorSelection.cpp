// Copyright 2023 CoC All rights reserved


#include "EngineWindow_ActorSelection.h"

#include "RadicalCharacter.h"
#include "EngineUtils.h"
#include "ImGuiDebug.h"
#include "ImGuiHelper.h"
#include "ImGuiInputHelper.h"
#include "ImGuiWidgets.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"

FString FEngineWindow_ActorSelection::ToggleSelectionModeCommand = TEXT("ImGui.ToggleSelectionMode");


void FEngineWindow_ActorSelection::Initialize()
{
	FImGuiWindow::Initialize();

	bHasMenu = true;
	bHasWidget = true;
	SetIsWidgetVisible(true);
	
	ActorClasses = { AActor::StaticClass(), ARadicalCharacter::StaticClass() };
	TraceType = UEngineTypes::ConvertToTraceType(ECC_WorldStatic);
	
	Config = GetConfig<UImGuiEngineConfig_Selection>();

	TryReapplySelection();

	if (ConsoleCommands.Num() != 0) return; // HACK: Issues with crashes during shutdown where we unregister the console command, so we avoid that by only registering it once the first time to avoid needing to unregister it....
	
	ConsoleCommands.Add(IConsoleManager::Get().RegisterConsoleCommand(
		*ToggleSelectionModeCommand,
		TEXT("Toggle the actor selection mode"),
		FConsoleCommandWithArgsDelegate::CreateLambda([this](const TArray<FString>& Args) { ToggleSelectionMode(); }),
		ECVF_Cheat));
}

void FEngineWindow_ActorSelection::Shutdown()
{
	for (IConsoleObject* ConsoleCommand : ConsoleCommands)
	{
		//IConsoleManager::Get().UnregisterConsoleObject(ConsoleCommand);
	}
}

void FEngineWindow_ActorSelection::Tick(float DeltaTime)
{
	if (FImGuiDebug::GetSelection() == nullptr)
	{
		SetGlobalSelection(GetLocalPlayerPawn());
	}

	if (bSelectionModeActive)
	{
		TickSelectionMode();
	}

	if (const AActor* Actor = GetSelection())
	{
		if (Actor != GetLocalPlayerPawn())
		{
			UImGuiHelperWidgets::ActorFrame(Actor);
		}
	}
}

/*--------------------------------------------------------------------------------------------------------------
* RENDER HANDLING
*--------------------------------------------------------------------------------------------------------------*/

void FEngineWindow_ActorSelection::RenderHelp()
{
	ImGui::Text(
	"This window can be used to select an actor either by picking an actor in the world, "
	"or by selecting an actor in the actor list. "
	"The actor list can be filtered by actor type (Actor, Character, etc). "
	"The current selection is used by various debug windows to filter out their content"
	);
}

void FEngineWindow_ActorSelection::RenderContent()
{
	if (ImGui::BeginMenuBar())
	{
		if (ImGui::MenuItem("Pick"))
		{
			ActivateSelectionMode();
			//HackWaitInputRelease();
		}

		if (ImGui::BeginMenu("Options"))
		{
			ImGui::Checkbox("Save selection", &Config->bReapplySelection);
			ImGui::Checkbox("Actor Name Use Label", &FImGuiDebug::Settings.ActorNameUseLabel);
			ImGui::EndMenu();
		}

		RenderPickButtonTooltip();

		ImGui::EndMenuBar();
	}

	DrawSelectionCombo();
}

float FEngineWindow_ActorSelection::GetMainMenuWidgetWidth(int32 SubWidgetIndex, float MaxWidth)
{
	switch (SubWidgetIndex)
	{
	case 0: return UImGuiHelperWidgets::GetFontWidth() * 6;
	case 1: return FMath::Min(FMath::Max(MaxWidth, UImGuiHelperWidgets::GetFontWidth() * 10), UImGuiHelperWidgets::GetFontWidth() * 30);
	case 2: return UImGuiHelperWidgets::GetFontWidth() * 3;
	}

	return -1.0f;
}

void FEngineWindow_ActorSelection::RenderMainMenuWidget(int32 SubWidgetIndex, float Width)
{
	//-----------------------------------
	// Pick Button
	//-----------------------------------
	if (SubWidgetIndex == 0)
	{
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
		ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
		ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(0, 0, 0, 0));

		if (ImGui::Button("Pick", ImVec2(Width, 0)))
		{
			ActivateSelectionMode();
			HackWaitInputRelease();
		}
		RenderPickButtonTooltip();

		ImGui::PopStyleColor(1);
		ImGui::PopStyleVar(2);
	}
	else if (SubWidgetIndex == 1)
	{
		ImGui::SetNextItemWidth(Width);
		UImGuiHelperWidgets::MenuActorsCombo("MenuActorSelection", GetWorld(), ActorClasses, Config->SelectedClassIndex, &Filter, GetLocalPlayerPawn(), [this](AActor* Actor) { RenderActorContextMenu(Actor);  });
	}
	else if (SubWidgetIndex == 2)
	{
		//-----------------------------------
		// Reset Button
		//-----------------------------------
		{
			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
			ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(0, 0, 0, 0));
			if (ImGui::Button("X", ImVec2(Width, 0)))
			{
				SetGlobalSelection(nullptr);
				ImGui::CloseCurrentPopup();
			}
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("Reset the selection to the controlled actor.");
			}
			ImGui::PopStyleColor(1);
			ImGui::PopStyleVar(1);
		}
	}
}

bool FEngineWindow_ActorSelection::DrawSelectionCombo()
{
	return UImGuiHelperWidgets::ActorsListWithFilters(GetWorld(), ActorClasses, Config->SelectedClassIndex, &Filter, GetLocalPlayerPawn());
}

void FEngineWindow_ActorSelection::RenderPickButtonTooltip()
{
	if (ImGui::IsItemHovered(ImGuiHoveredFlags_Stationary))
	{
		const FString Shortcut = FImGuiInputHelper::CommandToString(*GetWorld(), ToggleSelectionModeCommand);
		ImGui::SetTooltip("Enter picking mode to pick an actor on screen. %s", TCHAR_TO_ANSI(*Shortcut));
	}
}

void FEngineWindow_ActorSelection::RenderActorContextMenu(AActor* Actor)
{
	//FImGuiEngineHelper::ActorContextMenu(Actor);
}

/*--------------------------------------------------------------------------------------------------------------
* SELECTION HANDLING
*--------------------------------------------------------------------------------------------------------------*/

void FEngineWindow_ActorSelection::TickSelectionMode()
{
	if (ImGui::IsMouseClicked(ImGuiMouseButton_Right))
    {
        DeactivateSelectionMode();
        return;
    }

    APlayerController* PlayerController = GetLocalPlayerController();
    if (PlayerController == nullptr)
    {
        DeactivateSelectionMode();
        return;
    }

    ImGuiViewport* Viewport = ImGui::GetMainViewport();
    if (Viewport == nullptr)
    {
        return;
    }

    const ImVec2 ViewportPos = Viewport->Pos;
    const ImVec2 ViewportSize = Viewport->Size;
    ImDrawList* DrawList = ImGui::GetBackgroundDrawList(Viewport);
    DrawList->AddRect(ViewportPos, ViewportPos + ViewportSize, IM_COL32(255, 0, 0, 128), 0.0f, 0, 20.0f);
    UImGuiHelperWidgets::AddTextWithShadow(DrawList, FImGuiHelper::ToFVector2D(ViewportPos + ImVec2(20, 20)), IM_COL32(255, 255, 255, 255), "Picking Mode. \n[LMB] Pick \n[RMB] Cancel");

    TSubclassOf<AActor> SelectedActorClass = GetSelectedActorClass();

    AActor* HoveredActor = nullptr;
    FVector WorldOrigin, WorldDirection;
    if (UGameplayStatics::DeprojectScreenToWorld(PlayerController, FImGuiHelper::ToFVector2D(ImGui::GetMousePos() - ViewportPos), WorldOrigin, WorldDirection))
    {
        //--------------------------------------------------------------------------------------------------------
        // Prioritize another actor than the selected actor unless we only touch the selected actor.
        //--------------------------------------------------------------------------------------------------------
        TArray<AActor*> IgnoreList;
        IgnoreList.Add(FImGuiDebug::GetSelection());

        FHitResult HitResult;
        for (int i = 0; i < 2; ++i)
        {
            if (UKismetSystemLibrary::LineTraceSingle(GetWorld(), WorldOrigin, WorldOrigin + WorldDirection * 10000, TraceType, false, IgnoreList, EDrawDebugTrace::None, HitResult, true))
            {
                if (SelectedActorClass == nullptr || HitResult.GetActor()->GetClass()->IsChildOf(SelectedActorClass))
                {
                    HoveredActor = HitResult.GetActor();
                    break;
                }

                //------------------------------------------------
                // The second time we accept the selected actor
                //------------------------------------------------
                IgnoreList.Empty();
            }
        }
    }

    if (HoveredActor)
    {
        UImGuiHelperWidgets::ActorFrame(HoveredActor);
    }

    if (bSelectionModeActive)
    {
        if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
        {
            if (WaitInputReleased == 0)
            {
                if (HoveredActor != nullptr)
                {
                    SetGlobalSelection(HoveredActor);
                }

                DeactivateSelectionMode();
            }
            else
            {
                WaitInputReleased--;
            }
        }
    }
}

void FEngineWindow_ActorSelection::SetGlobalSelection(AActor* Value) const
{
	FImGuiDebug::SetSelection(Value);
}

TSubclassOf<AActor> FEngineWindow_ActorSelection::GetSelectedActorClass() const
{
	TSubclassOf<AActor> SelectedClass = AActor::StaticClass();
	if (ActorClasses.IsValidIndex(Config->SelectedClassIndex))
	{
		SelectedClass = ActorClasses[Config->SelectedClassIndex];
	}

	return SelectedClass;
}

void FEngineWindow_ActorSelection::ActivateSelectionMode()
{
	bSelectionModeActive = true;
	bIsInputEnabledBeforeEnteringSelectionMode = GetOwner()->GetContext().GetEnableInput();
	GetOwner()->GetContext().SetEnableInput(true);
	GetOwner()->SetHideAllWindows(true);
}

void FEngineWindow_ActorSelection::DeactivateSelectionMode()
{
	bSelectionModeActive = false;

	//--------------------------------------------------------------------------------------------
	// We can enter selection mode by a command, and ImGui might not have the input focus
	// When in selection mode we need ImGui to have the input focus
	// When leaving selection mode we want to leave it as it was before
	//--------------------------------------------------------------------------------------------
	GetOwner()->GetContext().SetEnableInput(bIsInputEnabledBeforeEnteringSelectionMode);

	GetOwner()->SetHideAllWindows(false);
}

void FEngineWindow_ActorSelection::ToggleSelectionMode()
{
	if (bSelectionModeActive)
	{
		DeactivateSelectionMode();
	}
	else
	{
		ActivateSelectionMode();
	}
}

void FEngineWindow_ActorSelection::TryReapplySelection() const
{
	const UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	if (Config->bReapplySelection == false)
	{
		return;
	}

	if (Config->SelectionName.IsEmpty())
	{
		return;
	}

	const TSubclassOf<AActor> SelectedClass = GetSelectedActorClass();
	if (SelectedClass == nullptr)
	{
		return;
	}

	TArray<AActor*> Actors;
	for (TActorIterator It(World, SelectedClass); It; ++It)
	{
		AActor* Actor = *It;
		if (GetNameSafe(Actor) == Config->SelectionName)
		{
			SetGlobalSelection(Actor);
		}
	}
}

void FEngineWindow_ActorSelection::HackWaitInputRelease()
{
	WaitInputReleased = 1;
}
