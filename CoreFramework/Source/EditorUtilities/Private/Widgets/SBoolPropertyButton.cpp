// Copyright 2023 CoC All rights reserved


#include "Widgets/SBoolPropertyButton.h"

#include "SlateOptMacros.h"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

void SBoolPropertyButton::Construct(const FArguments& InArgs, TSharedPtr<IPropertyHandle> InPropertyHandle)
{
	PropertyHandle = InPropertyHandle;
	// We display this widget instead of the default, so notify it to not show the default
	InPropertyHandle->MarkHiddenByCustomization();
	PropertyHandle->GetValue(bCurrentVal);

	FLinearColor StartColor = bCurrentVal ? FLinearColor::Green : FLinearColor::Red;
	SetBorderBackgroundColor(StartColor);

	if (PropertyHandle->IsValidHandle())
	{
		auto TStyle = new FTextBlockStyle();
		TStyle->SetFont(FAppStyle::Get().GetFontStyle("SlateFileDialogs.Dialog"));
		TStyle->SetColorAndOpacity(FColor::White);
		TStyle->SetShadowOffset(FVector2D(1, 1));
		TStyle->Font.TypefaceFontName = FName("Bold");
		
		SButton::Construct(SButton::FArguments()
			.Text(InArgs._Text).TextStyle(TStyle).ForegroundColor(FColor::Black)
			.OnClicked_Lambda([this]()
		{
				bCurrentVal = !bCurrentVal;
				PropertyHandle->SetValue(bCurrentVal);
				if (bCurrentVal)
					SetBorderBackgroundColor(FLinearColor::Green);
				else
					SetBorderBackgroundColor(FLinearColor::Red);

			return FReply::Handled();	
		}));
	}
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION
