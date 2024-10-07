// Copyright 2023 CoC All rights reserved
#include "InputWindow_InputActions.h"
#include "EnhancedInputSubsystems.h"
#include "ImGuiWidgets.h"
#include "InputMappingContext.h"
#include "StaticLibraries/InputBufferLibrary.h"
#include "UserSettings/EnhancedInputUserSettings.h"

//--------------------------------------------------------------------------------------------------------------------------

void FDebugInjectActionInfo::Inject(UEnhancedInputLocalPlayerSubsystem& EnhancedInput, bool IsTimeToRepeat)
{
	if (Action == nullptr)
	{
		return;
	}

	switch (Action->ValueType)
	{
	case EInputActionValueType::Boolean:
	{
		if (bRepeat)
		{
			EnhancedInput.InjectInputForAction(Action, FInputActionValue(IsTimeToRepeat), {}, {});
		}
		else
		{
			EnhancedInput.InjectInputForAction(Action, FInputActionValue(bPressed), {}, {});
		}
		break;
	}

	case EInputActionValueType::Axis1D:
	{
		EnhancedInput.InjectInputForAction(Action, FInputActionValue(X), {}, {});
		break;
	}

	case EInputActionValueType::Axis2D:
	{
		EnhancedInput.InjectInputForAction(Action, FInputActionValue(FVector2D(X, Y)), {}, {});
		break;
	}

	case EInputActionValueType::Axis3D:
	{
		EnhancedInput.InjectInputForAction(Action, FInputActionValue(FVector(X, Y, Z)), {}, {});
		break;
	}

	}
}

//--------------------------------------------------------------------------------------------------------------------------

void FInputWindow_InputActions::Initialize()
{
	FImGuiWindow::Initialize();

	bHasMenu = true;
	Config = GetConfig<UImGuiInputConfig_Actions>();
	InputMap = GetAsset<UInputMappingContext>();
}

void FInputWindow_InputActions::RenderHelp()
{
	ImGui::Text(
	"This window displays the current state of each Input Action. "
	"It can also be used to inject inputs to help debugging. ");
}

void FInputWindow_InputActions::Tick(float DeltaTime)
{
	if (GetWorld() == nullptr)
	{
		return;
	}

	ULocalPlayer* LocalPlayer = GetWorld()->GetFirstLocalPlayerFromController();
	if (LocalPlayer == nullptr)
	{
		return;
	}

	UEnhancedInputLocalPlayerSubsystem* EnhancedInput = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LocalPlayer);
	if (EnhancedInput == nullptr)
	{
		return;
	}

	bool IsTimeToRepeat = false;
	float WorldTime = GetWorld()->GetTimeSeconds();
	if (RepeatTime < WorldTime)
	{
		RepeatTime = WorldTime + Config->RepeatPeriod;
		IsTimeToRepeat = true;
	}

	for (FDebugInjectActionInfo& ActionInfo : Actions)
	{
		ActionInfo.Inject(*EnhancedInput, IsTimeToRepeat);
	}
}

void FInputWindow_InputActions::RenderContent()
{
    if (InputMap == nullptr)
    {
        ImGui::TextDisabled("Invalid Asset");
        return;
    }

    ULocalPlayer* LocalPlayer = GetWorld()->GetFirstLocalPlayerFromController();
    if (LocalPlayer == nullptr)
    {
        ImGui::TextDisabled("No Local Player");
        return;
    }

    UEnhancedInputLocalPlayerSubsystem* EnhancedInputSubsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LocalPlayer);
    if (EnhancedInputSubsystem == nullptr)
    {
        ImGui::TextDisabled("No Enhanced Input Subsystem");
        return;
    }

    if(EnhancedInputSubsystem->GetPlayerInput() == nullptr)
    {
        ImGui::TextDisabled("No Player Input");
        return;
    }
    
    if (ImGui::BeginMenuBar())
    {
        if (ImGui::BeginMenu("Options"))
        {
            ImGui::SliderFloat("##Repeat", &Config->RepeatPeriod, 0.1f, 10.0f, "Repeat: %0.1fs");
            ImGui::EndMenu();
        }

        if (ImGui::MenuItem("Reset"))
        {
            for (FDebugInjectActionInfo& ActionInfo : Actions)
            {
                ActionInfo.Reset();
            }
        }

        ImGui::EndMenuBar();
    }

    if (Actions.Num() == 0)
    {
    	for (const FEnhancedActionKeyMapping& Mapping : InputMap->GetMappings())
    	{
    		if (Mapping.Action != nullptr && Actions.ContainsByPredicate([&Mapping](const FDebugInjectActionInfo& ActionInfo) { return Mapping.Action == ActionInfo.Action; }) == false)
    		{
    			FDebugInjectActionInfo& ActionInfo = Actions.AddDefaulted_GetRef();
    			ActionInfo.Action = Mapping.Action;
    		}
    	}

        Actions.Sort([](const FDebugInjectActionInfo& Lhs, const FDebugInjectActionInfo& Rhs)
        {
            return GetNameSafe(Lhs.Action).Compare(GetNameSafe(Rhs.Action)) < 0;
        });
    }

	// BUG: Resizable flag makes it so the input focus on the ImGui doesnt work for some reason. Input only handles resizing but cant click any of the buttons and stuff.
    if (ImGui::BeginTable("Actions", 3, ImGuiTableFlags_SizingFixedFit /*| ImGuiTableFlags_Resizable*/ | ImGuiTableFlags_NoBordersInBodyUntilResize))
    {
        ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Inject", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();

        int Index = 0;

        for (FDebugInjectActionInfo& ActionInfo : Actions)
        {
            ImGui::PushID(Index);

            const auto ActionName = StringCast<ANSICHAR>(*ActionInfo.Action->GetName());

            FInputActionValue ActionValue = EnhancedInputSubsystem->GetPlayerInput()->GetActionValue(ActionInfo.Action);

            switch (ActionInfo.Action->ValueType)
            {
                case EInputActionValueType::Boolean: 
                {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::Text("%s", ActionName.Get());

                    const FVector4 ActiveColor(1, 0.8f, 0, 1);
                    const FVector4 InnactiveColor(0.3f, 0.3f, 0.3f, 1);
                    const FVector2D ButtonSize(UImGuiHelperWidgets::GetFontWidth() * 10, 0);

                    ImGui::TableNextColumn();
                    ImGui::BeginDisabled();
                    bool Value = ActionValue.Get<bool>();
                    UImGuiHelperWidgets::ToggleButton(Value, "Pressed##Value", "Released##Value", ActiveColor, InnactiveColor, ButtonSize);
                    ImGui::EndDisabled();

                    ImGui::TableNextColumn();
                    UImGuiHelperWidgets::ToggleButton(ActionInfo.bPressed, "Pressed##Inject", "Released##Inject", ActiveColor, InnactiveColor, ButtonSize);
                    ImGui::SameLine();
                    UImGuiHelperWidgets::ToggleButton(ActionInfo.bRepeat, "Repeat", "Repeat", ActiveColor, InnactiveColor, ButtonSize);
                    break;
                }

                case EInputActionValueType::Axis1D: 
                {
                    const float Value = ActionValue.Get<float>();
                    DrawAxis("%s", ActionName.Get(), Value, ActionInfo.X);
                    break;
                }

                case EInputActionValueType::Axis2D:
                {
                    const FVector2D Value = ActionValue.Get<FVector2D>();
                    DrawAxis("%s X", ActionName.Get(), Value.X, ActionInfo.X);
                    DrawAxis("%s Y", ActionName.Get(), Value.Y, ActionInfo.Y);
                    break;
                }

                case EInputActionValueType::Axis3D:
                {
                    const FVector Value = ActionValue.Get<FVector>();
                    DrawAxis("%s X", ActionName.Get(), Value.X, ActionInfo.X);
                    DrawAxis("%s Y", ActionName.Get(), Value.Y, ActionInfo.Y);
                    DrawAxis("%s Z", ActionName.Get(), Value.Z, ActionInfo.Z);
                    break;
                }
            }

            ImGui::PopID();
            Index++;
        }
        ImGui::EndTable();
    }
}

void FInputWindow_InputActions::DrawAxis(const char* Format, const char* ActionName, float CurrentValue, float& InjectValue)
{
	ImGui::PushID(Format);
	ImGui::TableNextRow();

	ImGui::TableNextColumn();
	ImGui::Text(Format, ActionName);

	ImGui::TableNextColumn();
	ImGui::SetNextItemWidth(-1);
	ImGui::BeginDisabled();
	ImGui::SliderFloat("##Value", &CurrentValue, -1.0f, 1.0f, "%0.2f");
	ImGui::EndDisabled();

	ImGui::TableNextColumn();
	ImGui::SetNextItemWidth(-1);
	UImGuiHelperWidgets::SliderWithReset("##Inject", InjectValue, -1.0f, 1.0f, 0.0f, "%0.2f");
	ImGui::PopID();
}