// 

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimMontage.h"
#include "UObject/Interface.h"
#include "ActionStateAnimation.generated.h"

/*~~~~~ Forward Declarations ~~~~~*/
class UAnimMontage;
class ARadicalCharacter;
class UGameplayAction;
struct FBranchingPointNotifyPayload;

// This class does not need to be modified.
UINTERFACE()
class UActionStateAnimation : public UInterface
{
	GENERATED_BODY()
};


class ACTIONFRAMEWORK_API IActionStateAnimation
{
	GENERATED_BODY()

	// Add interface functions to this class. This is the class that will be inherited to implement this interface.
protected:

	UFUNCTION()
	virtual void PlayAnimMontage(UGameplayAction* ActionInstance, USkeletalMeshComponent* Mesh, UAnimMontage* Montage, float PlayRate = 1.f, float StartingPosition = 0.f, FName StartingSection = NAME_None);

	// BEGIN Callback Events (Not pure virtual, don't need to force an implementation if not needed)
	/// @brief  Default implementation does some cleanup so be sure to call the Super
	UFUNCTION()
	virtual void OnMontageEnded(UAnimMontage* Montage, bool bInterrupted) { UnbindDelegates(); };
	UFUNCTION()
	virtual void OnBlendingOut(UAnimMontage* Montage, bool bInterrupted) {};
	UFUNCTION()
	virtual void OnNotifyBeginReceived(FName NotifyName, const FBranchingPointNotifyPayload& BranchingPointNotifyPayload) {};
	UFUNCTION()
	virtual void OnNotifyEndReceived(FName NotifyName, const FBranchingPointNotifyPayload& BranchingPointNotifyPayload) {};
	// END Callback Events

	// BEGIN Util
	
	/// @brief  Checks whether the notify is valid for the current montage. No need to be overridden.
	bool IsNotifyValid(FName NotifyName, const FBranchingPointNotifyPayload& BranchingPointNotifyPayload) const;
	
	// END Util

	TWeakObjectPtr<UAnimInstance> AnimInstancePtr;
private:
	TWeakObjectPtr<UGameplayAction> ActionInstancePtr;
	int32 MontageInstanceID{0};

	// BEGIN Event Delegates
	FOnMontageEnded MontageEndedDelegate;
	FOnMontageBlendingOutStarted BlendingOutDelegate;
	// END Event Delegates

	void UnbindDelegates();
};
