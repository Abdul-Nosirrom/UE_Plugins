// Copyright 2023 CoC All rights reserved


#include "Triggers/InteractionTrigger_OnDamaged.h"

#include "Components/InteractableComponent.h"
#include "Interfaces/DamageableInterface.h"
#include "Misc/DataValidation.h"

void UInteractionTrigger_OnDamaged::Initialize(UInteractableComponent* OwnerInteractable)
{
	if (auto DamageInterface = Cast<IDamageableInterface>(InteractableOwner->GetOwner()))
	{
		DamageInterface->OnGotHitDelegate.AddLambda([this](AActor* Victim)
		{
			InteractableOwner->InitializeInteraction();
		});
	}
}

#if WITH_EDITOR 

EDataValidationResult UInteractionTrigger_OnDamaged::IsDataValid(FDataValidationContext& Context)
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	InteractableOwner = Cast<UInteractableComponent>(GetOuter());
	if (!InteractableOwner || !InteractableOwner->GetOwner()) return Result;
	
	if (InteractableOwner->GetOwner()->Implements<UDamageableInterface>())
	{
		FText CurrentError = FText::FormatOrdered(FTextFormat::FromString("[{0}] Owner does not implement IDamageableInterface!"), FText::FromString(InteractableOwner->GetOwner()->GetName()));
		Context.AddError(CurrentError);
		Result =  EDataValidationResult::Invalid;
	}
	

	return Result;
}

#endif 