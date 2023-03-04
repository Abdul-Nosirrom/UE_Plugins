#include "Template/TOPCharacter.h"

#include "BufferedController.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "Components/CustomMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/SpringArmComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Subsystems/InputBufferSubsystem.h"


ATOPCharacter::ATOPCharacter() : Super() 
{

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
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.0f; // The camera follows at this distance behind the character	
	CameraBoom->bUsePawnControlRotation = true; // Rotate the arm based on the controller

	// Create a follow camera
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName); // Attach the camera to the end of the boom and let the boom adjust to match the controller orientation
	FollowCamera->bUsePawnControlRotation = false; // Camera does not rotate relative to arm

	// Note: The skeletal mesh and anim blueprint references on the Mesh component (inherited from Character) 
	// are set in the derived blueprint asset named ThirdPersonCharacter (to avoid direct content references in C++)
}

void ATOPCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
}

void ATOPCharacter::BeginPlay()
{
	// Call the base class  
	Super::BeginPlay();

	//Add Input Mapping Context
	//if (ABufferedController* PlayerController = Cast<ABufferedController>(Controller))
	//{
	//	if (UInputBufferSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UInputBufferSubsystem>(PlayerController->GetLocalPlayer()))
	//	{
	//		Subsystem->AddMappingContext(DefaultInputBufferMapping, Cast<UEnhancedInputComponent>(PlayerController->InputComponent));
	//	}
	//}
}


//////////////////////////////////////////////////////////////////////////
// Input

void ATOPCharacter::SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent)
{
	// Set up action bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = CastChecked<UEnhancedInputComponent>(PlayerInputComponent)) 
	{
		//Add Input Mapping Context
		if (ABufferedController* PlayerController = Cast<ABufferedController>(Controller))
		{
			if (UInputBufferSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UInputBufferSubsystem>(PlayerController->GetLocalPlayer()))
			{
				Subsystem->AddMappingContext(DefaultInputBufferMapping, EnhancedInputComponent);

				// Bind Input Buffer Shit
				//Subsystem->InputPressedDelegate.AddDynamic(this, &ATOPCharacter::InputTriggered);
				//Subsystem->DirectionalAndButtonDelegate.AddDynamic(this, &ATOPCharacter::DirectionalRegistered);
				Subsystem->DirectionalInputRegisteredDelegate.AddDynamic(this, &ATOPCharacter::DirectionalRegistered);
				//Subsystem->InputPressedDelegate.AddUFunction(this, &ATOPCharacter::InputTriggered);
				//Subsystem->InputPressedDelegate.Add()
				Subsystem->InputReleasedDelegate.AddDynamic(this, &ATOPCharacter::InputReleased);
			}
		}
		//Jumping
		//EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Triggered, this, &ATOPCharacter::Jump);
		//EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ATOPCharacter::StopJumping);

		//Moving NOTE: "Look" we can leave out of the input buffer. Move
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ATOPCharacter::Move);

		//Looking
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &ATOPCharacter::Look);

	}

}


void ATOPCharacter::InputTriggered(const UInputAction* InputAction, const FInputActionValue& Value)
{
	UE_LOG(LogTemp, Error, TEXT("Recieved buffer event for %s"), *InputAction->ActionDescription.ToString());
	if (InputAction == JumpAction) Jump(Value);
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Action comparison failed for %s == %s"), *JumpAction->ActionDescription.ToString(), *InputAction->ActionDescription.ToString());
	}
}

void ATOPCharacter::DirectionalRegistered(const UMotionAction* Motion)
{
	if (DirectionalInput == Motion) Jump(FInputActionValue());
}


void ATOPCharacter::InputReleased(const UInputAction* InputAction)
{
	if (InputAction == JumpAction) StopJumping(FInputActionValue());
}


void ATOPCharacter::Move(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D MovementVector = Value.Get<FVector2D>();

	if (Controller != nullptr)
	{
		// find out which way is forward
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		// get forward vector
		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	
		// get right vector 
		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		// add movement 
		AddMovementInput(ForwardDirection, MovementVector.Y);
		AddMovementInput(RightDirection, MovementVector.X);
	}
}

void ATOPCharacter::Look(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D LookAxisVector = Value.Get<FVector2D>();

	if (Controller != nullptr)
	{
		// add yaw and pitch input to controller
		AddControllerYawInput(LookAxisVector.X);
		AddControllerPitchInput(LookAxisVector.Y);
	}
}

void ATOPCharacter::Jump(const FInputActionValue& Value)
{
	if (GetMovementComponent()->IsMovingOnGround())
	{
		APlayerController* PC = Cast<APlayerController>(GetController());
		ULocalPlayer::GetSubsystem<UInputBufferSubsystem>(PC->GetLocalPlayer())->ConsumeButtonInput(JumpAction);
		OnJumpedDelegate.Broadcast();
		LaunchCharacter(FVector(0,0, JumpZVelocity), false, true);
	}
}

void ATOPCharacter::StopJumping(const FInputActionValue& Value)
{
	//ResetJumpState();
}

