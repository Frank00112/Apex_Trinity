// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "TAIController.generated.h"

class AUnit;
class ATGameMode;

UCLASS()
class APEX_TRINITY_API ATAIController : public AAIController
{
	GENERATED_BODY()

public:
	ATAIController();

	// Main execution loop for the AI's turn
	void ExecuteAITurn();

	void ExecuteAIDeployment();

	// Array to store the units that still need to take action this turn
	TArray<class AUnit*> UnitsToProcess;

	// Tracks which unit in the array is currently acting
	int32 CurrentUnitIndex;

private:
	// Pointer to the referee to access grid and rules
	ATGameMode* GameMode;

	// Evaluates the board and processes a single unit's actions
	void ProcessUnitTurn(AUnit* AIUnit);

	// Scans the board to find the nearest Player 0 unit
	AUnit* FindClosestEnemy(AUnit* AIUnit);

	// Timer manager for pacing AI actions
	FTimerHandle AITimerHandle;

	// Function triggered by the timer to process the next unit in the array
	void ProcessNextUnit();

	UPROPERTY()
	TMap<class AUnit*, class ATile*> PreviousUnitTiles;
};