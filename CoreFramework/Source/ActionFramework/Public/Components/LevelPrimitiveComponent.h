// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "LevelPrimitiveComponent.generated.h"


class UActionSystemComponent;

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class ACTIONFRAMEWORK_API ULevelPrimitiveComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	ULevelPrimitiveComponent();

	FORCEINLINE FGameplayTag GetTag() const { return TagToGrant; }
	
	virtual void OnRegister() override;



protected:
	UPROPERTY( Category="GamplayTags",EditDefaultsOnly, BlueprintReadOnly)
	FGameplayTag TagToGrant;

#if WITH_EDITORONLY_DATA
	//UPROPERTY()
	class UBillboardComponent* SpriteComponent;
#endif
	
	UFUNCTION(BlueprintCallable)
	void RegisterLevelPrimative(UActionSystemComponent* Target);


	UFUNCTION(BlueprintCallable)
	void UnRegisterLevelPrimative(UActionSystemComponent* Target);

	UPROPERTY(EditAnywhere)
	class USplineComponent* Spline;
	
};
