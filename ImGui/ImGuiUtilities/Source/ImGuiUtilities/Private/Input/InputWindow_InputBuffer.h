// Copyright 2023 CoC All rights reserved

#pragma once
#include "CoreMinimal.h"
#include "ImGuiWindow.h"
#include "GameplayTags.h" // due to pch stuff with the way input buffer includes gameplay tags, we have to include this here  otherwise we get annoying ass compile errors

class FInputWindow_InputBuffer : public FImGuiWindow
{
public:
	virtual void RenderContent() override;
};
