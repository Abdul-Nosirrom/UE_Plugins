// Fill out your copyright notice in the Description page of Project Settings.


#include "Actions/Action_Interactable.h"

#include "RadicalMovementComponent.h"
#include "Components/InteractableComponent.h"
#include "Subsystems/InteractionManager.h"

void UAction_Interactable::OnActionActivated_Implementation()
{
	SetCanBeCanceled(false);
}

void UAction_Interactable::OnActionEnd_Implementation()
{
	if (Interactable)
	{
		//Interactable->StopInteraction(true);
	}
}

void UAction_Interactable::CalcVelocity_Implementation(URadicalMovementComponent* MovementComponent, float DeltaTime)
{
	if (!Interactable) return;

	if (Interactable->CalcPlayerVelocityEvent.IsBound())
		Interactable->CalcPlayerVelocityEvent.Broadcast(MovementComponent, DeltaTime);
	else
	{
		TGuardValue<bool> RestoreAccelRotates(MovementComponent->GetMovementData()->bAccelerationRotates, false);
		TGuardValue<float> RestoreTopSpeed(MovementComponent->GetMovementData()->TopSpeed, 600.f);
		MovementComponent->GetMovementData()->CalculateVelocity(MovementComponent, DeltaTime);
	}
}

void UAction_Interactable::UpdateRotation_Implementation(URadicalMovementComponent* MovementComponent, float DeltaTime)
{
	if (!Interactable) return;

	if (Interactable->UpdatePlayerRotationEvent.IsBound())
		Interactable->UpdatePlayerRotationEvent.Broadcast(MovementComponent, DeltaTime);
	else
	{
		MovementComponent->GetMovementData()->UpdateRotation(MovementComponent, DeltaTime);
	}
}

bool UAction_Interactable::EnterCondition_Implementation()
{
	return true;
}
