// 


#include "AssetCategories/SharedAssetFactory.h"

#include "InputData.h"
#include "ActionSystem/ActionBlueprint.h"
#include "ActionSystem/GameplayAction.h"
#include "AssetCategories/AssetEditorUtilities.h"
#include "Editor/ActionBlueprintFactory.h"
#include "Editor/ActionDataFactory.h"
#include "Editor/ActionDataBlueprintFactory.h"
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

FText FAssetTypeActions_ActionDataBlueprint::GetName() const
{ 
	return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_GameplayActionBlueprint", "Action Data Definition"); 
}

FColor FAssetTypeActions_ActionDataBlueprint::GetTypeColor() const
{
	return FColor(0, 96, 128);
}


bool FAssetTypeActions_ActionDataBlueprint::ShouldUseDataOnlyEditor(const UBlueprint* Blueprint) const
{
	return FBlueprintEditorUtils::IsDataOnlyBlueprint(Blueprint)
		&& !FBlueprintEditorUtils::IsLevelScriptBlueprint(Blueprint)
		&& !FBlueprintEditorUtils::IsInterfaceBlueprint(Blueprint)
		&& !Blueprint->bForceFullEditor
		&& !Blueprint->bIsNewlyCreated;
}

UClass* FAssetTypeActions_ActionDataBlueprint::GetSupportedClass() const
{ 
	return UActionDataBlueprint::StaticClass(); 
}

UFactory* FAssetTypeActions_ActionDataBlueprint::GetFactoryForBlueprintType(UBlueprint* InBlueprint) const
{
	UActionDataBlueprintFactory* GameplayAbilitiesBlueprintFactory = NewObject<UActionDataBlueprintFactory>();
	GameplayAbilitiesBlueprintFactory->ParentClass = TSubclassOf<UGameplayActionData>(*InBlueprint->GeneratedClass);
	return GameplayAbilitiesBlueprintFactory;
}

//#define LOCTEXT_NAMESPACE "ActionData"

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
