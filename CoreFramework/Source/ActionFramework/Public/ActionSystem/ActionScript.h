// Copyright 2023 CoC All rights reserved

#pragma once

#include "CoreMinimal.h"
#include "GameplayAction.h"
#include "UObject/Object.h"
#include "ActionScript.generated.h"


/* Helper macro */
#if WITH_EDITORONLY_DATA
#define DECLARE_ACTION_SCRIPT(Name) \
	{\
	EditorFriendlyName = Name;\
	}\
	
#define DECLARE_ACTION_EVENT(Name, bHideEvent) \
	{\
	EditorFriendlyName = Name;\
	bIgnoreEvent = bHideEvent;\
	}

	//if (!bIgnoreEvent && !GetOuter()->IsA<UGameplayAction>()) bIgnoreEvent = true;\
	//}\

#else

#define DECLARE_ACTION_SCRIPT(Name) \
	{}
	
#define DECLARE_ACTION_EVENT(Name, bHideEvent) \
	{\
	bIgnoreEvent = bHideEvent;\
	if (!bIgnoreEvent && !GetOuter()->IsA<UGameplayAction>()) bIgnoreEvent = true;\
	}\

#endif

UCLASS(Blueprintable, EditInlineNew, DefaultToInstanced, Abstract)
class ACTIONFRAMEWORK_API UActionScript : public UObject
{
	GENERATED_BODY()

protected:
	TWeakObjectPtr<UGameplayAction> OwnerAction;
	mutable const FActionActorInfo* CurrentActorInfo;
	FTimerHandle RetryTimerHandle;
	
public:
	virtual void Initialize(UGameplayAction* InOwnerAction);
	virtual void Cleanup();

	const FActionActorInfo* GetCharacterInfo() const { return CurrentActorInfo; }
	void SetCharacterInfo(const FActionActorInfo* InActorInfo) const { CurrentActorInfo = InActorInfo; } 

	int32 GetPlayerIndex() const { return PlayerIndex; }

private:
	int32 PlayerIndex;

public:
	
#if WITH_EDITORONLY_DATA
	/// @brief	Friendly name for displaying in the Editor's Scripts Index. We set EditCondition False here so it doesn't show up otherwise. */
	UPROPERTY(VisibleDefaultsOnly, ScriptReadWrite, Transient, Category=AlwaysHidden, Meta=(EditCondition=False, EditConditionHides))
	mutable FString EditorFriendlyName;
#endif

#if WITH_EDITOR
	virtual FString GetEditorFriendlyName() const { return EditorFriendlyName; }
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override
	{
		EditorFriendlyName = GetEditorFriendlyName();
		return EDataValidationResult::NotValidated;
	}
#endif 
};


UCLASS(Abstract, CollapseCategories)
class ACTIONFRAMEWORK_API UActionTrigger : public UActionScript
{
	GENERATED_BODY()

public:
	virtual void SetupTrigger(int32 Priority) {}
	virtual void CleanupTrigger() {}
};

/// @brief	This trigger implies that the action is never registered and can only be activated if explicitly requested somewhere
UCLASS(DisplayName="External Trigger")
class UActionTrigger_ExternalTrigger : public UActionTrigger
{
	GENERATED_BODY()
protected:
	
	UActionTrigger_ExternalTrigger()
	{ DECLARE_ACTION_SCRIPT("External Trigger") }
};

UCLASS(Abstract, CollapseCategories)
class ACTIONFRAMEWORK_API UActionCondition : public UActionScript
{
	GENERATED_BODY()

public:
	virtual bool DoesConditionPass() { return true; }
};

UCLASS(Abstract, CollapseCategories)
class ACTIONFRAMEWORK_API UActionEvent : public UActionScript
{
	GENERATED_BODY()

public:
	
	UPROPERTY(Category=Event, EditDefaultsOnly, meta=(DisplayPriority=-1, EditCondition="!bIgnoreEvent", EditConditionHides))
	FActionEventWrapper Event;
	
	UPROPERTY(VisibleDefaultsOnly, Transient, Category=AlwaysHidden, Meta=(EditCondition=False, EditConditionHides))
	bool bIgnoreEvent = false;

	virtual void Initialize(UGameplayAction* InOwnerAction) override;

	virtual void ExecuteEvent() { ExecuteScriptEvent(GetCharacterInfo()->CharacterOwner.Get()); }

	UFUNCTION(BlueprintImplementableEvent)
	void ExecuteScriptEvent(class ARadicalCharacter* Owner);
	
};
