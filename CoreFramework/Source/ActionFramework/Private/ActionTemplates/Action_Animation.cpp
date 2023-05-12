// 


#include "ActionTemplates/Action_Animation.h"

#include "ActionSystem/GameplayActionTypes.h"
#include "Actors/RadicalCharacter.h"

void UAction_Animation::OnActionActivated_Implementation()
{
	PlayAnimMontage(this, CurrentActorInfo->SkeletalMeshComponent.Get(), MontageToPlay, PlayRate, StartingPosition, StartingSection);
}
