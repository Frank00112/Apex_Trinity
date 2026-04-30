// Fill out your copyright notice in the Description page of Project Settings.


#include "TPlayer.h"
#include "Camera/CameraComponent.h" 

ATPlayer::ATPlayer()
{
	// Disable tick, the camera will be static
	PrimaryActorTick.bCanEverTick = false;

	// Create a basic root component
	USceneComponent* SceneComp = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(SceneComp);

	// Create and attach the camera
	CameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("CameraComponent"));
	CameraComponent->SetupAttachment(RootComponent);
	// Point the camera straight down (Pitch = -90 degrees) to simulate 2D Top-Down view
	CameraComponent->SetRelativeRotation(FRotator(-90.f, 0.f, 0.f));
	CameraComponent->SetProjectionMode(ECameraProjectionMode::Perspective);
	// Reduce the Field of View (FOV) to flatten the perspective and simulate an orthographic projection
	CameraComponent->FieldOfView = 20.f;
	CameraComponent->SetRelativeLocation(FVector(0.f, 0.f, 14000.f));
}
