// 


#include "Interfaces/ActionStateAnimation.h"

#include "Animation/AnimInstance.h"
#include "ActionSystem/GameplayAction.h"


// Add default functionality here for any IAnimationState functions that are not pure virtual.
void IActionStateAnimation::PlayAnimMontage(UGameplayAction* ActionInstance, USkeletalMeshComponent* Mesh, UAnimMontage* Montage, float PlayRate, float StartingPosition, FName StartingSection)
{
	if (UAnimInstance* AnimInstance = Mesh->GetAnimInstance())
	{
		const float MontageLength = AnimInstance->Montage_Play(Montage, PlayRate, EMontagePlayReturnType::Duration, StartingPosition);

		if (MontageLength > 0.f) // If montage played successfully
		{
			AnimInstancePtr = AnimInstance;
			ActionInstancePtr = ActionInstance;
			
			if (const FAnimMontageInstance* MontageInstance = AnimInstance->GetActiveInstanceForMontage(Montage))
			{
				MontageInstanceID = MontageInstance->GetInstanceID();
			}

			if (StartingSection != NAME_None)
			{
				AnimInstance->Montage_JumpToSection(StartingSection, Montage);
			}

			/* Bind Events */
	
			BlendingOutDelegate.BindRaw(this, &IActionStateAnimation::OnBlendingOut);
			AnimInstance->Montage_SetBlendingOutDelegate(BlendingOutDelegate, Montage);

			MontageEndedDelegate.BindRaw(this, &IActionStateAnimation::OnMontageEnded);
			AnimInstance->Montage_SetEndDelegate(MontageEndedDelegate, Montage);

			AnimInstance->OnPlayMontageNotifyBegin.AddDynamic(this, &IActionStateAnimation::OnNotifyBeginReceived);
			AnimInstance->OnPlayMontageNotifyEnd.AddDynamic(this, &IActionStateAnimation::OnNotifyEndReceived);
		}
	}
	
}


bool IActionStateAnimation::IsNotifyValid(FName NotifyName, const FBranchingPointNotifyPayload& BranchingPointNotifyPayload) const
{
	return ((MontageInstanceID != INDEX_NONE) && (BranchingPointNotifyPayload.MontageInstanceID == MontageInstanceID));
}

void IActionStateAnimation::UnbindDelegates()
{
	if (UGameplayAction* StateInstance = ActionInstancePtr.Get())
	{
		if (!Cast<IActionStateAnimation>(StateInstance)) return;
		
		if (UAnimInstance* AnimInstance = AnimInstancePtr.Get())
		{
			AnimInstance->OnPlayMontageNotifyBegin.RemoveAll(StateInstance);
			AnimInstance->OnPlayMontageNotifyEnd.RemoveAll(StateInstance);
		}
	}
}
