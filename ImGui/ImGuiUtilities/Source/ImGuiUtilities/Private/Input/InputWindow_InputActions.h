// Copyright 2023 CoC All rights reserved

#pragma once

#include "CoreMinimal.h"
#include "ImGuiWindow.h"
#include "ImGuiWindowManager.h"
#include "InputWindow_InputActions.generated.h"

class UInputAction;
class UEnhancedInputLocalPlayerSubsystem;

struct FDebugInjectActionInfo
{
	const UInputAction* Action = nullptr;

	bool bPressed = false;

	bool bRepeat = false;

	float X = 0.0f;

	float Y = 0.0f;

	float Z = 0.0f;

	bool bIsMouseDownOnStick = false;

	bool bIsMouseDraggingStick = false;

	void Reset()
	{
		bPressed = false;
		bRepeat = false;
		X = 0.0f;
		Y = 0.0f;
		Z = 0.0f;
		bIsMouseDownOnStick = false;
		bIsMouseDraggingStick = false;
	}

	void Inject(UEnhancedInputLocalPlayerSubsystem& EnhancedInput, bool IsTimeToRepeat);
};

//--------------------------------------------------------------------------------------------------------------------------
UCLASS(Config = Cog)
class UImGuiInputConfig_Actions : public UImGuiWindowConfig
{
	GENERATED_BODY()

public:

	UPROPERTY(Config)
	float RepeatPeriod = 0.5f;

	virtual void Reset() override
	{
		Super::Reset();

		RepeatPeriod = 0.5f;
	}
};

class FInputWindow_InputActions : public FImGuiWindow
{
public:
	virtual void Initialize() override;

protected:

	virtual void RenderHelp() override;
	virtual void Tick(float DeltaTime) override;
	virtual void RenderContent() override;
	virtual void DrawAxis(const char* Format, const char* ActionName, float CurrentValue, float& InjectValue);

private:
	float RepeatTime = 0.0f;
	TArray<FDebugInjectActionInfo> Actions;
	TObjectPtr<UImGuiInputConfig_Actions> Config = nullptr;
	TObjectPtr<const class UInputMappingContext> InputMap;
};
