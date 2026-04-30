// Fill out your copyright notice in the Description page of Project Settings.


#include "TAIController.h"
#include "TGameMode.h"
#include "TPlayerController.h" 
#include "Unit.h"
#include "GameField.h"
#include "Tower.h"
#include "Sniper.h"
#include "Brawler.h"
#include "Kismet/GameplayStatics.h"

ATAIController::ATAIController()
{
	// Default constructor
}

void ATAIController::ExecuteAITurn()
{
	GameMode = Cast<ATGameMode>(UGameplayStatics::GetGameMode(GetWorld()));

	// If the match has concluded, the AI does not initiate its turn logic
	if (!GameMode || GameMode->bIsGameOver) return;

	TArray<AActor*> AllActors;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AUnit::StaticClass(), AllActors);

	UnitsToProcess.Empty();

	for (AActor* Actor : AllActors)
	{
		AUnit* Unit = Cast<AUnit>(Actor);
		if (IsValid(Unit) && Unit->TeamID == 1)
		{
			UnitsToProcess.Add(Unit);
		}
	}

	CurrentUnitIndex = 0;

	GetWorld()->GetTimerManager().SetTimer(AITimerHandle, this, &ATAIController::ProcessNextUnit, 2.0f, false);
}

void ATAIController::ProcessNextUnit()
{
	if (CurrentUnitIndex < UnitsToProcess.Num())
	{
		AUnit* UnitToCommand = UnitsToProcess[CurrentUnitIndex];
		ProcessUnitTurn(UnitToCommand);
		CurrentUnitIndex++;

		GetWorld()->GetTimerManager().SetTimer(AITimerHandle, this, &ATAIController::ProcessNextUnit, 2.5f, false);
	}
	else
	{
		if (GameMode && GameMode->GameField)
		{
			GameMode->GameField->ClearHighlights();
		}
		GetWorld()->GetTimerManager().ClearTimer(AITimerHandle);
		GameMode->EndTurn();
	}
}

void ATAIController::ProcessUnitTurn(AUnit* AIUnit)
{
	if (!AIUnit || !GameMode || !GameMode->GameField) return;

	GameMode->GameField->ClearHighlights();

	if (AIUnit->CurrentTile)
	{
		AIUnit->CurrentTile->BP_SetSelectionHighlight(true, AIUnit->TeamID);
	}

	AUnit* TargetEnemy = FindClosestEnemy(AIUnit);

	// ACTION A: try to attack immediately if already in range
	if (TargetEnemy && GameMode->IsAttackValid(AIUnit, TargetEnemy))
	{
		if (TargetEnemy->CurrentTile)
		{
			TargetEnemy->CurrentTile->BP_SetAttackHighlight(true);
		}

		// Force the AI to orient itself towards the target before initiating the attack
		AIUnit->FaceTargetLocation(TargetEnemy->GetActorLocation());

		int32 Damage = AIUnit->GetRandomAttackDamage();
		TargetEnemy->ReceiveDamage(Damage);

		// GDD logging: attack
		FString PlayerTag = (AIUnit->TeamID == 0) ? TEXT("HP") : TEXT("AI");
		FString UnitTag = AIUnit->IsA(ASniper::StaticClass()) ? TEXT("S") : TEXT("B");
		FString TargetCoord = GameMode->GameField->GetAlphanumericCoordinate(TargetEnemy->CurrentTile->GetGridPosition());

		// Format: [Player] [Unit] [TargetCoord] [Damage]
		FString LogStr = FString::Printf(TEXT("%s: %s %s %d"), *PlayerTag, *UnitTag, *TargetCoord, Damage);
		GameMode->LogAction(LogStr);

		// Counterattack logic
		if (AIUnit->IsA(ASniper::StaticClass()))
		{
			bool bTargetIsSniper = TargetEnemy->IsA(ASniper::StaticClass());
			bool bTargetIsBrawler = TargetEnemy->IsA(ABrawler::StaticClass());
			int32 Dist = GameMode->GameField->GetManhattanDistance(AIUnit->CurrentTile->GetGridPosition(), TargetEnemy->CurrentTile->GetGridPosition());

			if (bTargetIsSniper || (bTargetIsBrawler && Dist == 1))
			{
				int32 CounterDamage = FMath::RandRange(1, 3);
				AIUnit->ReceiveDamage(CounterDamage);

				// GDD logging: counterattack
				// The defender (TargetEnemy) becomes the executor of the counterattack
				FString CounterPlayerTag = (TargetEnemy->TeamID == 0) ? TEXT("HP") : TEXT("AI");
				FString CounterUnitTag = TargetEnemy->IsA(ASniper::StaticClass()) ? TEXT("S") : TEXT("B");
				FString AICoord = GameMode->GameField->GetAlphanumericCoordinate(AIUnit->CurrentTile->GetGridPosition());

				// Standardized the counterattack string format to match the human player
				FString CounterLogStr = FString::Printf(TEXT("%s: %s %s Counterattack %d"), *CounterPlayerTag, *CounterUnitTag, *AICoord, CounterDamage);
				GameMode->LogAction(CounterLogStr);

				// UI Notification for the Human Player observing the AI turn
				if (ATPlayerController* PC = Cast<ATPlayerController>(GetWorld()->GetFirstPlayerController()))
				{
					if (PC->MainHUDWidget)
					{
						PC->MainHUDWidget->BP_ShowSystemMessage(TEXT("COUNTERATTACK!"));
					}
				}
			}
		}

		AIUnit->bHasAttacked = true;
		AIUnit->bHasMoved = true;
		return;
	}

	// ACTION B: calculate optimal movement
	ATile* BestTile = nullptr;
	float BestTacticalScore = -MAX_FLT; // Initialize with the lowest possible floating-point value

	TArray<AActor*> AllTiles;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ATile::StaticClass(), AllTiles);

	TArray<AActor*> AllTowers;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ATower::StaticClass(), AllTowers);

	for (AActor* Actor : AllTiles)
	{
		ATile* Tile = Cast<ATile>(Actor);
		if (Tile && GameMode->IsMoveValid(AIUnit, Tile))
		{
			float TacticalScore = 0.f;

			// 1. Enemy factor: incentivize closing the gap with the target (if a target exists)
			if (TargetEnemy)
			{
				float DistToEnemy = FVector::Dist(Tile->GetActorLocation(), TargetEnemy->GetActorLocation());
				TacticalScore -= DistToEnemy; // Subtract distance (lower distance results in a higher score)
			}

			// 2. Objective factor: assign absolute priority to uncontested Towers
			for (AActor* TowerActor : AllTowers)
			{
				ATower* Tower = Cast<ATower>(TowerActor);
				// Evaluate only towers NOT currently controlled by the AI (Neutral or Player 0)
				if (Tower && Tower->ControllingTeam != 1 && Tower->OwningTile)
				{
					int32 ManhattanDist = GameMode->GameField->GetManhattanDistance(Tile->GetGridPosition(), Tower->OwningTile->GetGridPosition());

					// If this tile is within the "Capture Zone" (Manhattan distance of 1 or 2)
					if (ManhattanDist <= 2)
					{
						TacticalScore += 50000.f; // Bonus: capture the tower
					}
					else
					{
						// Proportional bonus: the closer the tile is to the tower, the higher the score
						TacticalScore += (5000.f / (float)ManhattanDist);
					}
				}
			}

			// Memory penalty to break infinite pathfinding loops
			if (PreviousUnitTiles.Contains(AIUnit) && PreviousUnitTiles[AIUnit] == Tile)
			{
				TacticalScore -= 100000.f; // Large penalty applied to the previous tile's cost to prevent immediate backtracking
			}

			// If this tactical score is the highest found so far, cache this tile as the best option
			if (TacticalScore > BestTacticalScore)
			{
				BestTacticalScore = TacticalScore;
				BestTile = Tile;
			}
		}
	}

	// ACTION C: execute the best found move
	if (BestTile)
	{
		BestTile->BP_SetMovementHighlight(true);

		// GDD logging: pre-movement
		// Save the original position before calling MoveToTile
		FVector2D OriginPos = AIUnit->CurrentTile->GetGridPosition();

		if (AIUnit->CurrentTile)
		{
			AIUnit->CurrentTile->OccupyingUnit = nullptr;
		}

		// Force the AI to orient towards the destination before starting the movement
		AIUnit->FaceTargetLocation(BestTile->GetActorLocation());

		// Cache current tile position before initiating movement
		if (AIUnit->CurrentTile)
		{
			PreviousUnitTiles.Add(AIUnit, AIUnit->CurrentTile);
		}

		AIUnit->MoveToTile(BestTile, true);

		// GDD logging: post-movement
		FVector2D DestPos = BestTile->GetGridPosition();

		FString MovePlayerTag = (AIUnit->TeamID == 0) ? TEXT("HP") : TEXT("AI");
		FString MoveUnitTag = AIUnit->IsA(ASniper::StaticClass()) ? TEXT("S") : TEXT("B");
		FString OriginStr = GameMode->GameField->GetAlphanumericCoordinate(OriginPos);
		FString DestStr = GameMode->GameField->GetAlphanumericCoordinate(DestPos);

		FString MoveLogStr = FString::Printf(TEXT("%s: %s %s -> %s"), *MovePlayerTag, *MoveUnitTag, *OriginStr, *DestStr);
		GameMode->LogAction(MoveLogStr);

		BestTile->OccupyingUnit = AIUnit;
		AIUnit->bHasMoved = true;

		// ACTION D: check if the AI can attack after closing the distance
		if (TargetEnemy && !AIUnit->bHasAttacked && GameMode->IsAttackValid(AIUnit, TargetEnemy))
		{
			if (TargetEnemy->CurrentTile)
			{
				TargetEnemy->CurrentTile->BP_SetAttackHighlight(true);
			}

			// Ensure the AI rotates to face the target before executing the attack
			AIUnit->FaceTargetLocation(TargetEnemy->GetActorLocation());

			int32 Damage = AIUnit->GetRandomAttackDamage();
			TargetEnemy->ReceiveDamage(Damage);

			// GDD logging: attack
			FString AtkPlayerTag = (AIUnit->TeamID == 0) ? TEXT("HP") : TEXT("AI");
			FString AtkUnitTag = AIUnit->IsA(ASniper::StaticClass()) ? TEXT("S") : TEXT("B");
			FString AtkTargetCoord = GameMode->GameField->GetAlphanumericCoordinate(TargetEnemy->CurrentTile->GetGridPosition());

			// Format: [Player] [Unit] [TargetCoord] [Damage]
			FString AtkLogStr = FString::Printf(TEXT("%s: %s %s %d"), *AtkPlayerTag, *AtkUnitTag, *AtkTargetCoord, Damage);
			GameMode->LogAction(AtkLogStr);

			// Counterattack logic after movement
			if (AIUnit->IsA(ASniper::StaticClass()))
			{
				bool bTargetIsSniper = TargetEnemy->IsA(ASniper::StaticClass());
				bool bTargetIsBrawler = TargetEnemy->IsA(ABrawler::StaticClass());
				int32 Dist = GameMode->GameField->GetManhattanDistance(AIUnit->CurrentTile->GetGridPosition(), TargetEnemy->CurrentTile->GetGridPosition());

				if (bTargetIsSniper || (bTargetIsBrawler && Dist == 1))
				{
					int32 CounterDamage = FMath::RandRange(1, 3);
					AIUnit->ReceiveDamage(CounterDamage);

					// GDD logging: counterattack
					// The defender (TargetEnemy) becomes the executor of the counterattack
					FString CounterPlayerTag = (TargetEnemy->TeamID == 0) ? TEXT("HP") : TEXT("AI");
					FString CounterUnitTag = TargetEnemy->IsA(ASniper::StaticClass()) ? TEXT("S") : TEXT("B");
					FString AICoord = GameMode->GameField->GetAlphanumericCoordinate(AIUnit->CurrentTile->GetGridPosition());

					// Standardized the counterattack string format to match the human player
					FString CounterLogStr = FString::Printf(TEXT("%s: %s %s Counterattack %d"), *CounterPlayerTag, *CounterUnitTag, *AICoord, CounterDamage);
					GameMode->LogAction(CounterLogStr);

					// UI Notification for the Human Player observing the AI turn
					if (ATPlayerController* PC = Cast<ATPlayerController>(GetWorld()->GetFirstPlayerController()))
					{
						if (PC->MainHUDWidget)
						{
							PC->MainHUDWidget->BP_ShowSystemMessage(TEXT("COUNTERATTACK!"));
						}
					}
				}
			}

			AIUnit->bHasAttacked = true;
		}
	}
}

AUnit* ATAIController::FindClosestEnemy(AUnit* AIUnit)
{
	AUnit* ClosestEnemy = nullptr;
	float MinDistance = MAX_FLT;

	TArray<AActor*> AllActors;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AUnit::StaticClass(), AllActors);

	for (AActor* Actor : AllActors)
	{
		AUnit* Enemy = Cast<AUnit>(Actor);
		if (IsValid(Enemy) && Enemy->TeamID == 0 && Enemy->CurrentTile)
		{
			float Distance = FVector::Dist(AIUnit->GetActorLocation(), Enemy->GetActorLocation());
			if (Distance < MinDistance)
			{
				MinDistance = Distance;
				ClosestEnemy = Enemy;
			}
		}
	}

	return ClosestEnemy;
}

void ATAIController::ExecuteAIDeployment()
{
	GameMode = Cast<ATGameMode>(UGameplayStatics::GetGameMode(GetWorld()));
	if (!GameMode || GameMode->MatchPhase != EMatchPhase::Setup) return;

	TArray<AActor*> AllTiles;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ATile::StaticClass(), AllTiles);

	TArray<ATile*> ValidTiles;

	// Identify all legal tiles within the AI's designated deployment zone
	for (AActor* Actor : AllTiles)
	{
		ATile* Tile = Cast<ATile>(Actor);
		if (Tile && !Tile->bIsObstacle && !Tile->OccupyingUnit && Tile->GetElevation() != ETileElevation::Water)
		{
			// AI Zone Constraint: Only tiles in rows Y 22 through 24 are eligible
			if (Tile->GetGridPosition().X <= 2)
			{
				ValidTiles.Add(Tile);
			}
		}
	}

	// Select a random valid tile and submit the deployment request to the GameMode
	if (ValidTiles.Num() > 0)
	{
		int32 RandomIndex = FMath::RandRange(0, ValidTiles.Num() - 1);
		GameMode->RequestDeployment(ValidTiles[RandomIndex], 1);
	}
}