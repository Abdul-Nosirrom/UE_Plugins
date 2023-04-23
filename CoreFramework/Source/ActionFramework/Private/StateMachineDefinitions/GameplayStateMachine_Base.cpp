// Fill out your copyright notice in the Description page of Project Settings.


#include "StateMachineDefinitions/GameplayStateMachine_Base.h"
#include "Actors/RadicalPlayerCharacter.h"
#include "Components/ActionManagerComponent.h"
#include "StateMachineDefinitions/Action_CoreStateInstance.h"


void UGameplayStateMachine_Base::InitializeActions(ARadicalPlayerCharacter* Character, UActionManagerComponent* SetComponent)
{
	RadicalOwner = Character;
	Component = SetComponent;
	Initialize(SetComponent->GetOwner());
}

void UGameplayStateMachine_Base::OnStateMachineStart_Implementation()
{
	Super::OnStateMachineStart_Implementation();
}

void UGameplayStateMachine_Base::OnStateMachineStateStarted_Implementation(const FSMStateInfo& State)
{
	Super::OnStateMachineStateStarted_Implementation(State);
}
