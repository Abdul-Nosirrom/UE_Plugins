// Copyright 2023 CoC All rights reserved

#pragma once

#include "CoreMinimal.h"
#include "ClassViewerFilter.h"
#include "ClassViewerModule.h"
#include "Widgets/SCompoundWidget.h"

/**
 * 
 */
class EDITORUTILITIES_API SBlueprintClassCreateDialog : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SBlueprintClassCreateDialog) {}
		SLATE_ATTRIBUTE(UClass*, BaseClass)
	SLATE_END_ARGS()

	/// @brief	Constructs this widget with InArgs
	void Construct(const FArguments& InArgs);

	/// @brief	Sets properties for the supplied blueprint factory
	bool ConfigureProperties(TWeakObjectPtr<class UBlueprintFactory> InBlueprintFactor);

private:

	/// @brief	Creates the combo menu for the parent class */
	void MakeParentClassPicker();
	
	/// @brief	Handler for when a parent class is selected */
	void OnClassPicked(UClass* ChosenClass);

	/// @brief	Handler for when ok is clicked */
	FReply OkClicked();

	void CloseDialog(bool bWasPicked = false);

	/// @brief	Handler for when cancel is clicked */
	FReply CancelClicked();

	FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent);

private:

	/// @brief	The factory for which we are setting up properties 
	TWeakObjectPtr<UBlueprintFactory> BlueprintFactory;

	/// @brief	A pointer to the window that is asking the user to select a parent class 
	TWeakPtr<SWindow> PickerWindow;

	/// @brief	The container for the Parent Class picker 
	TSharedPtr<SVerticalBox> ParentClassContainer;

	/// @brief	The selected class 
	TWeakObjectPtr<UClass> ParentClass;

	/// @brief	Cached args
	FArguments CachedArgs;

	/// @brief	True if Ok was clicked 
	bool bOkClicked;

	class FBlueprintParentFilter : public IClassViewerFilter
	{
	public:
		/** All children of these classes will be included unless filtered out by another setting. */
		TSet< const UClass* > AllowedChildrenOfClasses;

		FBlueprintParentFilter() {}

		virtual bool IsClassAllowed(const FClassViewerInitializationOptions& InInitOptions, const UClass* InClass, TSharedRef< FClassViewerFilterFuncs > InFilterFuncs) override
		{
			// If it appears on the allowed child-of classes list (or there is nothing on that list)
			return InFilterFuncs->IfInChildOfClassesSet(AllowedChildrenOfClasses, InClass) != EFilterReturn::Failed;
		}

		virtual bool IsUnloadedClassAllowed(const FClassViewerInitializationOptions& InInitOptions, const TSharedRef< const IUnloadedBlueprintData > InUnloadedClassData, TSharedRef< FClassViewerFilterFuncs > InFilterFuncs) override
		{
			// If it appears on the allowed child-of classes list (or there is nothing on that list)
			return InFilterFuncs->IfInChildOfClassesSet(AllowedChildrenOfClasses, InUnloadedClassData) != EFilterReturn::Failed;
		}
	};
};
