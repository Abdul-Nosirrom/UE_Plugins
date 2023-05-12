// 


#include "ActionSystem/GameplayActionTypes.h"

#include "RadicalCharacter.h"
#include "RadicalMovementComponent.h"
#include "Components/ActionSystemComponent.h"
#include "Components/SkeletalMeshComponent.h"

void FActionActorInfo::InitFromCharacter(ARadicalCharacter* Character, UActionSystemComponent* InActionSystemComponent)
{
	check(Character);
	check(InActionSystemComponent);
	
	CharacterOwner = Character;
	ActionSystemComponent = InActionSystemComponent;
	MovementComponent = Character->GetCharacterMovement();
	SkeletalMeshComponent = Character->GetMesh();
}
