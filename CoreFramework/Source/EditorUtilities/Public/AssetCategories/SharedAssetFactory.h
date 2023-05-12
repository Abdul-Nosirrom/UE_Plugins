// 

#pragma once

#include "CoreMinimal.h"
#include "AssetTypeActions_Base.h"
#include "UnrealEd.h"
#include "ActionSystem/GameplayAction.h"
#include "AssetTypeActions/AssetTypeActions_Blueprint.h"
#include "AssetTypeActions/AssetTypeActions_DataAsset.h"
#include "SharedAssetFactory.generated.h"


static EAssetTypeCategories::Type ActionAssetCategory;
static EAssetTypeCategories::Type InputBufferAssetCategory;


// --------------------------------------------------------------------------------------------------------------------
// Gameplay Actions Category
// --------------------------------------------------------------------------------------------------------------------

class FAssetTypeActions_GameplayActionBlueprint : public FAssetTypeActions_Blueprint
{
public:
	// IAssetTypeActions Implementation
	virtual FText GetName() const override;
	virtual FColor GetTypeColor() const override;
	virtual UClass* GetSupportedClass() const override;
	virtual uint32 GetCategories() override { return ActionAssetCategory; }
	// End IAssetTypeActions Implementation

	// FAssetTypeActions_Blueprint interface
	virtual UFactory* GetFactoryForBlueprintType(UBlueprint* InBlueprint) const override;

private:
	/** Returns true if the blueprint is data only */
	bool ShouldUseDataOnlyEditor(const UBlueprint* Blueprint) const;
};

// --------------------------------------------------------------------------------------------------------------------
// Action Data Category
// --------------------------------------------------------------------------------------------------------------------


class FAssetTypeActions_ActionDataBlueprint : public FAssetTypeActions_Blueprint
{
public:
	// IAssetTypeActions Implementation
	virtual FText GetName() const override;
	virtual FColor GetTypeColor() const override;
	virtual UClass* GetSupportedClass() const override;
	virtual uint32 GetCategories() override { return ActionAssetCategory; }
	// End IAssetTypeActions Implementation

	// FAssetTypeActions_Blueprint interface
	virtual UFactory* GetFactoryForBlueprintType(UBlueprint* InBlueprint) const override;

private:
	/** Returns true if the blueprint is data only */
	bool ShouldUseDataOnlyEditor(const UBlueprint* Blueprint) const;
};

class FATA_ActionData : public FAssetTypeActions_DataAsset
{
public:
	virtual FText GetName() const override;
	virtual uint32 GetCategories() override { return ActionAssetCategory; }
	virtual FColor GetTypeColor() const override { return FColor(255, 50, 255);}
	virtual FText GetAssetDescription(const FAssetData& AssetData) const override;
	virtual UClass* GetSupportedClass() const override;
};


// --------------------------------------------------------------------------------------------------------------------
// Input Buffer Map Category
// --------------------------------------------------------------------------------------------------------------------
UCLASS()
class EDITORUTILITIES_API UInputBufferMapFactory : public UFactory
{
	GENERATED_BODY()

	UInputBufferMapFactory();
	
	// UFactory interface
	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;
	// End of UFactory interface
	
};

class FATA_InputBufferMap : public FAssetTypeActions_Base
{
public:
	virtual FText GetName() const override;
	virtual uint32 GetCategories() override { return InputBufferAssetCategory; }
	virtual FColor GetTypeColor() const override { return FColor(127, 255, 255);}
	virtual FText GetAssetDescription(const FAssetData& AssetData) const override;
	virtual UClass* GetSupportedClass() const override;
};

UCLASS()
class EDITORUTILITIES_API UMotionMapFactory : public UFactory
{
	GENERATED_BODY()

	UMotionMapFactory();
	
	// UFactory interface
	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;
	// End of UFactory interface
	
};

class FATA_MotionMap : public FAssetTypeActions_Base
{
public:
	virtual FText GetName() const override;
	virtual uint32 GetCategories() override { return InputBufferAssetCategory; }
	virtual FColor GetTypeColor() const override { return FColor(255, 127, 255);}
	virtual FText GetAssetDescription(const FAssetData& AssetData) const override;
	virtual UClass* GetSupportedClass() const override;
};

UCLASS()
class EDITORUTILITIES_API UMotionActionFactory : public UFactory
{
	GENERATED_BODY()

	UMotionActionFactory();
	
	// UFactory interface
	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;
	// End of UFactory interface
	
};

class FATA_MotionAction : public FAssetTypeActions_Base
{
public:
	virtual FText GetName() const override;
	virtual uint32 GetCategories() override { return InputBufferAssetCategory; }
	virtual FColor GetTypeColor() const override { return FColor(255, 255, 127);}
	virtual FText GetAssetDescription(const FAssetData& AssetData) const override;
	virtual UClass* GetSupportedClass() const override;
};