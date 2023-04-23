// Fill out your copyright notice in the Description page of Project Settings.


#include "Actors/RadicalPlayerCharacter.h"

#include "DisplayDebugHelpers.h"
#include "Components/RadicalMovementComponent.h"
#include "CameraBoomComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/PlayerController.h"
#include "Subsystems/InputBufferSubsystem.h"
#include "EnhancedInputComponent.h"
#include "Components/ActionManagerComponent.h"
#include "Engine/Canvas.h"
#include "Engine/LocalPlayer.h"


// Sets default values
ARadicalPlayerCharacter::ARadicalPlayerCharacter()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	SetActorTickEnabled(true);
	
	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);
		
	// Don't rotate when the controller rotates. Let that just affect the camera.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Configure character movement
	GetCharacterMovement()->CharacterOwner = this;

	// Create a camera boom (pulls in towards the player if there is a collision)
	CameraBoom = CreateDefaultSubobject<UCameraBoomComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.0f; // The camera follows at this distance behind the character	
	CameraBoom->bUsePawnControlRotation = true; // Rotate the arm based on the controller

	// Create a follow camera
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName); // Attach the camera to the end of the boom and let the boom adjust to match the controller orientation
	FollowCamera->bUsePawnControlRotation = false; // Camera does not rotate relative to arm

	// Create action manager
	ActionManager = CreateDefaultSubobject<UActionManagerComponent>(TEXT("ActionManager"));
	ActionManager->CharacterOwner = this;
}

// Called when the game starts or when spawned
void ARadicalPlayerCharacter::BeginPlay()
{
	Super::BeginPlay();

	ActionManager->CharacterOwner = this;

	if (!GetInputBuffer())
	{
		UE_LOG(LogTemp, Error, TEXT("Buffer null in begin play"))
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Buffer valid in begin play"))
	}

	UE_LOG(LogTemp, Error, TEXT("Context name: %s"), *GetName())
}

void ARadicalPlayerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	// Set up action bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = CastChecked<UEnhancedInputComponent>(PlayerInputComponent)) 
	{
		//Add Input Mapping Context
		if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
		{
			if (auto Subsystem = ULocalPlayer::GetSubsystem<UInputBufferSubsystem>(PlayerController->GetLocalPlayer()))
			{
				Subsystem->AddMappingContext(InputMap, EnhancedInputComponent);
			}
		}

		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ARadicalPlayerCharacter::RegisterMoveAxis);
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &ARadicalPlayerCharacter::RegisterLookAxis);

	}
}

void ARadicalPlayerCharacter::RegisterMoveAxis(const FInputActionValue& Value)
{
	// input is a Vector2D
	const FVector2D MovementVector = Value.Get<FVector2D>();

	if (Controller != nullptr)
	{
		auto PCM = UGameplayStatics::GetPlayerCameraManager(GetWorld(), 0);

		const FRotator Rotation = PCM->GetCameraRotation(); // NOTE: This is how you do it
		
		AddMovementInput(FVector(1,0,0), MovementVector.Y);
		AddMovementInput(FVector(0,1,0), MovementVector.X);
		
		return;
		// find out which way is forward
		//const FRotator Rotation = MovementComponent->IsMovingOnGround() ? GetActorRotation() : Controller->GetControlRotation(); //TEMP: FOR SKATING
		const FRotator YawRotation(0, Rotation.Yaw, 0); 

		// get forward vector
		const FVector ForwardDirection = GetActorForwardVector(); //FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	
		// get right vector 
		const FVector RightDirection = GetActorRightVector();//FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		// add movement 
		AddMovementInput(ForwardDirection, MovementVector.Y);
		AddMovementInput(RightDirection, MovementVector.X);
	}
}

void ARadicalPlayerCharacter::RegisterLookAxis(const FInputActionValue& Value)
{
	// input is a Vector2D
	const FVector2D LookAxisVector = Value.Get<FVector2D>();

	if (Controller != nullptr)
	{
		// add yaw and pitch input to controller
		AddControllerYawInput(LookAxisVector.X);
		AddControllerPitchInput(LookAxisVector.Y);
	}
}

UInputBufferSubsystem* ARadicalPlayerCharacter::GetInputBuffer() 
{
	if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
	{
		return ULocalPlayer::GetSubsystem<UInputBufferSubsystem>(PlayerController->GetLocalPlayer());
	}
	UE_LOG(LogTemp, Error, TEXT("Input buffer null in radical player class??"))
	return nullptr;
}


void ARadicalPlayerCharacter::DisplayDebug(UCanvas* Canvas, const FDebugDisplayInfo& DebugDisplay, float& YL,
	float& YPos)
{
	float Time = UGameplayStatics::GetTimeSeconds(GetWorld());
	Canvas->DrawDebugGraph(FString("Acceleration Curve"), Time, MovementComponent->Velocity.Length()/MovementComponent->GetMovementData()->MaxSpeed, 0.1, 0.4, 400, 400, FVector2D(Time-3,Time+2), FVector2D(0,2));
	
	static FDebugFloatHistory floatHistory;
	floatHistory.AddSample(MovementComponent->Velocity.Length()/MovementComponent->GetMovementData()->MaxSpeed);
	floatHistory.MinValue = -0.1;
	floatHistory.MaxValue = 1.4;

	FTransform DrawTrans = GetTransform();
	DrawTrans.SetLocation(DrawTrans.GetLocation() + GetActorUpVector() * 50.f);
	DrawTrans.SetRotation(UGameplayStatics::GetPlayerController(GetWorld(), 0)->GetControlRotation().Quaternion());
	FVector2D DrawSize(500,500);
	DrawDebugFloatHistory(*GetWorld(), floatHistory, DrawTrans, DrawSize, FColor::Green, false, 0.f, 1);
	
	CameraBoom->DisplayDebug(Canvas);
	return;
	Super::DisplayDebug(Canvas, DebugDisplay, YL, YPos);
	
	static FName NAME_Physics = FName(TEXT("Actions"));
	if (DebugDisplay.IsDisplayOn(NAME_Physics))
	{
		ActionManager->DisplayDebug(Canvas, DebugDisplay, YL, YPos);
	}
	
}


