#include "EditorUtilities.h"

#include "AssetToolsModule.h"
#include "EdGraphUtilities.h"
#include "AssetCategories/SharedAssetFactory.h"
#include "Components/DialogueInteractableComponent.h"
#include "Details/ActionInputReqDetails.h"
#include "Details/ActionScriptEventDetails.h"
#include "Details/ActionSetEntryDetails.h"
#include "Details/AttackDataDetails.h"
#include "Details/EntityAttributeDetails.h"
#include "Details/InheritableTagContainerDetails.h"
#include "Editor/ActionBlueprintFactory.h"
#include "Editor/AttributesGraphPanelPinFactory.h"
#include "Interfaces/IPluginManager.h"
#include "Visualizers/DialogueComponentVisualizer.h"
#include "Visualizers/InteractableComponentVisualizer.h"

#define LOCTEXT_NAMESPACE "FEditorUtilitiesModule"


#define SLATE_IMAGE_BRUSH( ImagePath, ImageSize ) new FSlateImageBrush( StyleSetInstance->RootToContentDir( TEXT(ImagePath), TEXT(".png") ), FVector2D(ImageSize, ImageSize ) )

void FEditorUtilitiesModule::StartupModule()
{
	
	IAssetTools &AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	
	ActionAssetCategory = AssetTools.RegisterAdvancedAssetCategory(FName(TEXT("GameplayActions")), LOCTEXT("Gameplay Actions", "Action System"));
	{
		TSharedRef<IAssetTypeActions> ACT_GameplayAction = MakeShareable(new FAssetTypeActions_GameplayActionBlueprint);
		AssetTools.RegisterAssetTypeActions(ACT_GameplayAction);
		
		TSharedRef<IAssetTypeActions> ACT_GameplayActionData = MakeShareable(new FATA_ActionData);
		AssetTools.RegisterAssetTypeActions(ACT_GameplayActionData);
		
	}

	AttributesCategory = AssetTools.RegisterAdvancedAssetCategory(FName(TEXT("EntityAttributes")), LOCTEXT("Entity Attributes", "Attribute System"));
	{
		TSharedRef<IAssetTypeActions> ACT_EntityEffect = MakeShareable(new FAssetTypeActions_EntityEffectBlueprint);
		AssetTools.RegisterAssetTypeActions(ACT_EntityEffect);

		TSharedRef<IAssetTypeActions> ACT_AttributeInits = MakeShareable(new FATA_AttributeDataTable);
		AssetTools.RegisterAssetTypeActions(ACT_AttributeInits);
	}

	CombatCategory = AssetTools.RegisterAdvancedAssetCategory(FName(TEXT("CombatSystem")), LOCTEXT("Combat System", "Combat System"));
	{
		TSharedRef<IAssetTypeActions> ACT_TargetingPreset = MakeShareable(new FATA_TargetingPreset);
		AssetTools.RegisterAssetTypeActions(ACT_TargetingPreset);
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

	// Register visualizers
	if (GUnrealEd)
	{
		TSharedPtr<FDialogueComponentVisualizer> DlgVisualizer = MakeShareable(new FDialogueComponentVisualizer);
		TSharedPtr<FInteractableComponentVisualizer> InteractableVisualizer = MakeShareable(new FInteractableComponentVisualizer);

		if (DlgVisualizer.IsValid())
		{
			GUnrealEd->RegisterComponentVisualizer(UDialogueInteractableComponent::StaticClass()->GetFName(), DlgVisualizer);
		}
		if (InteractableVisualizer.IsValid())
		{
			GUnrealEd->RegisterComponentVisualizer(UInteractableComponent::StaticClass()->GetFName(), InteractableVisualizer);
		}
	}

	// Register Attribute System Property UI
	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	PropertyModule.RegisterCustomPropertyTypeLayout( "EntityAttribute", FOnGetPropertyTypeCustomizationInstance::CreateStatic( &FEntityAttributePropertyDetails::MakeInstance ) );
	PropertyModule.RegisterCustomPropertyTypeLayout("InheritedTagContainer", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FInheritableTagContainerDetails::MakeInstance));
	PropertyModule.RegisterCustomPropertyTypeLayout("ActionSetEntry", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FActionSetEntryDetails::MakeInstance));
	PropertyModule.RegisterCustomPropertyTypeLayout( "ActionEventWrapper", FOnGetPropertyTypeCustomizationInstance::CreateStatic( &FActionScriptEventDetails::MakeInstance ) );
	PropertyModule.RegisterCustomPropertyTypeLayout("ActionInputRequirement", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FActionInputReqDetails::MakeInstance));
	//PropertyModule.RegisterCustomPropertyTypeLayout("AttackData", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FAttackDataPropertyDetails::MakeInstance));
	
	// Register style sheet for our classes in the plugin
	{
		// Create the new style set
		StyleSetInstance = MakeShareable(new FSlateStyleSet("CoreFrameworkEditorStyle"));
		// Set root directory as resources folder
		static FString ContentDir = IPluginManager::Get().FindPlugin(TEXT("CoreFramework"))->GetBaseDir() / TEXT("Resources");
		StyleSetInstance->SetContentRoot(ContentDir);
		
		// Setup class Icons
		StyleSetInstance->Set("ClassIcon.GameplayAction", SLATE_IMAGE_BRUSH("GameplayActionIcon", 20.f));
		StyleSetInstance->Set("ClassThumbnail.GameplayAction", SLATE_IMAGE_BRUSH("GameplayActionIcon", 128.f));

		StyleSetInstance->Set("ClassIcon.GameplayActionData", SLATE_IMAGE_BRUSH("ActionDataIcon", 20.f));
		StyleSetInstance->Set("ClassThumbnail.GameplayActionData", SLATE_IMAGE_BRUSH("ActionDataIcon", 128.f));

		StyleSetInstance->Set("ClassIcon.RadicalMovementComponent", SLATE_IMAGE_BRUSH("MovementComponentIcon", 20.f));
		StyleSetInstance->Set("ClassThumbnail.RadicalMovementComponent", SLATE_IMAGE_BRUSH("MovementComponentIcon", 128.f));

		StyleSetInstance->Set("ClassIcon.ActionSystemComponent", SLATE_IMAGE_BRUSH("GameplayActionIcon", 20.f));
		StyleSetInstance->Set("ClassThumbnail.ActionSystemComponent", SLATE_IMAGE_BRUSH("GameplayActionIcon", 128.f));
		
		StyleSetInstance->Set("ClassIcon.AttributeSystemComponent", SLATE_IMAGE_BRUSH("EntityEffectIcon", 20.f));
		StyleSetInstance->Set("ClassThumbnail.AttributeSystemComponent", SLATE_IMAGE_BRUSH("EntityEffectIcon", 128.f));
		
		StyleSetInstance->Set("ClassIcon.DialogueAsset", SLATE_IMAGE_BRUSH("DialogueSMIcon", 20.f));
		StyleSetInstance->Set("ClassThumbnail.DialogueAsset", SLATE_IMAGE_BRUSH("DialogueSMIcon", 128.f));


		StyleSetInstance->Set("ClassIcon.InteractableComponent", SLATE_IMAGE_BRUSH("InteractableIcon", 20.f));
		StyleSetInstance->Set("ClassThumbnail.InteractableComponent", SLATE_IMAGE_BRUSH("InteractableIcon", 128.f));
		
		StyleSetInstance->Set("ClassIcon.EntityEffect", SLATE_IMAGE_BRUSH("EntityEffectIcon", 20.f));
		StyleSetInstance->Set("ClassThumbnail.EntityEffect", SLATE_IMAGE_BRUSH("EntityEffectIcon", 128.f));

		StyleSetInstance->Set("ClassIcon.AttributeInitDataTable", SLATE_IMAGE_BRUSH("AttributeInitIcon", 20.f));
		StyleSetInstance->Set("ClassThumbnail.AttributeInitDataTable", SLATE_IMAGE_BRUSH("AttributeInitIcon", 128.f));

		StyleSetInstance->Set("ClassIcon.TargetingPreset", SLATE_IMAGE_BRUSH("TargetingDataIcon", 20.f));
		StyleSetInstance->Set("ClassThumbnail.TargetingPreset", SLATE_IMAGE_BRUSH("TargetingDataIcon", 128.f));

		StyleSetInstance->Set("ClassIcon.TargetingQueryComponent", SLATE_IMAGE_BRUSH("TargetingDataIcon", 20.f));
		StyleSetInstance->Set("ClassThumbnail.TargetingQueryComponent", SLATE_IMAGE_BRUSH("TargetingDataIcon", 128.f));

		// Finally register the style set
		FSlateStyleRegistry::RegisterSlateStyle(*StyleSetInstance);
	}

	// Register pin for FAttribute
	FEdGraphUtilities::RegisterVisualPinFactory(MakeShareable(new FAttributesGraphPanelPinFactory));
}

void FEditorUtilitiesModule::ShutdownModule()
{
	// Unregister visualizers
	if (GUnrealEd)
	{
		GUnrealEd->UnregisterComponentVisualizer(UDialogueInteractableComponent::StaticClass()->GetFName());
	}

	// Unregister style set
	{
		FSlateStyleRegistry::UnRegisterSlateStyle(*StyleSetInstance.Get());
		StyleSetInstance.Reset();
	}
}

#undef LOCTEXT_NAMESPACE
    
IMPLEMENT_MODULE(FEditorUtilitiesModule, EditorUtilities)