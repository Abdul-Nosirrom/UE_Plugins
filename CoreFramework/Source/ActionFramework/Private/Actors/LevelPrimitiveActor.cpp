// Fill out your copyright notice in the Description page of Project Settings.


#include "Actors/LevelPrimitiveActor.h"
#include "Actors/RadicalPlayerCharacter.h"
#include "Components/ActionManagerComponent.h"
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
	PlayerCharacter = Cast<ARadicalPlayerCharacter>(UGameplayStatics::GetPlayerPawn(GetWorld(), 0));
}

void ALevelPrimitiveActor::RegisterWithPlayer()
{
	PlayerCharacter->GetActionManager()->RegisterLevelPrimitive(this);
}

void ALevelPrimitiveActor::UnRegisterPlayer()
{
	PlayerCharacter->GetActionManager()->UnRegisterLevelPrimitive(this);
}
