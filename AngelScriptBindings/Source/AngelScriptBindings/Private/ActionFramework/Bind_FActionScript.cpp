#pragma once
#include "CoreMinimal.h"
#include "AngelscriptBinds.h"
#include "AngelscriptManager.h"
#include "ActionSystem/GameplayActionTypes.h"

AS_FORCE_LINK const FAngelscriptBinds::FBind Bind_FDelegateHandle(FAngelscriptBinds::EOrder::Late, []
{
	FBindFlags bindflags;
	auto FDelegateHandle_ = FAngelscriptBinds::ValueClass<FDelegateHandle>("FDelegateHandle", bindflags);

	FDelegateHandle_.Constructor("void f()", [](FDelegateHandle* Address)
	{
		new(Address)FDelegateHandle();
	});
	FAngelscriptBinds::SetPreviousBindNoDiscard(true);
	SCRIPT_TRIVIAL_NATIVE_CONSTRUCTOR(FDelegateHandle_, "FDelegateHandle");

	FDelegateHandle_.Method("bool IsValid() const", METHODPR_TRIVIAL(void, FDelegateHandle, IsValid, () const));
	FDelegateHandle_.Method("void Reset()", METHODPR_TRIVIAL(void, FDelegateHandle, Reset, ()));
});

AS_FORCE_LINK const FAngelscriptBinds::FBind Bind_FActionScript(FAngelscriptBinds::EOrder::Late, []
{
	auto FActionScriptEvent_ = FAngelscriptBinds::ExistingClass("FActionScriptEvent");
	
	FActionScriptEvent_.Method("FDelegateHandle Bind(UObject object, FName function_name)", [](FActionScriptEvent* actionEvent, UObject* object, FName function_name)
	{
		// Verify function name exists on the class
		if (object)
		{
			UFunction* func = object->FindFunction(function_name);
			if (!func || func->NumParms != 0)
			{
				FAngelscriptManager::Throw("No matching function signature found for CurveEvalCompleted [signature void f()]");
				return FDelegateHandle();
			}
			else
			{
				return actionEvent->Event.AddUFunction(object, function_name);
			}
		}
		return FDelegateHandle();
	});
	
	FActionScriptEvent_.Method("void UnBindAll(UObject object)", [](FActionScriptEvent* actionEvent, UObject* object)
	{
		return actionEvent->Event.RemoveAll(object);
	});
	FActionScriptEvent_.Method("void UnBind(FDelegateHandle handle)", [](FActionScriptEvent* actionEvent, FDelegateHandle handle)
	{
		return actionEvent->Event.Remove(handle);
	});
});