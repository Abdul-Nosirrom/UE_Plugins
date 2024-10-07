// Copyright 2023 CoC All rights reserved

#pragma once

#include "CoreMinimal.h"
#include "ImGuiWindow.h"

/**
 * 
 */
class FGameplayWindow_Actions : public FImGuiWindow
{
public:
	virtual void Initialize() override;
	virtual void RenderContent() override;

protected:
	void RenderGameplayTags();
	void RenderActiveActions();
	void RenderActionRow(class UGameplayAction* Action);

	class UActionSystemComponent* ACS = nullptr;

	bool bDisplayPropertyInspector = false;
	TWeakObjectPtr<UGameplayAction> InspectedAction = nullptr;
	class FEngineWindow_PropertyInspector* PropertyWindow = nullptr;
};
