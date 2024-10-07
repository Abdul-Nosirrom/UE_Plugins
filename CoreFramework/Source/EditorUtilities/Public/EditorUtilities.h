#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FEditorUtilitiesModule : public IModuleInterface
{
    static inline TSharedPtr<FSlateStyleSet> StyleSetInstance = nullptr;
    
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;
};
