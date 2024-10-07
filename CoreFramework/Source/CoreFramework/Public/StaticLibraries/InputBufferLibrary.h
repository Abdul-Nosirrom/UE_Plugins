// Copyright 2023 CoC All rights reserved

#pragma once

#include "CoreMinimal.h"
#include "InputBufferSubsystem.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "InputBufferLibrary.generated.h"

/**
 * 
 */
UCLASS()
class COREFRAMEWORK_API UInputBuffer : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(Category="InputBuffer", ScriptCallable)
	static UInputBufferSubsystem* Get(const AActor* Context)
	{
		if (!Context) return nullptr;
		
		if (const APlayerController* PlayerController = Cast<APlayerController>(Context->GetInstigatorController()))
		{
			return ULocalPlayer::GetSubsystem<UInputBufferSubsystem>(PlayerController->GetLocalPlayer());
		}
		return nullptr;
	}

	UFUNCTION(Category="InputBuffer", ScriptCallable)
	static bool CanPressInput(const AActor* Context, const FGameplayTag& InputTag)
	{
		UInputBufferSubsystem* IB = Get(Context);
		if (!IB) return false;

		return IB->CanPressInput(InputTag);
	}

	UFUNCTION(Category="InputBuffer", ScriptCallable)
	static bool ConsumeInput(const AActor* Context, const FGameplayTag& InputTag)
	{
		UInputBufferSubsystem* IB = Get(Context);
		if (!IB) return false;

		return IB->ConsumeInput(InputTag);
	}
};
