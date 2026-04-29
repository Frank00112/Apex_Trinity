// Fill out your copyright notice in the Description page of Project Settings.


#include "TacticalCamera3D.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"

ATacticalCamera3D::ATacticalCamera3D()
{
	// Tick is disabled as camera movement is driven by Input Action events
	PrimaryActorTick.bCanEverTick = false;

	RootScene = CreateDefaultSubobject<USceneComponent>(TEXT("RootScene"));
	SetRootComponent(RootScene);

	// SpringArm manages camera distance and maintains a fixed pitch angle
	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArm->SetupAttachment(RootScene);
	SpringArm->bDoCollisionTest = false; // Disabled to prevent unwanted zooms on grid obstacles
	SpringArm->TargetArmLength = 3800.f;
	SpringArm->SetRelativeRotation(FRotator(-45.f, 0.f, 0.f));

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(SpringArm);
	Camera->SetProjectionMode(ECameraProjectionMode::Perspective);
}

void ATacticalCamera3D::BeginPlay()
{
	Super::BeginPlay();

	// 1. Register the Mapping Context to the Local Player Subsystem
	if (APlayerController* PlayerController = Cast<APlayerController>(GetController()))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
		{
			if (DefaultMappingContext)
			{
				Subsystem->AddMappingContext(DefaultMappingContext, 0);
			}
		}
	}
}

void ATacticalCamera3D::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	// 2. Bind Input Actions to handler functions using the Enhanced Input Component
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		if (TurnCameraAction)
		{
			// "Triggered" executes every frame while the key or axis is active
			EnhancedInputComponent->BindAction(TurnCameraAction, ETriggerEvent::Triggered, this, &ATacticalCamera3D::RotateCameraHorizontal);
		}

		if (ZoomCameraAction)
		{
			EnhancedInputComponent->BindAction(ZoomCameraAction, ETriggerEvent::Triggered, this, &ATacticalCamera3D::ZoomCamera);
		}

		if (PitchCameraAction)
		{
			EnhancedInputComponent->BindAction(PitchCameraAction, ETriggerEvent::Triggered, this, &ATacticalCamera3D::RotateCameraVertical);
		}
	}
}

void ATacticalCamera3D::RotateCameraHorizontal(const FInputActionValue& Value)
{
	// 3. Extract the input value (assuming a 1D float axis)
	float AxisValue = Value.Get<float>();

	if (!FMath::IsNearlyZero(AxisValue))
	{
		// Rotate the RootScene to pivot the entire camera rig around the center
		FRotator NewRotation = RootScene->GetComponentRotation();
		NewRotation.Yaw += AxisValue * 2.f; // Rotation speed multiplier
		RootScene->SetWorldRotation(NewRotation);
	}
}

void ATacticalCamera3D::ZoomCamera(const FInputActionValue& Value)
{
	// Extract the scalar value from the input action (typically mouse wheel axis)
	float ZoomValue = Value.Get<float>();

	if (!FMath::IsNearlyZero(ZoomValue))
	{
		// Adjust the spring arm length. 
		// A multiplier of 50.f defines the zoom sensitivity.
		float NewLength = SpringArm->TargetArmLength + (ZoomValue * 50.f);

		// Clamp the result to prevent the camera from clipping into the floor (500.f) 
		// or moving beyond the tactical view limit (4000.f)
		SpringArm->TargetArmLength = FMath::Clamp(NewLength, 500.f, 4000.f);
	}
}

void ATacticalCamera3D::RotateCameraVertical(const FInputActionValue& Value)
{
	float AxisValue = Value.Get<float>();

	if (!FMath::IsNearlyZero(AxisValue))
	{
		// Capture the current relative rotation of the SpringArm component
		FRotator NewRotation = SpringArm->GetRelativeRotation();

		// Apply the vertical tilt (Pitch) adjustment based on input
		NewRotation.Pitch += AxisValue * 2.f; // 2.f represents the rotation sensitivity

		// Clamo the pitch to prevent the camera from flipping or gimbal locking
		// -85.f: Near-top-down perspective (looking straight at the grid)
		// -10.f: Low-angle perspective (looking towards the horizon)
		NewRotation.Pitch = FMath::Clamp(NewRotation.Pitch, -85.f, -10.f);

		// Apply the finalized, constrained rotation back to the SpringArm
		SpringArm->SetRelativeRotation(NewRotation);
	}
}