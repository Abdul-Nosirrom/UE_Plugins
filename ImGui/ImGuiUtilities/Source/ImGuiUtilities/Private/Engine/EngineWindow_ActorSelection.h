// Copyright 2023 CoC All rights reserved

#pragma once

#include "CoreMinimal.h"
#include "ImGuiWindow.h"
#include "ImGuiWindowManager.h"
#include "UObject/Object.h"
#include "EngineWindow_ActorSelection.generated.h"

//--------------------------------------------------------------------------------------------------------------------------
class FEngineWindow_ActorSelection : public FImGuiWindow
{
    
public:

    static FString ToggleSelectionModeCommand;

    virtual void Initialize() override;
    virtual void Shutdown() override;

protected:
    
    virtual void Tick(float DeltaTime) override;
    
    virtual void RenderHelp() override;
    virtual void RenderContent() override;
    virtual float GetMainMenuWidgetWidth(int32 SubWidgetIndex, float MaxWidth) override;
    virtual void RenderMainMenuWidget(int32 SubWidgetIndex, float Width) override;
    virtual bool DrawSelectionCombo();

    virtual void RenderPickButtonTooltip();
    virtual void RenderActorContextMenu(AActor* Actor);

    void TickSelectionMode();
    
    virtual void SetGlobalSelection(AActor* Value) const;
    TSubclassOf<AActor> GetSelectedActorClass() const;

public:
    
    bool GetIsSelecting() const { return bSelectionModeActive; }
    const TArray<TSubclassOf<AActor>>& GetActorClasses() const { return ActorClasses; }
    void SetActorClasses(const TArray<TSubclassOf<AActor>>& Value) { ActorClasses = Value; }
    ETraceTypeQuery GetTraceType() const { return TraceType; }
    void SetTraceType(ETraceTypeQuery Value) { TraceType = Value; }

protected:
    
    virtual void ActivateSelectionMode();
    virtual void DeactivateSelectionMode();
    virtual void ToggleSelectionMode();
    virtual void TryReapplySelection() const;

    virtual void HackWaitInputRelease();
    
    FVector LastSelectedActorLocation = FVector::ZeroVector;

    bool bSelectionModeActive = false;

    bool bIsInputEnabledBeforeEnteringSelectionMode = false;

    int32 WaitInputReleased = 0;

    TArray<TSubclassOf<AActor>> ActorClasses;

    ETraceTypeQuery TraceType = TraceTypeQuery1;

    TArray<IConsoleObject*> ConsoleCommands;

    TObjectPtr<class UImGuiEngineConfig_Selection> Config;

	ImGuiTextFilter Filter;
};

//--------------------------------------------------------------------------------------------------------------------------
UCLASS(Config = ImGui)
class UImGuiEngineConfig_Selection : public UImGuiWindowConfig
{
    GENERATED_BODY()

public:

    UPROPERTY(Config)
    bool bReapplySelection = true;

    UPROPERTY(Config)
    FString SelectionName;

    UPROPERTY(Config)
    int32 SelectedClassIndex = 0;

    virtual void Reset() override
    {
        Super::Reset();

        bReapplySelection = true;
        SelectionName.Reset();
        SelectedClassIndex = 0;
    }
};