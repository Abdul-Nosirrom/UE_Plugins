// Copyright 2023 Abdulrahmen Almodaimegh. All Rights Reserved.

#include "OPCharacter.h"
#include "CFW_PCH.h"
#include "CustomMovementComponent.h"
#include "Debug/CFW_LOG.h"


/* Define default component object names */
FName AOPCharacter::MeshComponentName(TEXT("Character Mesh"));
FName AOPCharacter::CustomMovementComponentName(TEXT("Movement Component"));
FName AOPCharacter::CapsuleComponentName(TEXT("Collision Component"));

// Sets default values
AOPCharacter::AOPCharacter() : Super()
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
	//PrimaryActorTick.TickInterval = .5f;

	/* ~~~~~ Setup and attach primary components ~~~~~ */

	// Capsule Component
	CapsuleComponent = CreateDefaultSubobject<UCapsuleComponent>(AOPCharacter::CapsuleComponentName);
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
	CustomMovement = CreateDefaultSubobject<UCustomMovementComponent>(AOPCharacter::CustomMovementComponentName);
	if (CustomMovement)
	{
		CustomMovement->UpdatedComponent = CapsuleComponent;
		CustomMovement->CharacterOwner = this;
	}
	
	
	// Mesh Component
	Mesh = CreateOptionalDefaultSubobject<USkeletalMeshComponent>(AOPCharacter::MeshComponentName);
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

void AOPCharacter::PostLoad()
{
	Super::PostLoad();

#if WITH_EDITORONLY_DATA
	if (ArrowComponent)
	{
		ArrowComponent->SetSimulatePhysics(false);
	}
#endif // WITH_EDITORONLY_DATA
}

void AOPCharacter::BeginPlay()
{
	Super::BeginPlay();
	//bHasBasedMovementOverride = false;
}

void AOPCharacter::TickActor(float DeltaTime, ELevelTick TickType, FActorTickFunction& ThisTickFunction)
{
	Super::TickActor(DeltaTime, TickType, ThisTickFunction);
}

// TODO: This
void AOPCharacter::ClearCrossLevelReferences()
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

void AOPCharacter::GetSimpleCollisionCylinder(float& CollisionRadius, float& CollisionHalfHeight) const
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

UActorComponent* AOPCharacter::FindComponentByClass(const TSubclassOf<UActorComponent> ComponentClass) const
{
	// If the character has a Mesh, treat it as the first 'hit' when finding components
	if (Mesh && ComponentClass && Mesh->IsA(ComponentClass))
	{
		return Mesh;
	}

	return Super::FindComponentByClass(ComponentClass);
}

// TODO: This
void AOPCharacter::NotifyActorBeginOverlap(AActor* OtherActor)
{
	Super::NotifyActorBeginOverlap(OtherActor);
}

// TODO: This
void AOPCharacter::NotifyActorEndOverlap(AActor* OtherActor)
{
	Super::NotifyActorEndOverlap(OtherActor);
}

#pragma endregion AActor & UObject Interface


#pragma region APawn Interface

void AOPCharacter::PostInitializeComponents()
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_Character_PostInitComponents);

	Super::PostInitializeComponents();
	
	if (IsValid(this))
	{
		if (Mesh)
		{
			//CacheInitialMeshOffset(Mesh->GetRelativeLocation(), Mesh->GetRelativeRotation());

			// force animation tick after movement component updates
			if (Mesh->PrimaryComponentTick.bCanEverTick && CustomMovement)
			{
				Mesh->PrimaryComponentTick.AddPrerequisite(CustomMovement, CustomMovement->PrimaryComponentTick);
			}
		}

		if (CustomMovement && CapsuleComponent)
		{
			CustomMovement->UpdateNavAgent(*CapsuleComponent);
		}

	}
}

UPawnMovementComponent* AOPCharacter::GetMovementComponent() const
{
	return CustomMovement;
}

UPrimitiveComponent* AOPCharacter::GetMovementBase() const
{
	return GetBasedMovement().MovementBase;
}


// Called to bind functionality to input
void AOPCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	check(PlayerInputComponent)
}

float AOPCharacter::GetDefaultHalfHeight() const
{
	UCapsuleComponent* DefaultCapsule = GetClass()->GetDefaultObject<AOPCharacter>()->CapsuleComponent;
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
void AOPCharacter::DisplayDebug(UCanvas* Canvas, const FDebugDisplayInfo& DebugDisplay, float& YL, float& YPos)
{
	Super::DisplayDebug(Canvas, DebugDisplay, YL, YPos);

	float Indent = 0.f;
	static FName NAME_Physics = FName(TEXT("Physics"));
	if (DebugDisplay.IsDisplayOn(NAME_Physics))
	{
		auto DisplayDebugManager = Canvas->DisplayDebugManager;
		
		FIndenter PhysicsIndent(Indent);

		if (CustomMovement != nullptr)
		{
			CustomMovement->DisplayDebug(Canvas, DebugDisplay, YL, YPos);
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
}

void AOPCharacter::RecalculateBaseEyeHeight()
{
	Super::RecalculateBaseEyeHeight();
}

void AOPCharacter::UpdateNavigationRelevance()
{
	if (CapsuleComponent)
	{
		CapsuleComponent->SetCanEverAffectNavigation(bCanAffectNavigationGeneration);
	}
}

#pragma endregion APawn Interface

#pragma region Gameplay Interface

void AOPCharacter::LaunchCharacter(FVector LaunchVelocity, bool bPlanarOverride, bool bVerticalOverride)
{
	if (CustomMovement)
	{
		FVector FinalVel = LaunchVelocity;
		const FVector Velocity = GetVelocity();

		if (!bPlanarOverride)
		{
			FinalVel += FVector::VectorPlaneProject(Velocity, CustomMovement->GetUpOrientation(MODE_Gravity));
		}
		if (bVerticalOverride)
		{
			FinalVel += Velocity.ProjectOnToNormal(CustomMovement->GetUpOrientation(MODE_Gravity));
		}

		CustomMovement->Launch(FinalVel);

		OnLaunched(LaunchVelocity, bPlanarOverride, bVerticalOverride);
	}
}


#pragma endregion Gameplay Interface

#pragma region Events

void AOPCharacter::Landed(const FHitResult& Hit)
{
	OnLanded(Hit);
	LandedDelegate.Broadcast(Hit);
}

void AOPCharacter::MovementStateChanged(EMovementState PrevMovementState)
{
	OnMovementStateChanged(PrevMovementState);
	MovementStateChangedDelegate.Broadcast(this, PrevMovementState);
}

void AOPCharacter::WalkingOffLedge(const FVector& PreviousFloorImpactNormal, const FVector& PreviousFloorContactNormal, const FVector& PreviousLocation, float DeltaTime)
{
	OnWalkingOffLedge(PreviousFloorImpactNormal, PreviousFloorContactNormal, PreviousLocation, DeltaTime);
	WalkedOffLedgeDelegate.Broadcast(PreviousFloorImpactNormal, PreviousFloorContactNormal, PreviousLocation, DeltaTime);
}

void AOPCharacter::MoveBlockedBy(const FHitResult& Hit)
{
	OnMoveBlocked(Hit);
	MoveBlockedByDelegate.Broadcast(Hit);
}

void AOPCharacter::OnStuckInGeometry(const FHitResult& Hit)
{
	StuckInGeometryDelegate.Broadcast(Hit);
}


#pragma endregion Events

#pragma region Based Movement

void AOPCharacter::CreateBasedMovementInfo(FBasedMovementInfo& BasedMovementInfoToFill, UPrimitiveComponent* BaseComponent, const FName InBoneName)
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
			MovementBaseUtility::RemoveTickDependency(CustomMovement->PrimaryComponentTick, OldBase);
			/* Use special post physics function if simulating, otherwise add normal tick prereqs. */
			if (!bBaseIsSimulating)
			{
				MovementBaseUtility::AddTickDependency(CustomMovement->PrimaryComponentTick, BaseComponent);
			}
		}

		if (BaseComponent)
		{
			CustomMovement->SaveBaseLocation();
			CustomMovement->PostPhysicsTickFunction.SetTickFunctionEnable(bBaseIsSimulating);
		}
		else
		{
			BasedMovementInfoToFill.BoneName = NAME_None;
			BasedMovementInfoToFill.bRelativeRotation = false;
			CustomMovement->CurrentFloor.Clear();
			CustomMovement->PostPhysicsTickFunction.SetTickFunctionEnable(false);
		}
	}
}


void AOPCharacter::SetBase(UPrimitiveComponent* NewBaseComponent, const FName InBoneName, bool bNotifyActor)
{
	CreateBasedMovementInfo(BasedMovement, NewBaseComponent, InBoneName);

	if (bNotifyActor)
	{
		BaseChange();
	}
}

void AOPCharacter::SetBaseOverride(UPrimitiveComponent* NewBase, const FName BoneName)
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

void AOPCharacter::RemoveBaseOverride()
{
	bHasBasedMovementOverride = false;
	SetBaseOverride(nullptr);
}


const FBasedMovementInfo& AOPCharacter::GetBasedMovement() const
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

void AOPCharacter::SaveRelativeBasedMovement(const FVector& NewRelativeLocation, const FRotator& NewRotation, bool bRelativeRotation)
{
	FBasedMovementInfo BasedMovementUsed = GetBasedMovement();
	BasedMovementUsed.Location = NewRelativeLocation;
	BasedMovementUsed.Rotation = NewRotation;
	BasedMovementUsed.bRelativeRotation = bRelativeRotation;
}

	

#pragma endregion Based Movement

#pragma region Animation Interface

void AOPCharacter::SetAnimRootMotionTranslationScale(float InAnimRootMotionTranslationScale)
{
	AnimRootMotionTranslationScale = InAnimRootMotionTranslationScale;
}

float AOPCharacter::GetAnimRootMotionTranslationScale() const
{
	return AnimRootMotionTranslationScale;
}


float AOPCharacter::PlayAnimMontage(UAnimMontage* AnimMontage, float InPlayRate, FName StartSectionName)
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

void AOPCharacter::StopAnimMontage(UAnimMontage* AnimMontage)
{
	UAnimInstance * AnimInstance = (Mesh)? Mesh->GetAnimInstance() : nullptr; 
	UAnimMontage * MontageToStop = (AnimMontage)? AnimMontage : GetCurrentMontage();
	bool bShouldStopMontage =  AnimInstance && MontageToStop && !AnimInstance->Montage_GetIsStopped(MontageToStop);

	if ( bShouldStopMontage )
	{
		AnimInstance->Montage_Stop(MontageToStop->BlendOut.GetBlendTime(), MontageToStop);
	}
}

UAnimMontage* AOPCharacter::GetCurrentMontage() const
{
	UAnimInstance * AnimInstance = (Mesh)? Mesh->GetAnimInstance() : nullptr; 
	if ( AnimInstance )
	{
		return AnimInstance->GetCurrentActiveMontage();
	}

	return nullptr;
}

FAnimMontageInstance* AOPCharacter::GetRootMotionAnimMontageInstance() const
{
	return (Mesh && Mesh->GetAnimInstance()) ? Mesh->GetAnimInstance()->GetRootMotionMontageInstance() : nullptr;
}

bool AOPCharacter::IsPlayingRootMotion() const
{
	if (Mesh)
	{
		return Mesh->IsPlayingRootMotion();
	}
	return false;
}

bool AOPCharacter::HasAnyRootMotion() const
{
	return false;
}

#pragma endregion Animation Interface