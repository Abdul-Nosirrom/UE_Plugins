// 


#include "ActionSystem/GameplayActionTypes.h"

#include "RadicalCharacter.h"
#include "RadicalMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"

void FActionActorInfo::InitFromCharacter(ARadicalCharacter* Character, UActionManagerComponent* InActionManagerComponent)
{
	check(Character);
	check(InActionManagerComponent);
	
	CharacterOwner = Character;
	ActionManagerComponent = InActionManagerComponent;
	MovementComponent = Character->GetCharacterMovement();
	SkeletalMeshComponent = Character->GetMesh();
}
