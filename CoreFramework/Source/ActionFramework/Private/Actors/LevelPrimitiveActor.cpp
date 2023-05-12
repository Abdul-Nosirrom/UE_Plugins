// Fill out your copyright notice in the Description page of Project Settings.


#include "Actors/LevelPrimitiveActor.h"
#include "Actors/RadicalCharacter.h"
#include "Components/ActionSystemComponent.h"
#include "GameplayTagContainer.h"
#include "GameplayTags.h"
#include "Kismet/GameplayStatics.h"


// Sets default values
ALevelPrimitiveActor::ALevelPrimitiveActor()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
}

// Called when the game starts or when spawned
void ALevelPrimitiveActor::BeginPlay()
{
	Super::BeginPlay();
	PlayerCharacter = Cast<ARadicalCharacter>(UGameplayStatics::GetPlayerPawn(GetWorld(), 0));
}

void ALevelPrimitiveActor::RegisterWithPlayer()
{
	Cast<UActionSystemComponent>(PlayerCharacter->FindComponentByClass(UActionSystemComponent::StaticClass()))->RegisterLevelPrimitive(this);
}

void ALevelPrimitiveActor::UnRegisterPlayer()
{
	Cast<UActionSystemComponent>(PlayerCharacter->FindComponentByClass(UActionSystemComponent::StaticClass()))->UnRegisterLevelPrimitive(this);
}
