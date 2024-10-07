// Copyright 2023 CoC All rights reserved

#pragma once

#include "CoreMinimal.h"
#include "InteractionBehavior.h"
#include "MovieSceneSequencePlaybackSettings.h"
#include "Components/InterpToMovementComponent.h"
#include "Engine/TargetPoint.h"
#include "InteractionBehaviors_Misc.generated.h"


UCLASS(DisplayName="Spawn VFX", MinimalAPI)
class UInteractionBehavior_SpawnVFX : public UInteractionBehavior
{
	GENERATED_BODY()
protected:
	UPROPERTY(EditAnywhere)
	class UNiagaraSystem* VFX;
	UPROPERTY(EditAnywhere)
	ATargetPoint* SpawnLocation;

	virtual void ExecuteInteraction(EInteractionDomain Domain, float DeltaTime) override;

#if WITH_EDITOR 
	virtual TArray<AActor*> GetWorldReferences() const override { return {SpawnLocation};}
#endif
};

UCLASS(DisplayName="Spawn SFX", MinimalAPI)
class UInteractionBehavior_SpawnSFX : public UInteractionBehavior
{
	GENERATED_BODY()
protected:
	UPROPERTY(EditAnywhere)
	class USoundBase* Sound;
	UPROPERTY(EditAnywhere, meta=(UIMin=0, ClampMin=0))
	float VolumeMultiplier = 1.f;
	UPROPERTY(EditAnywhere, meta=(EditCondition=bSound2D))
	ATargetPoint* SoundLocation;
	UPROPERTY(EditAnywhere, meta=(InlineEditConditionToggle))
	bool bSound2D;

	virtual void ExecuteInteraction(EInteractionDomain Domain, float DeltaTime) override;

#if WITH_EDITOR 
	virtual TArray<AActor*> GetWorldReferences() const override
	{
		if (bSound2D) return {};
		return {SoundLocation};
	}
#endif 
};

UCLASS(DisplayName="Fade To Black", MinimalAPI)
class UInteractionBehavior_FadeToBlack : public UInteractionBehavior
{
	GENERATED_BODY()

protected:
	/// @brief  Time it takes to fade to black and then after hold time is over, fade out. Duration of the "fade"
	UPROPERTY(EditAnywhere, meta=(UIMin=0, ClampMin=0))
	float FadeTime = 0.5f;
	/// @brief  How long to be in "Blackness"
	UPROPERTY(EditAnywhere, meta=(UIMin=0, ClampMin=0))
	float HoldTime = 1.f;

	
	/// @brief  Player will be teleported to this location during the time the screen is fully faded.
	UPROPERTY(EditAnywhere, meta=(EditCondition=bShouldTeleportPlayer))
	ATargetPoint* PlayerTeleportTransform;
	UPROPERTY(EditAnywhere, meta=(InlineEditConditionToggle))
	bool bShouldTeleportPlayer;

	bool bDoneExecuting;

	virtual void InteractionStarted() override { Super::InteractionStarted(); bDoneExecuting = false; }
	
	virtual void ExecuteInteraction(EInteractionDomain Domain, float DeltaTime) override;

#if WITH_EDITOR 
	virtual TArray<AActor*> GetWorldReferences() const override
	{
		if (!bShouldTeleportPlayer) return {};
		return {PlayerTeleportTransform};
	}
#endif 
};

UCLASS(DisplayName="Level Sequencer", MinimalAPI)
class UInteractionBehavior_LevelSequencer : public UInteractionBehavior
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere)
	bool bWaitUntilComplete = false;
	UPROPERTY(EditAnywhere)
	class ULevelSequence* Sequence;
	UPROPERTY(EditAnywhere)
	FMovieSceneSequencePlaybackSettings PlaybackSettings;

	bool bDoneExecuting;

	UPROPERTY(Transient)
	class ULevelSequencePlayer* SequencePlayer;
	UPROPERTY(Transient)
	class ALevelSequenceActor* SequenceActor; 

	virtual void Initialize(UInteractableComponent* OwnerInteractable) override;
	virtual void ExecuteInteraction(EInteractionDomain Domain, float DeltaTime) override;
	UFUNCTION()
	void OnSequenceComplete()
	{
		if (bWaitUntilComplete) CompleteBehavior();
	}

#if WITH_EDITOR
	virtual EDataValidationResult IsBehaviorValid(EInteractionDomain InteractionDomain, FDataValidationContext& Context) override;
#endif 
};
