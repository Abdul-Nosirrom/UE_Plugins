// 


#include "AssetCategories/SharedAssetFactory.h"

#include "DataTableEditorModule.h"
#include "InputData.h"
#include "ActionSystem/ActionBlueprint.h"
#include "ActionSystem/GameplayAction.h"
#include "AttributeSystem/EntityEffect.h"
#include "AttributeSystem/EntityEffectBlueprint.h"
#include "Data/TargetingPreset.h"
#include "Editor/ActionBlueprintFactory.h"
#include "Editor/ActionDataFactory.h"
#include "Editor/EntityEffectBlueprintFactory.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"

// --------------------------------------------------------------------------------------------------------------------
// Gameplay Actions Category
// --------------------------------------------------------------------------------------------------------------------

#define LOCTEXT_NAMESPACE "AssetTypeActions"

FText FAssetTypeActions_GameplayActionBlueprint::GetName() const
{ 
	return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_GameplayActionBlueprint", "Gameplay Action Blueprint"); 
}

FColor FAssetTypeActions_GameplayActionBlueprint::GetTypeColor() const
{
	return FColor(0, 96, 128);
}


bool FAssetTypeActions_GameplayActionBlueprint::ShouldUseDataOnlyEditor(const UBlueprint* Blueprint) const
{
	return FBlueprintEditorUtils::IsDataOnlyBlueprint(Blueprint)
		&& !FBlueprintEditorUtils::IsLevelScriptBlueprint(Blueprint)
		&& !FBlueprintEditorUtils::IsInterfaceBlueprint(Blueprint)
		&& !Blueprint->bForceFullEditor
		&& !Blueprint->bIsNewlyCreated;
}

UClass* FAssetTypeActions_GameplayActionBlueprint::GetSupportedClass() const
{ 
	return UActionBlueprint::StaticClass(); 
}

UFactory* FAssetTypeActions_GameplayActionBlueprint::GetFactoryForBlueprintType(UBlueprint* InBlueprint) const
{
	UActionBlueprintFactory* GameplayAbilitiesBlueprintFactory = NewObject<UActionBlueprintFactory>();
	GameplayAbilitiesBlueprintFactory->ParentClass = TSubclassOf<UGameplayAction>(*InBlueprint->GeneratedClass);
	return GameplayAbilitiesBlueprintFactory;
}

//#undef LOCTEXT_NAMESPACE

// --------------------------------------------------------------------------------------------------------------------
// Action Data Categories
// --------------------------------------------------------------------------------------------------------------------

FText FATA_ActionData::GetName() const 
{
	return NSLOCTEXT("AssetTypeActions", "ATA_ActionData", "Action Data Asset"); 
}

FText FATA_ActionData::GetAssetDescription(const FAssetData& AssetData) const
{
	return LOCTEXT("AssetTypeActions", "Modifiable Parameters Of A Given Gameplay Action");
}

UClass* FATA_ActionData::GetSupportedClass() const
{
	return UGameplayActionData::StaticClass();//TSubclassOf<UGameplayActionData>();
}

// --------------------------------------------------------------------------------------------------------------------
// Entity Effect Category
// --------------------------------------------------------------------------------------------------------------------

FText FAssetTypeActions_EntityEffectBlueprint::GetName() const
{ 
	return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_EntityEffectBlueprint", "Entity Effect"); 
}

FColor FAssetTypeActions_EntityEffectBlueprint::GetTypeColor() const
{
	return FColor(128, 0, 0);
}


bool FAssetTypeActions_EntityEffectBlueprint::ShouldUseDataOnlyEditor(const UBlueprint* Blueprint) const
{
	return FBlueprintEditorUtils::IsDataOnlyBlueprint(Blueprint)
		&& !FBlueprintEditorUtils::IsLevelScriptBlueprint(Blueprint)
		&& !FBlueprintEditorUtils::IsInterfaceBlueprint(Blueprint)
		&& !Blueprint->bForceFullEditor
		&& !Blueprint->bIsNewlyCreated;
}

UClass* FAssetTypeActions_EntityEffectBlueprint::GetSupportedClass() const
{ 
	return UEntityEffectBlueprint::StaticClass(); 
}

UFactory* FAssetTypeActions_EntityEffectBlueprint::GetFactoryForBlueprintType(UBlueprint* InBlueprint) const
{
	UEntityEffectBlueprintFactory* EntityEffectBlueprintFactory = NewObject<UEntityEffectBlueprintFactory>();
	EntityEffectBlueprintFactory->ParentClass = TSubclassOf<UEntityEffect>(*InBlueprint->GeneratedClass);
	return EntityEffectBlueprintFactory;
}


FText FATA_AttributeDataTable::GetName() const
{
	return NSLOCTEXT("AssetTypeActions", "FATA_AttributeDataTable", "Initial Attribute Data"); 
}

FText FATA_AttributeDataTable::GetAssetDescription(const FAssetData& AssetData) const
{
	return LOCTEXT("AssetTypeActions", "Initial Data To Fill Out An Attribute Set");
}

UClass* FATA_AttributeDataTable::GetSupportedClass() const
{
	return UAttributeInitDataTable::StaticClass();
}

void FATA_AttributeDataTable::OpenAssetEditor(const TArray<UObject*>& InObjects,
	TSharedPtr<IToolkitHost> EditWithinLevelEditor)
{
	//@TODO: DarenC - This is from FAssetTypeActions_DataTable - but we can't derive to private include.
	TArray<UDataTable*> DataTablesToOpen;
	TArray<UDataTable*> InvalidDataTables;

	for (UObject* Obj : InObjects)
	{
		UDataTable* Table = Cast<UDataTable>(Obj);
		if (Table)
		{
			if (Table->GetRowStruct())
			{
				DataTablesToOpen.Add(Table);
			}
			else
			{
				InvalidDataTables.Add(Table);
			}
		}
	}

	if (InvalidDataTables.Num() > 0)
	{
		FTextBuilder DataTablesListText;
		DataTablesListText.Indent();
		for (UDataTable* Table : InvalidDataTables)
		{
			const FTopLevelAssetPath ResolvedRowStructName = Table->GetRowStructPathName();
			DataTablesListText.AppendLineFormat(LOCTEXT("DataTable_MissingRowStructListEntry", "* {0} (Row Structure: {1})"), FText::FromString(Table->GetName()), FText::FromString(ResolvedRowStructName.ToString()));
		}

		const FText Title = LOCTEXT("DataTable_MissingRowStructTitle", "Continue?");
		const EAppReturnType::Type DlgResult = FMessageDialog::Open(
			EAppMsgType::YesNoCancel,
			FText::Format(LOCTEXT("DataTable_MissingRowStructMsg", "The following Data Tables are missing their row structure and will not be editable.\n\n{0}\n\nDo you want to open these data tables?"), DataTablesListText.ToText()),
			Title
		);

		switch (DlgResult)
		{
			case EAppReturnType::Yes:
				DataTablesToOpen.Append(InvalidDataTables);
			break;
			case EAppReturnType::Cancel:
				return;
			default:
				break;
		}
	}

	FDataTableEditorModule& DataTableEditorModule = FModuleManager::LoadModuleChecked<FDataTableEditorModule>("DataTableEditor");
	for (UDataTable* Table : DataTablesToOpen)
	{
		DataTableEditorModule.CreateDataTableEditor(EToolkitMode::Standalone, EditWithinLevelEditor, Table);
	}
}

// --------------------------------------------------------------------------------------------------------------------
// Combat Asset Category
// --------------------------------------------------------------------------------------------------------------------


UTargetingPresetFactory::UTargetingPresetFactory()
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UTargetingPreset::StaticClass();
}

UObject* UTargetingPresetFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags,
	UObject* Context, FFeedbackContext* Warn)
{
	check(Class->IsChildOf(UTargetingPreset::StaticClass()));
	return NewObject<UTargetingPreset>(InParent,Class,Name,Flags|RF_Transactional,Context);
}

FText FATA_TargetingPreset::GetName() const
{
	return NSLOCTEXT("AssetTypeActions", "FATA_TargetingPreset", "Targeting Preset"); 
}

FText FATA_TargetingPreset::GetAssetDescription(const FAssetData& AssetData) const
{
	return NSLOCTEXT("AssetTypeActions", "FATA_TargetingPreset", "Settings For Performing A Targeting Request"); 
}

UClass* FATA_TargetingPreset::GetSupportedClass() const
{
	return UTargetingPreset::StaticClass();
}

#undef LOCTEXT_NAMESPACE

// --------------------------------------------------------------------------------------------------------------------
// Input Buffer Categories
// --------------------------------------------------------------------------------------------------------------------

#define LOCTEXT_NAMESPACE "InputBuffer"

UInputBufferMapFactory::UInputBufferMapFactory()
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UInputBufferMap::StaticClass();
}

UObject* UInputBufferMapFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags,
	UObject* Context, FFeedbackContext* Warn)
{
	check(Class->IsChildOf(UInputBufferMap::StaticClass()));
	return NewObject<UInputBufferMap>(InParent,Class,Name,Flags|RF_Transactional,Context);
}

FText FATA_InputBufferMap::GetName() const
{
	return LOCTEXT("InputBuffer", "Input Buffer Map");
}

FText FATA_InputBufferMap::GetAssetDescription(const FAssetData& AssetData) const
{
	return LOCTEXT("InputBuffer", "Mapping Context For An Input Buffer");
}

UClass* FATA_InputBufferMap::GetSupportedClass() const
{
	return UInputBufferMap::StaticClass();
}

/* ------------------------------------------------------------------------ */

UMotionMapFactory::UMotionMapFactory()
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UMotionMappingContext::StaticClass();
}

UObject* UMotionMapFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags,
	UObject* Context, FFeedbackContext* Warn)
{
	check(Class->IsChildOf(UMotionMappingContext::StaticClass()));
	return NewObject<UMotionMappingContext>(InParent,Class,Name,Flags|RF_Transactional,Context);
}

FText FATA_MotionMap::GetName() const
{
	return LOCTEXT("InputBuffer", "Directional Input Map");
}

FText FATA_MotionMap::GetAssetDescription(const FAssetData& AssetData) const
{
	return LOCTEXT("InputBuffer", "Mapping Context For Directional Input");
}

UClass* FATA_MotionMap::GetSupportedClass() const
{
	return UMotionMappingContext::StaticClass();
}

/* ------------------------------------------------------------------------ */

UMotionActionFactory::UMotionActionFactory()
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UMotionAction::StaticClass();
}

UObject* UMotionActionFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags,
	UObject* Context, FFeedbackContext* Warn)
{
	check(Class->IsChildOf(UMotionAction::StaticClass()));
	return NewObject<UMotionAction>(InParent,Class,Name,Flags|RF_Transactional,Context);
}

FText FATA_MotionAction::GetName() const
{
	return LOCTEXT("InputBuffer", "Directional Input Action");
}

FText FATA_MotionAction::GetAssetDescription(const FAssetData& AssetData) const
{
	return LOCTEXT("InputBuffer", "Definition Of A Directional Input");
}

UClass* FATA_MotionAction::GetSupportedClass() const
{
	return UMotionAction::StaticClass();
}

#undef LOCTEXT_NAMESPACE
