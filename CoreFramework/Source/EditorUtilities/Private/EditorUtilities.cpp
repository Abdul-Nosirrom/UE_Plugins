#include "EditorUtilities.h"

#include "AssetToolsModule.h"
#include "AssetCategories/SharedAssetFactory.h"
#include "Editor/ActionBlueprintFactory.h"

#define LOCTEXT_NAMESPACE "FEditorUtilitiesModule"

void FEditorUtilitiesModule::StartupModule()
{
	
	IAssetTools &AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	
	ActionAssetCategory = AssetTools.RegisterAdvancedAssetCategory(FName(TEXT("GameplayActions")), LOCTEXT("Gameplay Actions", "Action System"));
	{
		TSharedRef<IAssetTypeActions> ACT_GameplayAction = MakeShareable(new FAssetTypeActions_GameplayActionBlueprint);
		AssetTools.RegisterAssetTypeActions(ACT_GameplayAction);

		// One more for creating ActionData Blueprints
		TSharedRef<IAssetTypeActions> ACT_GameplayActionDataDef = MakeShareable(new FAssetTypeActions_ActionDataBlueprint);
		AssetTools.RegisterAssetTypeActions(ACT_GameplayActionDataDef);

		TSharedRef<IAssetTypeActions> ACT_GameplayActionData = MakeShareable(new FATA_ActionData);
		AssetTools.RegisterAssetTypeActions(ACT_GameplayActionData);

	}

	//ActionDataAssetCategory = AssetTools.RegisterAdvancedAssetCategory(FName(TEXT("ActionData")), LOCTEXT("Action Data", "Action Data"));
	//{
	//	TSharedRef<IAssetTypeActions> ACT_GameplayActionData = MakeShareable(new FATA_ActionData);
	//	AssetTools.RegisterAssetTypeActions(ACT_GameplayActionData);
	//}
	
	InputBufferAssetCategory = AssetTools.RegisterAdvancedAssetCategory(FName(TEXT("InputBuffer")), LOCTEXT("Input Buffer", "Input Buffer"));
	{
		TSharedRef<IAssetTypeActions> ACT_InputBufferMap = MakeShareable(new FATA_InputBufferMap);
		TSharedRef<IAssetTypeActions> ACT_DirectionalMap = MakeShareable(new FATA_MotionMap);
		TSharedRef<IAssetTypeActions> ACT_DirectionalAction = MakeShareable(new FATA_MotionAction);

		AssetTools.RegisterAssetTypeActions(ACT_InputBufferMap);
		AssetTools.RegisterAssetTypeActions(ACT_DirectionalMap);
		AssetTools.RegisterAssetTypeActions(ACT_DirectionalAction);
	}
}

void FEditorUtilitiesModule::ShutdownModule()
{
    
}

#undef LOCTEXT_NAMESPACE
    
IMPLEMENT_MODULE(FEditorUtilitiesModule, EditorUtilities)