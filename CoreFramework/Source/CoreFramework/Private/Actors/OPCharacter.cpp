// Copyright 2023 Abdulrahmen Almodaimegh. All Rights Reserved.

#include "OPCharacter.h"
#include "CFW_PCH.h"
#include "CustomMovementComponent.h"
#include "Template/OPMovementComponent.h"


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
	CustomMovement = CreateDefaultSubobject<UOPMovementComponent>(AOPCharacter::CustomMovementComponentName);
	if (CustomMovement)
	{
		CustomMovement->UpdatedComponent = CapsuleComponent;
		CustomMovement->SetSkeletalMeshReference(Mesh);
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
}

void AOPCharacter::TickActor(float DeltaTime, ELevelTick TickType, FActorTickFunction& ThisTickFunction)
{
	Super::TickActor(DeltaTime, TickType, ThisTickFunction);
}

// TODO: This
void AOPCharacter::ClearCrossLevelReferences()
{
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
		FIndenter PhysicsIndent(Indent);
		if (CustomMovement != nullptr)
		{
			CustomMovement->DisplayDebug(Canvas, DebugDisplay, YL, YPos);
		}
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

#pragma region Animation Interface

float AOPCharacter::PlayAnimMontage(UAnimMontage* AnimMontage, float InPlayRate, FName StartSectionName)
{
	return 0.f;
}

void AOPCharacter::StopAnimMontage(UAnimMontage* AnimMontage)
{
}

UAnimMontage* AOPCharacter::GetCurrentMontage() const
{
	return nullptr;
}

FAnimMontageInstance* AOPCharacter::GetRootMotionAnimMontageInstance() const
{
	return nullptr;
}

bool AOPCharacter::IsPlayingRootMotion() const
{
	return CustomMovement->HasAnimRootMotion();
}

bool AOPCharacter::HasAnyRootMotion() const
{
	return false;
}

#pragma endregion Animation Interface