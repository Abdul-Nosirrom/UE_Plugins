// Copyright 2023 CoC All rights reserved

#pragma once

#include "CoreMinimal.h"
#include "ActionSystem/ActionScript.h"
#include "Helpers/PhysicsUtilities.h"
#include "ActionEvents_Physics.generated.h"

/**
 * 
 */
UCLASS(DisplayName="Movement Curve")
class ACTIONFRAMEWORK_API UActionEvent_MovementCurve : public UActionEvent
{
	GENERATED_BODY()
public:
	UActionEvent_MovementCurve() DECLARE_ACTION_EVENT("Movement Curve", false);

	UPROPERTY(VisibleAnywhere)
	mutable FString EditorStatusText;
	UPROPERTY(EditAnywhere)
	bool bEndActionWhenCurveCompletes = true;
	UPROPERTY(EditAnywhere)
	float TotalTime = 1.f;
	UPROPERTY(EditAnywhere, meta=(ShowOnlyInnerProperties))
	FCurveMovementParams Params;

	float CurrentTime;

	virtual void Initialize(UGameplayAction* InOwnerAction) override;
	virtual void ExecuteEvent() override;
	void CompleteEvent();

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif 
};
