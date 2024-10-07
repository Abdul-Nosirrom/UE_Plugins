﻿// Copyright 2023 CoC All rights reserved

#pragma once

#include "CoreMinimal.h"
#include "ImGuiWindow.h"
#include "ImGuiWindowManager.h"

#include "EngineWindow_PropertyInspector.generated.h"

class UImGuiEngineConfig_PropertyInspector;
using FImGuiEngineInspectorApplyFunction = TFunction<void(UObject*)>;

class FEngineWindow_PropertyInspector : public FImGuiWindow
{
public:
    
    virtual void Initialize() override;

    virtual UObject* GetInspectedObject() const { return InspectedObject.Get(); }

    virtual void SetInspectedObject(UObject* Object);

    virtual void AddFavorite(UObject* Object);

    virtual void AddFavorite(UObject* Object, FImGuiEngineInspectorApplyFunction ApplyFunction);
    
    virtual void RenderHelp() override;

    virtual void RenderContent() override;

protected:
    
    virtual void RenderMenu();

    virtual bool RenderInspector();

    virtual bool RenderBegin();

    virtual void RenderEnd();

    virtual bool HasPropertyAnyChildren(const FProperty* Property, uint8* PointerToValue);

    virtual bool RenderPropertyList(TArray<const FProperty*>& Properties, uint8* PointerToValue);

    virtual bool RenderProperty(const FProperty* Property, uint8* PointerToValue, int IndexInArray);

    virtual bool RenderBool(const FBoolProperty* BoolProperty, uint8* PointerToValue);

    virtual bool RenderByte(const FByteProperty* ByteProperty, uint8* PointerToValue);

    virtual bool RenderInt8(const FInt8Property* Int8Property, uint8* PointerToValue);

    virtual bool RenderInt(const FIntProperty* IntProperty, uint8* PointerToValue);

    virtual bool RenderInt64(const FInt64Property* Int64Property, uint8* PointerToValue);

    virtual bool RenderUInt32(const FUInt32Property* UInt32Property, uint8* PointerToValue);

    virtual bool RenderFloat(const FFloatProperty* FloatProperty, uint8* PointerToValue);

    virtual bool RenderDouble(const FDoubleProperty* DoubleProperty, uint8* PointerToValue);

    virtual bool RenderEnum(const FEnumProperty* EnumProperty, uint8* PointerToValue);

    virtual bool RenderString(const FStrProperty* StrProperty, uint8* PointerToValue);

    virtual bool RenderName(const FNameProperty* NameProperty, uint8* PointerToValue);

    virtual bool RenderText(const FTextProperty* TextProperty, uint8* PointerToValue);

    virtual bool RenderClass(const FClassProperty* ClassProperty);

    virtual bool RenderInterface(const FInterfaceProperty* InterfaceProperty);

    virtual bool RenderStruct(const FStructProperty* StructProperty, uint8* PointerToValue, bool ShowChildren);

    virtual bool RenderObject(UObject* Object, bool ShowChildren);

    virtual bool RenderArray(const FArrayProperty* ArrayProperty, uint8* PointerToValue, bool ShowChildren);

    virtual FString GetPropertyName(const FProperty& Property);

    FImGuiEngineInspectorApplyFunction FindObjectApplyFunction(const UObject* Object) const;

    struct Favorite
    {
        TWeakObjectPtr<UObject> Object = nullptr;

        FImGuiEngineInspectorApplyFunction ApplyFunction = nullptr;
    };

    TWeakObjectPtr<UObject> InspectedObject = nullptr;
    
    ImGuiTextFilter Filter;

    bool bExpandAllCategories = false;

    bool bCollapseAllCategories = false;

    TArray<Favorite> Favorites;

    TArray<TWeakObjectPtr<UObject>> History;

    int32 HistoryIndex = INDEX_NONE;

    TWeakObjectPtr<UImGuiEngineConfig_PropertyInspector> Config = nullptr;
};


//--------------------------------------------------------------------------------------------------------------------------
UCLASS(Config = ImGui)
class UImGuiEngineConfig_PropertyInspector : public UImGuiWindowConfig
{
    GENERATED_BODY()

public:

    UPROPERTY(Config)
    bool bSyncWithSelection = true;

    UPROPERTY(Config)
    bool bShowDisplayName = true;

    UPROPERTY(Config)
    bool bShowRowBackground = true;

    UPROPERTY(Config)
    bool bShowBorders = false;

    UPROPERTY(Config)
    bool bShowCategories = true;

    UPROPERTY(Config)
    bool bSortByName = true;

    virtual void Reset() override
    {
        Super::Reset();

        bSyncWithSelection = true;
        bShowDisplayName = true;
        bShowRowBackground = true;
        bShowBorders = false;
        bShowCategories = true;
        bSortByName = true;
    }
};
