// Fill out your copyright notice in the Description page of Project Settings.


#include "Components/LevelPrimitiveComponent.h"

#include "ImageUtils.h"
#include "Components/ActionSystemComponent.h"
#include "Components/BillboardComponent.h"
#include "Components/SplineComponent.h"
#include "Interfaces/IPluginManager.h"

// Sets default values for this component's properties
ULevelPrimitiveComponent::ULevelPrimitiveComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;
	
}


void ULevelPrimitiveComponent::OnRegister()
{
	Super::OnRegister();
	
#if WITH_EDITORONLY_DATA
	static FString ContentDir = IPluginManager::Get().FindPlugin(TEXT("CoreFramework"))->GetBaseDir() / TEXT("Resources");
	static FString IconDir = (ContentDir / TEXT("LPIcon")) + TEXT(".png");
	const auto Icon = FImageUtils::ImportFileAsTexture2D(IconDir);
	SpriteComponent = NewObject<UBillboardComponent>(GetOwner(), NAME_None, RF_Transactional | RF_Transient | RF_TextExportTransient);
	SpriteComponent->Sprite = Icon;
	SpriteComponent->Mobility = EComponentMobility::Movable;
	SpriteComponent->SetupAttachment(GetOwner()->GetRootComponent());
	SpriteComponent->SpriteInfo.Category = TEXT("Misc");
	SpriteComponent->SpriteInfo.DisplayName = NSLOCTEXT("SpriteCategory", "Misc", "Misc");
	SpriteComponent->CreationMethod = CreationMethod;
	SpriteComponent->bIsScreenSizeScaled = true;
	SpriteComponent->bUseInEditorScaling = true;
	SpriteComponent->OpacityMaskRefVal = .3f;
	SpriteComponent->RegisterComponent();
#endif 
}



void ULevelPrimitiveComponent::RegisterLevelPrimative(UActionSystemComponent* Target)
{
	Target->RegisterLevelPrimitive(this);
}

void ULevelPrimitiveComponent::UnRegisterLevelPrimative(UActionSystemComponent* Target)
{
	Target->UnRegisterLevelPrimitive(this);
}
