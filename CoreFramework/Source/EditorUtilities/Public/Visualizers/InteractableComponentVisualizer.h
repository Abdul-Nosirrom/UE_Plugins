// Copyright 2023 CoC All rights reserved

#pragma once

#include "CoreMinimal.h"
#include "ComponentVisualizer.h"

/**
 * 
 */
class EDITORUTILITIES_API FInteractableComponentVisualizer : public FComponentVisualizer
{
public:
	virtual void DrawVisualization(const UActorComponent* Component, const FSceneView* View, FPrimitiveDrawInterface* PDI) override;
};
