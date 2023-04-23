// 

#pragma once

#include "CoreMinimal.h"
#include "GameplayActionTypes.generated.h"

class ARadicalCharacter;
class UActionManagerComponent;
class URadicalMovementComponent;
class UAnimInstance;
class USkeletalMeshComponent;


USTRUCT(BlueprintType)
struct ACTIONFRAMEWORK_API FActionActorInfo
{
	GENERATED_BODY()

	virtual ~FActionActorInfo() {}
	
	// NOTE: May change these to TWeakObjectPtr later

	UPROPERTY(Category=ActorInfo, BlueprintReadOnly)
	TWeakObjectPtr<ARadicalCharacter> CharacterOwner;

	UPROPERTY(Category=ActorInfo, BlueprintReadOnly)
	TWeakObjectPtr<UActionManagerComponent> ActionManagerComponent;

	UPROPERTY(Category=ActorInfo, BlueprintReadOnly)
	TWeakObjectPtr<URadicalMovementComponent> MovementComponent;

	UPROPERTY(Category=ActorInfo, BlueprintReadOnly)
	TWeakObjectPtr<USkeletalMeshComponent> SkeletalMeshComponent;

	virtual void InitFromCharacter(ARadicalCharacter* Character, UActionManagerComponent* InActionManagerComponent);
};
