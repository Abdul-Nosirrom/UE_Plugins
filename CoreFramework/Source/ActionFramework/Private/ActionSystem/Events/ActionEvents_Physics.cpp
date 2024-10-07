// Copyright 2023 CoC All rights reserved


#include "ActionSystem/Events/ActionEvents_Physics.h"
#include "ActionSystem/GameplayAction.h"

void UActionEvent_MovementCurve::Initialize(UGameplayAction* InOwnerAction)
{
	Super::Initialize(InOwnerAction);

	OwnerAction->OnGameplayActionEnded.Event.AddLambda([this]
	{
		Params.DeInit(GetCharacterInfo()->MovementComponent.Get());
	});

	Params.CurveEvalComplete.BindUObject(this, &UActionEvent_MovementCurve::CompleteEvent);
}

void UActionEvent_MovementCurve::ExecuteEvent()
{
	Params.Init(GetCharacterInfo()->MovementComponent.Get(), TotalTime);
}

void UActionEvent_MovementCurve::CompleteEvent()
{
	if (bEndActionWhenCurveCompletes)
	{
		OwnerAction->SetCanBeCanceled(true);
		OwnerAction->CancelAction();
	}
}


#if WITH_EDITOR
EDataValidationResult UActionEvent_MovementCurve::IsDataValid(FDataValidationContext& Context) const
{
	EditorStatusText = Params.ValidateCurve(TotalTime);
	return Super::IsDataValid(Context);
}
#endif
