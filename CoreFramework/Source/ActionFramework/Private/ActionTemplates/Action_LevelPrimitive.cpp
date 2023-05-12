// 


#include "ActionTemplates/Action_LevelPrimitive.h"
#include "RadicalMovementComponent.h"
#include "Actors/LevelPrimitiveActor.h"
#include "Actors/RadicalCharacter.h"
#include "Components/ActionSystemComponent.h"

void UAction_LevelPrimitive::OnActionActivated_Implementation()
{
	CurrentActorInfo->MovementComponent.Get()->SetMovementState(STATE_General);
}

void UAction_LevelPrimitive::OnActionEnd_Implementation()
{
	CurrentActorInfo->MovementComponent.Get()->SetMovementState(STATE_Falling);
}

bool UAction_LevelPrimitive::EnterCondition_Implementation()
{
	LevelPrimitive = CurrentActorInfo->ActionSystemComponent.Get()->GetActiveLevelPrimitive(PrimitiveTag);
	return LevelPrimitive != nullptr;
}
