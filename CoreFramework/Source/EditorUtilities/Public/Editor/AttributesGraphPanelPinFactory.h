// Copyright 2023 CoC All rights reserved

#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "AttributeSystem/EntityAttributeSet.h"
#include "EdGraphUtilities.h"
#include "EdGraphSchema_K2.h"
#include "SGraphPin.h"
#include "Widgets/SEntityAttributeGraphPin.h"

class FAttributesGraphPanelPinFactory: public FGraphPanelPinFactory
{
	virtual TSharedPtr<class SGraphPin> CreatePin(class UEdGraphPin* InPin) const override
	{
		if (InPin->PinType.PinCategory == UEdGraphSchema_K2::PC_Struct && InPin->PinType.PinSubCategoryObject == FEntityAttribute::StaticStruct())
		{
			return SNew(SEntityAttributeGraphPin, InPin);
		}
		return NULL;
	}
};
