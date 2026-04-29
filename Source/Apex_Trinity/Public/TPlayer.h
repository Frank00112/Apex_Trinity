// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "TPlayer.generated.h"

// Forward declaration: tells the compiler that UCameraComponent exists.
// This keeps the header lightweight and drastically reduces compile times.
class UCameraComponent;

UCLASS()
class APEX_TRINITY_API ATPlayer : public APawn
{
	GENERATED_BODY()

public:
	// Sets default values for this pawn's properties
	ATPlayer();

protected:
	// The camera pointing down at the grid
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UCameraComponent* CameraComponent;
};
