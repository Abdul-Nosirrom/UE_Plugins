// 


#include "AssetCategories/AssetEditorUtilities.h"


#include "K2Node_Event.h"

#include "AssetCategories/SharedAssetFactory.h"

#include "Kismet2/BlueprintEditorUtils.h"

#include "Kismet2/KismetEditorUtilities.h"

#include "Kismet2/SClassPickerDialog.h"

bool FAssetEditorUtilities::PickChildrenOfClass(const FText& TitleText, UClass*& OutChosenClass, UClass* Class)
{
	// Create filter

	TSharedPtr<FActionsClassViewerFilter> Filter = MakeShareable(new FActionsClassViewerFilter);

	Filter->AllowedChildrenOfClasses.Add(Class);

	// Fill in options

	FClassViewerInitializationOptions Options;

	Options.Mode = EClassViewerMode::ClassPicker;

	Options.ClassFilter = Filter;

	Options.bShowUnloadedBlueprints = true;

	Options.bExpandRootNodes = true;

	Options.NameTypeToDisplay = EClassViewerNameTypeToDisplay::Dynamic;

	return SClassPickerDialog::PickClass(TitleText, Options, OutChosenClass, Class);

}