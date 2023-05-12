// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "GameFramework/Actor.h"
#include "LevelPrimitiveActor.generated.h"

class ARadicalCharacter;

UCLASS()
class ACTIONFRAMEWORK_API ALevelPrimitiveActor : public AActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	ALevelPrimitiveActor();

	FORCEINLINE FGameplayTag GetTag() const { return TagToGrant; }

protected:
	virtual void BeginPlay() override;

	UFUNCTION(BlueprintCallable)
	void RegisterWithPlayer();
	UFUNCTION(BlueprintCallable)
	void UnRegisterPlayer();

	UPROPERTY(Transient)
	ARadicalCharacter* PlayerCharacter;
	UPROPERTY( Category="GamplayTags",EditDefaultsOnly, BlueprintReadOnly)
	FGameplayTag TagToGrant;
};