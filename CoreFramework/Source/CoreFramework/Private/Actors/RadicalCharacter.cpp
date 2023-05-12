// Copyright 2023 Abdulrahmen Almodaimegh. All Rights Reserved.

#include "RadicalCharacter.h"
#include "CFW_PCH.h"
#include "RadicalMovementComponent.h"
#include "InputBufferSubsystem.h"
#include "Debug/CFW_LOG.h"


/* Define default component object names */
FName ARadicalCharacter::MeshComponentName(TEXT("Character Mesh"));
FName ARadicalCharacter::MovementComponentName(TEXT("Movement Component"));
FName ARadicalCharacter::CapsuleComponentName(TEXT("Collision Component"));

// Sets default values
ARadicalCharacter::ARadicalCharacter(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FName ID_Characters;
		FText NAME_Characters;
		FConstructorStatics()
			: ID_Characters(TEXT("Characters"))
			, NAME_Characters(NSLOCTEXT("SpriteCategory", "Characters", "Characters"))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

	
	// Set this pawn to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	/* ~~~~~ Setup and attach primary components ~~~~~ */

	// Capsule Component
	CapsuleComponent = CreateDefaultSubobject<UCapsuleComponent>(ARadicalCharacter::CapsuleComponentName);
	CapsuleComponent->InitCapsuleSize(34.0f, 88.0f);
	CapsuleComponent->SetCollisionProfileName(UCollisionProfile::Pawn_ProfileName);

	CapsuleComponent->CanCharacterStepUpOn = ECB_No;
	CapsuleComponent->SetShouldUpdatePhysicsVolume(true);
	CapsuleComponent->SetCanEverAffectNavigation(false);
	CapsuleComponent->bDynamicObstacle = true;
	RootComponent = CapsuleComponent;

	// Arrow Debug Component
#if WITH_EDITORONLY_DATA
	ArrowComponent = CreateEditorOnlyDefaultSubobject<UArrowComponent>(TEXT("Arrow"));
	if (ArrowComponent)
	{
		ArrowComponent->ArrowColor = FColor(150, 200, 255);
		ArrowComponent->bTreatAsASprite = true;
		ArrowComponent->SpriteInfo.Category = ConstructorStatics.ID_Characters;
		ArrowComponent->SpriteInfo.DisplayName = ConstructorStatics.NAME_Characters;
		ArrowComponent->SetupAttachment(CapsuleComponent);
		ArrowComponent->bIsScreenSizeScaled = true;
		ArrowComponent->SetSimulatePhysics(false);
	}
#endif // WITH_EDITORONLY_DATA

	
	// Movement Component
	MovementComponent = CreateDefaultSubobject<URadicalMovementComponent>(ARadicalCharacter::MovementComponentName);
	if (MovementComponent)
	{
		MovementComponent->UpdatedComponent = CapsuleComponent;
		MovementComponent->CharacterOwner = this;
	}
	
	
	// Mesh Component
	Mesh = CreateOptionalDefaultSubobject<USkeletalMeshComponent>(ARadicalCharacter::MeshComponentName);
	if (Mesh)
	{
		Mesh->AlwaysLoadOnClient = true;
		Mesh->AlwaysLoadOnServer = true;
		Mesh->bOwnerNoSee = false;
		Mesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPose;
		Mesh->bCastDynamicShadow = true;
		Mesh->bAffectDynamicIndirectLighting = true;
		Mesh->PrimaryComponentTick.TickGroup = TG_PrePhysics;
		Mesh->SetupAttachment(CapsuleComponent);
		static FName MeshCollisionProfileName(TEXT("CharacterMesh"));
		Mesh->SetCollisionProfileName(MeshCollisionProfileName);
		Mesh->SetGenerateOverlapEvents(false);
		Mesh->SetCanEverAffectNavigation(false);
	}

	/* ~~~~~ Setup Default Values ~~~~~ */
	AnimRootMotionTranslationScale = 1.f;
}

#pragma region AActor & UObject Interface

UInputBufferSubsystem* ARadicalCharacter::GetInputBuffer() const
{
	if (const APlayerController* PlayerController = Cast<APlayerController>(Controller))
	{
		return ULocalPlayer::GetSubsystem<UInputBufferSubsystem>(PlayerController->GetLocalPlayer());
	}
	return nullptr;
}

void ARadicalCharacter::PostLoad()
{
	Super::PostLoad();

#if WITH_EDITORONLY_DATA
	if (ArrowComponent)
	{
		ArrowComponent->SetSimulatePhysics(false);
	}
#endif // WITH_EDITORONLY_DATA
}

void ARadicalCharacter::BeginPlay()
{
	Super::BeginPlay();
	//bHasBasedMovementOverride = false;
	MovementComponent->CharacterOwner = this; // NOTE: For instance
}

void ARadicalCharacter::TickActor(float DeltaTime, ELevelTick TickType, FActorTickFunction& ThisTickFunction)
{
	Super::TickActor(DeltaTime, TickType, ThisTickFunction);
}


// TODO: This
void ARadicalCharacter::ClearCrossLevelReferences()
{
	// Clear base
	if( BasedMovement.MovementBase != nullptr && GetOutermost() != BasedMovement.MovementBase->GetOutermost() )
	{
		SetBase( nullptr );
	}
	// Clear base override
	if (BasedMovementOverride.MovementBase != nullptr && GetOutermost() != BasedMovementOverride.MovementBase->GetOutermost())
	{
		RemoveBaseOverride();
	}
	
	Super::ClearCrossLevelReferences();
}

void ARadicalCharacter::GetSimpleCollisionCylinder(float& CollisionRadius, float& CollisionHalfHeight) const
{
	if (RootComponent == CapsuleComponent && IsRootComponentCollisionRegistered())
	{
		// Note: we purposefully ignore the component transform here aside from scale, always treating it as vertically aligned.
		// This improves performance and is also how we stated the CapsuleComponent would be used.
		CapsuleComponent->GetScaledCapsuleSize(CollisionRadius, CollisionHalfHeight);
	}
	else
	{
		Super::GetSimpleCollisionCylinder(CollisionRadius, CollisionHalfHeight);
	}
}

UActorComponent* ARadicalCharacter::FindComponentByClass(const TSubclassOf<UActorComponent> ComponentClass) const
{
	// If the character has a Mesh, treat it as the first 'hit' when finding components
	if (Mesh && ComponentClass && Mesh->IsA(ComponentClass))
	{
		return Mesh;
	}

	return Super::FindComponentByClass(ComponentClass);
}

// TODO: This
void ARadicalCharacter::NotifyActorBeginOverlap(AActor* OtherActor)
{
	Super::NotifyActorBeginOverlap(OtherActor);
}

// TODO: This
void ARadicalCharacter::NotifyActorEndOverlap(AActor* OtherActor)
{
	Super::NotifyActorEndOverlap(OtherActor);
}

#pragma endregion AActor & UObject Interface


#pragma region APawn Interface

void ARadicalCharacter::PostInitializeComponents()
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_Character_PostInitComponents);

	Super::PostInitializeComponents();
	
	if (IsValid(this))
	{
		if (Mesh)
		{
			//CacheInitialMeshOffset(Mesh->GetRelativeLocation(), Mesh->GetRelativeRotation());

			// force animation tick after movement component updates
			if (Mesh->PrimaryComponentTick.bCanEverTick && MovementComponent)
			{
				Mesh->PrimaryComponentTick.AddPrerequisite(MovementComponent, MovementComponent->PrimaryComponentTick);
			}
		}

		if (MovementComponent && CapsuleComponent)
		{
			MovementComponent->UpdateNavAgent(*CapsuleComponent);
		}

	}
}

UPawnMovementComponent* ARadicalCharacter::GetMovementComponent() const
{
	return MovementComponent;
}

UPrimitiveComponent* ARadicalCharacter::GetMovementBase() const
{
	return GetBasedMovement().MovementBase;
}


// Called to bind functionality to input
void ARadicalCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	check(PlayerInputComponent)
}

float ARadicalCharacter::GetDefaultHalfHeight() const
{
	UCapsuleComponent* DefaultCapsule = GetClass()->GetDefaultObject<ARadicalCharacter>()->CapsuleComponent;
	if (DefaultCapsule)
	{
		return DefaultCapsule->GetScaledCapsuleHalfHeight();
	}
	else
	{
		return Super::GetDefaultHalfHeight();
	}
}

// TODO: This
void ARadicalCharacter::DisplayDebug(UCanvas* Canvas, const FDebugDisplayInfo& DebugDisplay, float& YL, float& YPos)
{

	float Indent = 0.f;
	static FName NAME_Physics = FName(TEXT("Physics"));
	if (DebugDisplay.IsDisplayOn(NAME_Physics))
	{
		auto DisplayDebugManager = Canvas->DisplayDebugManager;
		
		FIndenter PhysicsIndent(Indent);

		if (MovementComponent != nullptr)
		{
			MovementComponent->DisplayDebug(Canvas, DebugDisplay, YL, YPos);
		}
		/*
		DisplayDebugManager.SetDrawColor(FColor::Yellow);
		FString T = FString::Printf(TEXT("----- BASED MOVEMENT -----"));
		DisplayDebugManager.DrawString(T, Indent);

		if (!GetBasedMovement().MovementBase)
		{
			DisplayDebugManager.SetDrawColor(FColor::Red);
			T = FString::Printf(TEXT("No Based Movement"));
			DisplayDebugManager.DrawString(T, Indent);
			
			DisplayDebugManager.SetDrawColor(FColor::White);
			T = FString::Printf(TEXT("Has Base Override? %s"), BOOL2STR(bHasBasedMovementOverride));
			DisplayDebugManager.DrawString(T, Indent);
		}
		else
		{
			DisplayDebugManager.SetDrawColor(FColor::White);
			T = FString::Printf(TEXT("Has Base Override? %s"), BOOL2STR(bHasBasedMovementOverride));
			DisplayDebugManager.DrawString(T, Indent);

			T = FString::Printf(TEXT("Base Name %s"), *GetBasedMovement().MovementBase.GetName());
			DisplayDebugManager.DrawString(T, Indent);

			T = FString::Printf(TEXT("Base Location %s"), *GetBasedMovement().Location.ToCompactString());
			DisplayDebugManager.DrawString(T, Indent);
		}
		*/
	}

	if (DebugDisplay.IsDisplayOn(TEXT("INPUTBUFFER")))
	{
		GetInputBuffer()->DisplayDebug(Canvas, DebugDisplay, YL, YPos);
	}
	
	Super::DisplayDebug(Canvas, DebugDisplay, YL, YPos);
}

void ARadicalCharacter::RecalculateBaseEyeHeight()
{
	Super::RecalculateBaseEyeHeight();
}

void ARadicalCharacter::UpdateNavigationRelevance()
{
	if (CapsuleComponent)
	{
		CapsuleComponent->SetCanEverAffectNavigation(bCanAffectNavigationGeneration);
	}
}

#pragma endregion APawn Interface

#pragma region Gameplay Interface

void ARadicalCharacter::LaunchCharacter(FVector LaunchVelocity, bool bPlanarOverride, bool bVerticalOverride)
{
	if (MovementComponent)
	{
		FVector FinalVel = LaunchVelocity;
		const FVector Velocity = GetVelocity();

		if (!bPlanarOverride)
		{
			FinalVel += FVector::VectorPlaneProject(Velocity, MovementComponent->GetUpOrientation(MODE_Gravity));
		}
		if (!bVerticalOverride)
		{
			FinalVel += Velocity.ProjectOnToNormal(MovementComponent->GetUpOrientation(MODE_Gravity));
		}

		MovementComponent->Launch(FinalVel);

		OnLaunched(LaunchVelocity, bPlanarOverride, bVerticalOverride);
	}
}


#pragma endregion Gameplay Interface

#pragma region Events

void ARadicalCharacter::Landed(const FHitResult& Hit)
{
	OnLanded(Hit);
	LandedDelegate.Broadcast(Hit);
}

void ARadicalCharacter::MovementStateChanged(EMovementState PrevMovementState)
{
	OnMovementStateChanged(PrevMovementState);
	MovementStateChangedDelegate.Broadcast(this, PrevMovementState);
}

void ARadicalCharacter::WalkingOffLedge(const FVector& PreviousFloorImpactNormal, const FVector& PreviousFloorContactNormal, const FVector& PreviousLocation, float DeltaTime)
{
	OnWalkingOffLedge(PreviousFloorImpactNormal, PreviousFloorContactNormal, PreviousLocation, DeltaTime);
	WalkedOffLedgeDelegate.Broadcast(PreviousFloorImpactNormal, PreviousFloorContactNormal, PreviousLocation, DeltaTime);
}

void ARadicalCharacter::ReachedJumpApex()
{
	OnReachedJumpApex();
	ReachedJumpApexDelegate.Broadcast();
}

void ARadicalCharacter::MoveBlockedBy(const FHitResult& Hit)
{
	OnMoveBlocked(Hit);
	MoveBlockedByDelegate.Broadcast(Hit);
}

void ARadicalCharacter::OnStuckInGeometry(const FHitResult& Hit)
{
	StuckInGeometryDelegate.Broadcast(Hit);
}


#pragma endregion Events

#pragma region Based Movement

void ARadicalCharacter::CreateBasedMovementInfo(FBasedMovementInfo& BasedMovementInfoToFill, UPrimitiveComponent* BaseComponent, const FName InBoneName)
{
	/* If new base component is nullptr, ignore bone name */
	const FName BoneName = (BaseComponent ? InBoneName : NAME_None);

	/* See what has changed */
	const bool bBaseChanged = (BaseComponent != BasedMovementInfoToFill.MovementBase);
	const bool bBoneChanged = (BoneName != BasedMovementInfoToFill.BoneName);

	if (bBaseChanged || bBoneChanged)
	{
		/* Verify no recursion */
		APawn* Loop = (BaseComponent ? Cast<APawn>(BaseComponent->GetOwner()): nullptr);
		while (Loop)
		{
			if (Loop == this) return;

			if (UPrimitiveComponent* LoopBase = Loop->GetMovementBase())
			{
				Loop = Cast<APawn>(LoopBase->GetOwner());
			}
			else
			{
				break;
			}
		}

		/* Set Base */
		UPrimitiveComponent* OldBase = BasedMovementInfoToFill.MovementBase;
		BasedMovementInfoToFill.MovementBase = BaseComponent;
		BasedMovementInfoToFill.BoneName = BoneName;

		/* Set tick dependencies */
		const bool bBaseIsSimulating = MovementBaseUtility::IsSimulatedBase(BaseComponent);
		if (bBaseChanged)
		{
			MovementBaseUtility::RemoveTickDependency(MovementComponent->PrimaryComponentTick, OldBase);
			/* Use special post physics function if simulating, otherwise add normal tick prereqs. */
			if (!bBaseIsSimulating)
			{
				MovementBaseUtility::AddTickDependency(MovementComponent->PrimaryComponentTick, BaseComponent);
			}
		}

		if (BaseComponent)
		{
			MovementComponent->SaveBaseLocation();
			MovementComponent->PostPhysicsTickFunction.SetTickFunctionEnable(bBaseIsSimulating);
		}
		else
		{
			BasedMovementInfoToFill.BoneName = NAME_None;
			BasedMovementInfoToFill.bRelativeRotation = false;
			MovementComponent->CurrentFloor.Clear();
			MovementComponent->PostPhysicsTickFunction.SetTickFunctionEnable(false);
		}
	}
}


void ARadicalCharacter::SetBase(UPrimitiveComponent* NewBaseComponent, const FName InBoneName, bool bNotifyActor)
{
	CreateBasedMovementInfo(BasedMovement, NewBaseComponent, InBoneName);

	if (bNotifyActor)
	{
		BaseChange();
	}
}

void ARadicalCharacter::SetBaseOverride(UPrimitiveComponent* NewBase, const FName BoneName)
{
	CreateBasedMovementInfo(BasedMovementOverride, NewBase, BoneName);
	
	if (NewBase)
	{
		bHasBasedMovementOverride = true;
	}
	else
	{
		bHasBasedMovementOverride = false;
	}
}

void ARadicalCharacter::RemoveBaseOverride()
{
	bHasBasedMovementOverride = false;
	SetBaseOverride(nullptr);
}


const FBasedMovementInfo& ARadicalCharacter::GetBasedMovement() const
{
	if (HasBasedMovementOverride() && BasedMovementOverride.MovementBase)
	{
		return BasedMovementOverride;
	}
	else
	{
		return BasedMovement;
	}
}

void ARadicalCharacter::SaveRelativeBasedMovement(const FVector& NewRelativeLocation, const FRotator& NewRotation, bool bRelativeRotation)
{
	FBasedMovementInfo BasedMovementUsed = GetBasedMovement();
	BasedMovementUsed.Location = NewRelativeLocation;
	BasedMovementUsed.Rotation = NewRotation;
	BasedMovementUsed.bRelativeRotation = bRelativeRotation;
}

	

#pragma endregion Based Movement

#pragma region Animation Interface

void ARadicalCharacter::SetAnimRootMotionTranslationScale(float InAnimRootMotionTranslationScale)
{
	AnimRootMotionTranslationScale = InAnimRootMotionTranslationScale;
}

float ARadicalCharacter::GetAnimRootMotionTranslationScale() const
{
	return AnimRootMotionTranslationScale;
}


float ARadicalCharacter::PlayAnimMontage(UAnimMontage* AnimMontage, float InPlayRate, FName StartSectionName)
{
	UAnimInstance* AnimInstance = (Mesh) ? Mesh->GetAnimInstance() : nullptr;
	if (AnimMontage && AnimInstance)
	{
		const float Duration = AnimInstance->Montage_Play(AnimMontage, InPlayRate);

		if (Duration > 0.f)
		{
			// Start at a given section
			if (StartSectionName != NAME_None)
			{
				AnimInstance->Montage_JumpToSection(StartSectionName, AnimMontage);
			}

			return Duration;
		}
	}

	return 0.f;
}

void ARadicalCharacter::StopAnimMontage(UAnimMontage* AnimMontage)
{
	UAnimInstance * AnimInstance = (Mesh)? Mesh->GetAnimInstance() : nullptr; 
	UAnimMontage * MontageToStop = (AnimMontage)? AnimMontage : GetCurrentMontage();
	bool bShouldStopMontage =  AnimInstance && MontageToStop && !AnimInstance->Montage_GetIsStopped(MontageToStop);

	if ( bShouldStopMontage )
	{
		AnimInstance->Montage_Stop(MontageToStop->BlendOut.GetBlendTime(), MontageToStop);
	}
}

UAnimMontage* ARadicalCharacter::GetCurrentMontage() const
{
	UAnimInstance * AnimInstance = (Mesh)? Mesh->GetAnimInstance() : nullptr; 
	if ( AnimInstance )
	{
		return AnimInstance->GetCurrentActiveMontage();
	}

	return nullptr;
}

FAnimMontageInstance* ARadicalCharacter::GetRootMotionAnimMontageInstance() const
{
	return (Mesh && Mesh->GetAnimInstance()) ? Mesh->GetAnimInstance()->GetRootMotionMontageInstance() : nullptr;
}

bool ARadicalCharacter::IsPlayingRootMotion() const
{
	if (Mesh)
	{
		return Mesh->IsPlayingRootMotion();
	}
	return false;
}

bool ARadicalCharacter::HasAnyRootMotion() const
{
	return false;
}

#pragma endregion Animation Interface