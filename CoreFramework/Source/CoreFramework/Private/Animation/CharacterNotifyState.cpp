// Copyright 2023 CoC All rights reserved


#include "Animation/CharacterNotifyState.h"
#include "RadicalCharacter.h"


FAnimPhysicsInfo::FAnimPhysicsInfo(ARadicalCharacter* Owner, float InStartTime, float InDuration)
	: Character(Owner), StartTime(InStartTime), Duration(InDuration), NormalizedTime(0.f)
{}

void UCharacterNotifyState::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	const float StartTime = EventReference.GetNotify()->GetTriggerTime();
	
	PhysicsInfo.Add(MeshComp, FAnimPhysicsInfo(Cast<ARadicalCharacter>(MeshComp->GetOwner()), StartTime, TotalDuration));
	if (MeshComp->GetOwner())
		PhysicsInfo[MeshComp].InitialTransform = MeshComp->GetOwner()->GetActorTransform();
}

void UCharacterNotifyState::NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	float FrameDeltaTime, const FAnimNotifyEventReference& EventReference)
{
	if (!MeshComp || !PhysicsInfo.Contains(MeshComp)) return;
	PhysicsInfo[MeshComp].NormalizedTime += FrameDeltaTime / PhysicsInfo[MeshComp].Duration;//(EventReference.GetCurrentAnimationTime() - PhysicsInfo[MeshComp].StartTime) / PhysicsInfo[MeshComp].Duration;
}

void UCharacterNotifyState::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	PhysicsInfo.Remove(MeshComp);
}
