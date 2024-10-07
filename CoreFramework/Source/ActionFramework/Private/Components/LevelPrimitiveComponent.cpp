// Fill out your copyright notice in the Description page of Project Settings.


#include "Components/LevelPrimitiveComponent.h"

#include "ImageUtils.h"
#include "TimerManager.h"
#include "Components/ActionSystemComponent.h"
#include "Components/BillboardComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SplineComponent.h"
#include "Interfaces/IPluginManager.h"

/* Define Gameplay Tag */
UE_DEFINE_GAMEPLAY_TAG(TAG_LevelPrimitive_Base, "LevelPrimitive");

// Sets default values for this component's properties
ULevelPrimitiveComponent::ULevelPrimitiveComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;
	
}


void ULevelPrimitiveComponent::BeginPlay()
{
	Super::BeginPlay();

	switch (DefaultBehavior)
	{
		case LP_None: break;
		case LP_OnOverlapRoot:
			{
				GetOwner()->OnActorBeginOverlap.AddDynamic(this, &ULevelPrimitiveComponent::OnActorBeginOverlap);
				GetOwner()->OnActorEndOverlap.AddDynamic(this, &ULevelPrimitiveComponent::OnActorEndOverlap);
				break;
			}
		case LP_OnOverlapComponent:
			{
				Component->OnComponentBeginOverlap.AddDynamic(this, &ULevelPrimitiveComponent::OnComponentBeginOverlap);
				Component->OnComponentEndOverlap.AddDynamic(this, &ULevelPrimitiveComponent::OnComponentEndOverlap);
				break;
			}
		case LP_OnHitRoot:
			{
				GetOwner()->OnActorHit.AddDynamic(this, &ULevelPrimitiveComponent::OnActorHit);
				break;
			}
		case LP_OnHitComponent:
			{
				Component->OnComponentHit.AddDynamic(this, &ULevelPrimitiveComponent::OnComponentHit);
				break;
			}
		default: break;
	}
}

void ULevelPrimitiveComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
	GetWorld()->GetTimerManager().ClearTimer(DelayedExitTimerHandle);
}

void ULevelPrimitiveComponent::OnRegister()
{
	Super::OnRegister();
	
#if WITH_EDITORONLY_DATA
	if (!GetOwner()->FindComponentByClass(UBillboardComponent::StaticClass()))
	{
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
		SpriteComponent->AddRelativeLocation(FVector(0,0,100));
		SpriteComponent->RegisterComponent();
		SpriteComponent->SetDepthPriorityGroup(ESceneDepthPriorityGroup::SDPG_MAX);
	}
#endif

	//Spline = GenerateRequiredComponent<USplineComponent>();
	//SplineMesh = GenerateRequiredComponent<UChildActorComponent>();
	//SplineMesh->SetChildActorClass(ASMSplineMeshActor::)
	GenerateComponentsCallback();

	//SplineMesh = GenerateRequiredComponent<UCapsuleComponent>();
	//SplineMesh->SetChildActorClass(ClassToGenerate);
}

template <typename T>
T* ULevelPrimitiveComponent::GenerateRequiredComponent()
{
	T* RequiredComponent = NewObject<T>(GetOwner(), NAME_None);
	RequiredComponent->Mobility = EComponentMobility::Movable;
	RequiredComponent->SetupAttachment(GetOwner()->GetRootComponent());
	RequiredComponent->CreationMethod = CreationMethod;
	RequiredComponent->RegisterComponent();

	return RequiredComponent;
}

bool ULevelPrimitiveComponent::OnBehaviorInitiated(AActor* OtherActor)
{
	if (auto ASC = Cast<UActionSystemComponent>(OtherActor->FindComponentByClass(UActionSystemComponent::StaticClass())))
	{
		RegisterLevelPrimitive(ASC);
		return true;
	}
	return false;
}

bool ULevelPrimitiveComponent::OnBehaviorExited(AActor* OtherActor)
{
	if (auto ASC = Cast<UActionSystemComponent>(OtherActor->FindComponentByClass(UActionSystemComponent::StaticClass())))
	{
		UnRegisterLevelPrimitive(ASC);
		return true;
	}
	return false; 
}

void ULevelPrimitiveComponent::DelayedBehaviorExit(AActor* OtherActor)
{
	FTimerDelegate TimerDelegate;

	TimerDelegate.BindLambda([this, OtherActor]
	{
		if (!OtherActor) return;
		if (auto ASC = Cast<UActionSystemComponent>(OtherActor->FindComponentByClass(UActionSystemComponent::StaticClass())))
		{
			UnRegisterLevelPrimitive(ASC);
		}
	});

	GetWorld()->GetTimerManager().SetTimer(DelayedExitTimerHandle, TimerDelegate, UnRegisterDelay, false);
}

void ULevelPrimitiveComponent::RegisterLevelPrimitive(UActionSystemComponent* Target, FGameplayTag GrantingTag)
{
	Target->RegisterLevelPrimitive(this, GrantingTag);
}

void ULevelPrimitiveComponent::UnRegisterLevelPrimitive(UActionSystemComponent* Target, FGameplayTag GrantingTag)
{
	Target->UnRegisterLevelPrimitive(this, GrantingTag);
}
