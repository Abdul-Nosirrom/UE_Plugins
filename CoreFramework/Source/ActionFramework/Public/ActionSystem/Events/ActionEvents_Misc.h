// Copyright 2023 CoC All rights reserved

#pragma once

#include "CoreMinimal.h"
#include "ActionSystem/ActionScript.h"
#include "ActionEvents_Misc.generated.h"

class UNiagaraSystem;

UENUM()
enum class EActionFXSpawnType
{
	AtLocation,
	Attached
};

UENUM()
enum class EActionFXLength
{
	OneShot,
	Looping
};

UENUM()
enum class EActionEventFXAttachment
{
	Capsule,
	Mesh
};

UENUM()
enum class EActionEventVFXDeactivation
{
	None,
	AfterTime,
	OnEvent
};

USTRUCT()
struct FActionEventFXAttachmentRules
{
	GENERATED_BODY()

	FActionEventFXAttachmentRules() {}
	FActionEventFXAttachmentRules(bool bShouldSimplifyTransform) : bSimpleTransform(bShouldSimplifyTransform) {}

	UPROPERTY(EditDefaultsOnly)
	EActionFXSpawnType SpawnType;
	UPROPERTY(EditDefaultsOnly, meta=(EditCondition="SpawnType==EActionFXSpawnType::Attached", EditConditionHides))
	EActionEventFXAttachment AttachmentTarget;
	UPROPERTY(EditDefaultsOnly, meta=(EditCondition="SpawnType==EActionFXSpawnType::Attached && AttachmentTarget==EActionEventFXAttachment::Mesh", EditConditionHides))
	FName AttachSocket	= NAME_None;

	UPROPERTY(EditDefaultsOnly)
	FVector Location	= FVector::ZeroVector;
	UPROPERTY(EditDefaultsOnly, meta=(EditCondition="!bSimpleTransform", EditConditionHides))
	FRotator Rotation	= FRotator::ZeroRotator;
	UPROPERTY(EditDefaultsOnly, meta=(UIMin=0, ClampMin=0, EditCondition="!bSimpleTransform", EditConditionHides))
	FVector Scale		= FVector::OneVector;

	// just using this to hide stuff for a nicer editor experience
	UPROPERTY(VisibleDefaultsOnly, Transient, Category=AlwaysHidden, Meta=(EditCondition=False, EditConditionHides))
	bool bSimpleTransform = false;
};


UCLASS(DisplayName="SFX")
class ACTIONFRAMEWORK_API UActionEvent_PlaySFX : public UActionEvent
{
	GENERATED_BODY()
protected:
	UActionEvent_PlaySFX()
	{DECLARE_ACTION_EVENT("SFX", false)}

	UPROPERTY(EditDefaultsOnly, meta=(EditCondition="Type==EActionFXLength::OneShot"))
	TArray<FActionEventWrapper> AdditionalEventsToExecute;
	UPROPERTY(VisibleDefaultsOnly, DisplayName="")
	EActionFXLength Type;
	UPROPERTY(EditDefaultsOnly)
	USoundBase* SFX;
	UPROPERTY(EditDefaultsOnly)
	float VolumeMultiplier	= 1;
	UPROPERTY(EditDefaultsOnly)
	float PitchMultiplier	= 1;
	UPROPERTY(EditDefaultsOnly)
	float StartTime			= 0.f;
	UPROPERTY(EditDefaultsOnly, meta=(InlineEditConditionToggle))
	bool bSound2D;
	UPROPERTY(EditDefaultsOnly, meta=(EditCondition="!bSound2D", ShowOnlyInnerProperties))
	FActionEventFXAttachmentRules SpawnRules = FActionEventFXAttachmentRules(true);

	UPROPERTY(EditDefaultsOnly, meta=(EditCondition="Type==EActionFXLength::Looping", EditConditionHides))
	FActionEventWrapper PauseSFXOnEvent;
	UPROPERTY(EditDefaultsOnly, meta=(EditCondition="Type==EActionFXLength::Looping", EditConditionHides))
	FActionEventWrapper UnpauseSFXOnEvent;

	UPROPERTY(Transient)
	UAudioComponent* SpawnedSFX;
	
	virtual void Initialize(UGameplayAction* InOwnerAction) override;
	virtual void ExecuteEvent() override;

	void PauseSFX();
	void UnpauseSFX();
	void DestroySFX();

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual FString GetEditorFriendlyName() const override;
#endif 
};

/// @brief	Spawns a VFX system, can be one shot or looping
UCLASS(DisplayName="VFX")
class ACTIONFRAMEWORK_API UActionEvent_PlayVFX : public UActionEvent
{
	GENERATED_BODY()
protected:
	UActionEvent_PlayVFX()
	DECLARE_ACTION_EVENT("VFX", false);

	UPROPERTY(EditDefaultsOnly, DisplayName="")
	EActionFXLength Type;
	UPROPERTY(EditDefaultsOnly)
	UNiagaraSystem* VFX;
	UPROPERTY(EditDefaultsOnly)
	FActionEventFXAttachmentRules SpawnRules = FActionEventFXAttachmentRules(false);

	/// @brief	Will unattach the FX system after this time has passed. 0 to disable deparenting
	UPROPERTY(EditDefaultsOnly, meta=(EditCondition="Type==EActionFXLength::OneShot && EditorCacheSpawnType==EActionFXSpawnType::Attached", EditConditionHides, UIMin=0, ClampMin=0))
	float DeparentTime = -1.f;
	UPROPERTY(EditDefaultsOnly, meta=(EditCondition="Type==EActionFXLength::OneShot", EditConditionHides))
	EActionEventVFXDeactivation DeactivationMethod;
	UPROPERTY(EditDefaultsOnly, meta=(EditCondition="Type==EActionFXLength::OneShot && DeactivationMethod==EActionEventVFXDeactivation::AfterTime", EditConditionHides, UIMin=0, ClampMin=0))
	float DeactivationTime;
	UPROPERTY(EditDefaultsOnly, meta=(EditCondition="Type==EActionFXLength::OneShot && DeactivationMethod==EActionEventVFXDeactivation::OnEvent", EditConditionHides))
	FActionEventWrapper DeactivationEvent;

	UPROPERTY(EditDefaultsOnly, meta=(EditCondition="Type==EActionFXLength::Looping", EditConditionHides))
	FActionEventWrapper PauseVFXOnEvent;
	UPROPERTY(EditDefaultsOnly, meta=(EditCondition="Type==EActionFXLength::Looping", EditConditionHides))
	FActionEventWrapper UnpauseVFXOnEvent;

	
	UPROPERTY(Transient)
	class UNiagaraComponent* SpawnedFX;

	FTimerHandle DeparentHandle;
	FTimerHandle DeactivationHandle;

	
	virtual void Initialize(UGameplayAction* InOwnerAction) override;
	virtual void Cleanup() override;
	virtual void ExecuteEvent() override;


	void DestroySpawnedFX();
	void ReactivateSpawnedFX();
	void DeactivateSpawnedFX();

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	mutable EActionFXSpawnType EditorCacheSpawnType; // NOTE: Just copying this over from the struct for the EditCondition really
#endif 
	
#if WITH_EDITOR
	virtual FString GetEditorFriendlyName() const override;
#endif 
};
