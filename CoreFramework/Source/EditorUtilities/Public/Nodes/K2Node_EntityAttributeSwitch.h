﻿// Copyright 2023 CoC All rights reserved

#pragma once

#include "CoreMinimal.h"
#include "K2Node_Switch.h"
#include "AttributeSystem/EntityAttributeSet.h"
#include "K2Node_EntityAttributeSwitch.generated.h"


UCLASS(MinimalAPI)
class UK2Node_EntityAttributeSwitch : public UK2Node_Switch
{
	GENERATED_UCLASS_BODY()
	
	UPROPERTY(EditAnywhere, Category = PinOptions)
	TArray<FEntityAttribute> PinAttributes;

	UPROPERTY()
	TArray<FName> PinNames;

	// UObject interface
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostLoad() override;
	// End of UObject interface

	// UEdGraphNode interface
	virtual FText GetTooltipText() const override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual bool ShouldShowNodeProperties() const override { return true; }
	// End of UEdGraphNode interface

	// UK2Node interface
	virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
	// End of UK2Node interface

	// UK2Node_Switch Interface
	EDITORUTILITIES_API virtual void AddPinToSwitchNode() override;
	virtual FName GetUniquePinName() override;
	virtual FEdGraphPinType GetPinType() const override;
	EDITORUTILITIES_API virtual FEdGraphPinType GetInnerCaseType() const override;
	// End of UK2Node_Switch Interface

	virtual FName GetPinNameGivenIndex(int32 Index) const override;

protected:
	virtual void CreateFunctionPin() override;
	virtual void CreateSelectionPin() override;
	virtual void CreateCasePins() override;
	virtual void RemovePin(UEdGraphPin* TargetPin) override;
};
