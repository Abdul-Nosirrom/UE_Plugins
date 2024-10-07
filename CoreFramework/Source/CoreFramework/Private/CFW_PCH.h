// Copyright 2023 Abdulrahmen Almodaimegh. All Rights Reserved.
#pragma once

#include "AI/Navigation/AvoidanceManager.h"
#include "AI/Navigation/NavigationAvoidanceTypes.h"
#include "AI/Navigation/NavigationDataInterface.h"
#include "AI/Navigation/NavigationTypes.h"
#include "AI/RVOAvoidanceInterface.h"
#include "AIController.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimationAsset.h"

#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "Components/ArrowComponent.h"
#include "Components/SkeletalMeshComponent.h"

#include "CoreMinimal.h"

#include "DrawDebugHelpers.h"
#include "DisplayDebugHelpers.h"

#include "GameplayTags.h"

#include "InputMappingContext.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"

#include "Engine/ActorChannel.h"
#include "Engine/Engine.h"
#include "Engine/GameEngine.h"
#include "Engine/GameInstance.h"
#include "Engine/ScopedMovementUpdate.h"
#include "Engine/Canvas.h"
#include "Engine/LocalPlayer.h"

#include "Curves/CurveFloat.h"

#include "GameFramework/Actor.h"
#include "GameFramework/Controller.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/InputSettings.h"
#include "GameFramework/MovementComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PawnMovementComponent.h"
#include "GameFramework/PhysicsVolume.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerInput.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/SpringArmComponent.h"

#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet/KismetMathLibrary.h"

#include "Misc/EngineVersionComparison.h"
#include "PhysicsEngine/BodySetup.h"
#include "PhysicsEngine/SphylElem.h"

#include "PrimitiveSceneProxy.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"
#include <cmath>
#include <type_traits>
