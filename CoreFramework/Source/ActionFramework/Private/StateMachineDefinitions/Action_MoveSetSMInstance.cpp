// 


#include "StateMachineDefinitions/Action_MoveSetSMInstance.h"

#include "ImageUtils.h"
#include "Interfaces/IPluginManager.h"
#include "StateMachineDefinitions/GameplayStateMachine_Base.h"


UAction_MoveSetSMInstance::UAction_MoveSetSMInstance()
{
#if WITH_EDITOR
	/* Establish Connection Rules */

	////////////////////////////////
	/// 1.) Entry->This (The others implicitly allow this, but this class has hard-set transitions)
	/// 2.) This->This
	////////////////////////////////
	
	/* Define Base Data */
	FSMStateClassRule MoveSetRule;
	FSMStateClassRule EntryStateRule;
	FSMStateMachineClassRule GameplayStateMachineRule;

	MoveSetRule.StateClass = TSoftClassPtr<USMStateInstance_Base>(StaticClass());
	MoveSetRule.bIncludeChildren = true;

	EntryStateRule.StateClass = TSoftClassPtr<USMStateInstance_Base>(USMEntryStateInstance::StaticClass());
	EntryStateRule.bIncludeChildren = true;
	
	GameplayStateMachineRule.StateMachineClass = TSoftClassPtr<USMStateMachineInstance>(UGameplayStateMachine_Base::StaticClass());
	GameplayStateMachineRule.bIncludeChildren = true;

	/* Set the rules */
	ConnectionRules.AllowedInboundStates = {EntryStateRule, MoveSetRule};
	ConnectionRules.AllowedOutboundStates = {MoveSetRule};
	ConnectionRules.AllowedInStateMachines = {GameplayStateMachineRule};
	bHideFromContextMenuIfRulesFail = true;

	/* Don't wait for end state, doesn't make sense for this type of state machine */
	SetWaitForEndState(false);
#endif
}

void UAction_MoveSetSMInstance::ConstructionScript_Implementation()
{
	/* Set Visuals */
	if (!NodeIcon)
	{
		static FString ContentDir = IPluginManager::Get().FindPlugin(TEXT("CoreFramework"))->GetBaseDir() / TEXT("Resources");
		static FString IconDir = (ContentDir / TEXT("moveseticon")) + TEXT(".png");
		const auto Icon = FImageUtils::ImportFileAsTexture2D(IconDir);
		SetUseCustomIcon(true);
		NodeIcon = Icon;
		NodeIconSize = FVector2D(100,100);
	}
#pragma region Input Construction
	
	bool bHideFirstButton = false, bHideSecondButton = false, bHideDirectional = false;
	bool bHideSequenceOrder = false, bHideTriggerEvent = false, bHideConsiderOrder = false;
	
	switch (InputType)
	{
		case None:
			bHideFirstButton = true;
		bHideSecondButton = true;
		bHideDirectional = true;
		bHideSequenceOrder = true;
		bHideTriggerEvent = true;
		bHideConsiderOrder = true;
		break;
		case Button:
			bHideSecondButton = true;
		bHideDirectional = true;
		bHideSequenceOrder = true;
		bHideConsiderOrder = true;
		break;
		case ButtonSequence:
			bHideDirectional = true;
		bHideSequenceOrder = true;
		bHideTriggerEvent = true;
		break;
		case Directional:
			bHideFirstButton = true;
		bHideSecondButton = true;
		bHideSequenceOrder = true;
		bHideConsiderOrder = true;
		bHideTriggerEvent = true;
		break;
		case DirectionButtonSequence:
			bHideSecondButton = true;
		bHideConsiderOrder = true;
		bHideTriggerEvent = true;
		break;
	}
		
	SetVariableHidden(GET_MEMBER_NAME_CHECKED(UAction_MoveSetSMInstance, ButtonInput), bHideFirstButton);
	SetVariableHidden(GET_MEMBER_NAME_CHECKED(UAction_MoveSetSMInstance, SecondButtonInput), bHideSecondButton);
	SetVariableHidden(GET_MEMBER_NAME_CHECKED(UAction_MoveSetSMInstance, DirectionalInput), bHideDirectional);

	SetVariableHidden(GET_MEMBER_NAME_CHECKED(UAction_MoveSetSMInstance, bConsiderOrder), bHideConsiderOrder);
	SetVariableHidden(GET_MEMBER_NAME_CHECKED(UAction_MoveSetSMInstance, SequenceOrder), bHideSequenceOrder);
	SetVariableHidden(GET_MEMBER_NAME_CHECKED(UAction_MoveSetSMInstance, TriggerEvent), bHideTriggerEvent);

#pragma endregion Input Construction
	
	Super::ConstructionScript_Implementation();
}
