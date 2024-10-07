// Copyright 2023 CoC All rights reserved


#include "Animation/AnimNotifyState_ConditionalSectionChange.h"

#include "RadicalCharacter.h"
#include "RadicalMovementComponent.h"
#include "ActionSystem/ActionScript.h"
#include "ActionSystem/ActionSystemInterface.h"
#include "Components/ActionSystemComponent.h"


void UAnimNotifyState_ConditionalSectionChange::BranchingPointNotifyBegin(
	FBranchingPointNotifyPayload& BranchingPointPayload)
{
	Super::BranchingPointNotifyBegin(BranchingPointPayload);
	//Condition->SetupCondition(ActionSystemComp);
}

void UAnimNotifyState_ConditionalSectionChange::BranchingPointNotifyEnd(
	FBranchingPointNotifyPayload& BranchingPointPayload)
{
	Super::BranchingPointNotifyEnd(BranchingPointPayload);
	//Condition->CleanupCondition(ActionSystemComp);
}

void UAnimNotifyState_ConditionalSectionChange::BranchingPointNotifyTick(
	FBranchingPointNotifyPayload& BranchingPointPayload, float FrameDeltaTime)
{
	if (auto IAS = Cast<IActionSystemInterface>(BranchingPointPayload.SkelMeshComponent->GetOwner()))
	{
		auto AS = IAS->GetActionSystem(); 
		Condition->SetCharacterInfo(&AS->ActionActorInfo);
		if (Condition->DoesConditionPass()) 
		{
			const auto MonIn = BranchingPointPayload.SkelMeshComponent->GetAnimInstance()->GetMontageInstanceForID(BranchingPointPayload.MontageInstanceID);
			BranchingPointPayload.SkelMeshComponent->GetAnimInstance()->Montage_JumpToSection(TargetSection, MonIn->Montage); // NOTE: Maybe give the option to not *jump* but instead just setting the next section, allowing current one to finish first
		}
	}
}

#if WITH_EDITOR
FString UAnimNotifyState_ConditionalSectionChange::GetNotifyName_Implementation() const
{
	FString Name = "Conditional Section Change To "; Name.Append(TargetSection.ToString());
	if (Condition)
	{
		Name = FString::Printf(TEXT("Section Change To %s When %s"), *TargetSection.ToString(), *Condition->GetName());
	}
	return Name;
}
#endif 
