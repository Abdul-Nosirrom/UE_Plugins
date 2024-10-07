// Copyright 2023 CoC All rights reserved


#include "Animation/AnimNotifyState_Rotation.h"

#include "RadicalCharacter.h"
#include "RadicalMovementComponent.h"

void UAnimNotifyState_Rotation::BranchingPointNotifyBegin(FBranchingPointNotifyPayload& BranchingPointPayload)
{
	Super::BranchingPointNotifyBegin(BranchingPointPayload);
	const auto MeshComp = BranchingPointPayload.SkelMeshComponent;
	
	if (PhysicsInfo[MeshComp].Character.IsValid())
	{
		PhysicsInfo[MeshComp].Character->GetCharacterMovement()->RequestSecondaryRotationBinding().BindUObject(this, &UAnimNotifyState_Rotation::UpdateRotation);
	}
}

void UAnimNotifyState_Rotation::BranchingPointNotifyEnd(FBranchingPointNotifyPayload& BranchingPointPayload)
{
	const auto MeshComp = BranchingPointPayload.SkelMeshComponent;

	if (PhysicsInfo.Contains(MeshComp) && PhysicsInfo[MeshComp].Character.IsValid())
	{
		PhysicsInfo[MeshComp].Character->GetCharacterMovement()->UnbindSecondaryRotation(this);
	}

	Super::BranchingPointNotifyEnd(BranchingPointPayload);
}

void UAnimNotifyState_Rotation::UpdateRotation(URadicalMovementComponent* MovementComponent, float DeltaTime)
{
	//const FRotator CurrentRot = MovementComponent->UpdatedComponent->GetComponentRotation();
}
