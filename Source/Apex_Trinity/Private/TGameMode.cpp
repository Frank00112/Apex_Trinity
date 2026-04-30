// Fill out your copyright notice in the Description page of Project Settings.


#include "TGameMode.h"
#include "TPlayerController.h"
#include "TAIController.h"
#include "TPlayer.h"
#include "GameField.h"
#include "Tile.h"
#include "Unit.h"
#include "Tower.h"
#include "Sniper.h"
#include "Kismet/GameplayStatics.h" // Needed to find actors in the world

ATGameMode::ATGameMode()
{
	// 1. Tell the game to use our custom PlayerController
	PlayerControllerClass = ATPlayerController::StaticClass();

	// 2. Tell the game to spawn our custom static 2D Camera instead of the default spectator
	DefaultPawnClass = ATPlayer::StaticClass();

	// Default initialization
	CurrentTurn = ETurnState::Player0Turn;
}

void ATGameMode::BeginPlay()
{
	Super::BeginPlay();

	// 1. Initialize the GameField
	GameField = Cast<AGameField>(UGameplayStatics::GetActorOfClass(GetWorld(), AGameField::StaticClass()));

	if (GameField)
	{
		UE_LOG(LogTemp, Warning, TEXT("GameField successfully found and linked by GameMode!"));
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("GameField NOT found! Make sure BP_GameField is placed in the level."));
	}

	// 2. Spawn the AI Controller dynamically
	AIPlayer = GetWorld()->SpawnActor<ATAIController>(ATAIController::StaticClass());

	if (AIPlayer)
	{
		UE_LOG(LogTemp, Warning, TEXT("SYSTEM: AI Controller dynamically spawned and registered by GameMode."));
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("CRITICAL ERROR: GameMode failed to spawn AI Controller."));
	}
}

void ATGameMode::StartMatch()
{
	// Set the initial turn to Player 1
	CurrentTurn = ETurnState::Player0Turn;
	UE_LOG(LogTemp, Warning, TEXT("Match Started! It is now Player 0's Turn."));
}

void ATGameMode::EndTurn()
{
	// 1. Force a complete visual clear every time the turn changes
	if (GameField)
	{
		GameField->ClearHighlights();
	}

	// 2. State machine and tower control evaluation
	TArray<AActor*> AllTowers;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ATower::StaticClass(), AllTowers);

	int32 TowersPlayer0 = 0;
	int32 TowersPlayer1 = 0;

	for (AActor* Actor : AllTowers)
	{
		ATower* Tower = Cast<ATower>(Actor);
		if (Tower)
		{
			// Evaluate who controls the tower at this exact moment
			Tower->EvaluateControlState();

			// Count how many towers are under exclusive control
			if (Tower->CurrentState == ETowerState::Captured)
			{
				if (Tower->ControllingTeam == 0) TowersPlayer0++;
				if (Tower->ControllingTeam == 1) TowersPlayer1++;
			}
		}
	}

	// 3. Win condition (2 towers for 2 consecutive turns)
	if (TowersPlayer0 >= 2) Player0TowerTurns++;
	else Player0TowerTurns = 0;

	if (TowersPlayer1 >= 2) Player1TowerTurns++;
	else Player1TowerTurns = 0;

	// Final Victory Evaluation
	if (Player0TowerTurns >= 4)
	{
		bIsGameOver = true;

		// Victory message for Player 0
		if (ATPlayerController* PC = Cast<ATPlayerController>(GetWorld()->GetFirstPlayerController()))
		{
			if (PC->MainHUDWidget)
			{
				PC->MainHUDWidget->BP_ShowSystemMessage(TEXT("VICTORY! Human Player dominates the Towers!"));
			}
		}

		BP_OnGameOver(0);
		return;
	}
	else if (Player1TowerTurns >= 4)
	{
		bIsGameOver = true;

		// Victory message for Player 1 (AI)
		if (ATPlayerController* PC = Cast<ATPlayerController>(GetWorld()->GetFirstPlayerController()))
		{
			if (PC->MainHUDWidget)
			{
				PC->MainHUDWidget->BP_ShowSystemMessage(TEXT("DEFEAT! AI dominates the Towers!"));
			}
		}

		BP_OnGameOver(1);
		return;
	}

	// 4. Standard turn switch
	if (CurrentTurn == ETurnState::Player0Turn)
	{
		CurrentTurn = ETurnState::Player1Turn;

		// Wake up the AI
		if (AIPlayer)
		{
			AIPlayer->ExecuteAITurn();
		}
	}
	else if (CurrentTurn == ETurnState::Player1Turn)
	{
		CurrentTurn = ETurnState::Player0Turn;
	}

	if (ATPlayerController* PC = Cast<ATPlayerController>(GetWorld()->GetFirstPlayerController()))
	{
		// Reset the internal selection state to prevent interaction bugs or stale pointer references
		PC->ClearSelectionState();

		// Synchronize global game state data with the HUD Widget
		if (PC->MainHUDWidget)
		{
			FString TurnName = (CurrentTurn == ETurnState::Player0Turn) ? TEXT("HUMAN PLAYER") : TEXT("AI PLAYER");
			PC->MainHUDWidget->BP_UpdateGlobalStatus(TurnName, TowersPlayer0, TowersPlayer1);
		}
	}

	// 5. Restore action points for all units for the new turn
	TArray<AActor*> AllUnits;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AUnit::StaticClass(), AllUnits);

	for (AActor* Actor : AllUnits)
	{
		AUnit* Unit = Cast<AUnit>(Actor);
		if (Unit)
		{
			Unit->ResetActions();
		}
	}
}

bool ATGameMode::IsMoveValid(AUnit* Unit, ATile* DestinationTile) const
{
	// Basic safety checks
	if (!Unit || !DestinationTile || !GameField || Unit->bHasMoved || Unit->bHasAttacked) return false;

	// Prevents "Ghost Moves." It is physically illegal to move to the tile the unit already occupies
	if (DestinationTile == Unit->CurrentTile) return false;

	// If the clicked tile is in the list calculated by the pathfinding algorithm, the move is legal
	TArray<ATile*> ReachableTiles = GameField->GetReachableTiles(Unit);

	return ReachableTiles.Contains(DestinationTile);
}

bool ATGameMode::IsAttackValid(AUnit* Attacker, AUnit* Defender) const
{
	// 1. Basic safety checks: Valid pointers, tiles, and attack state
	if (!Attacker || !Defender || !Attacker->CurrentTile || !Defender->CurrentTile || !GameField || Attacker->bHasAttacked) return false;

	// 2. Friendly Fire Restriction: Ensure units belong to opposing teams
	if (Attacker->TeamID == Defender->TeamID) return false;

	// 3. Elevation Constraint: Units cannot target enemies on higher elevation levels
	if (Defender->CurrentTile->GetElevation() > Attacker->CurrentTile->GetElevation()) return false;

	// 4. Distance Calculation: Validate Manhattan Distance against unit's AttackRange
	int32 Distance = GameField->GetManhattanDistance(Attacker->CurrentTile->GetGridPosition(), Defender->CurrentTile->GetGridPosition());
	if (Distance > Attacker->AttackRange) return false;

	// Calculate raycast start and end points with a vertical offset to clear the floor
	FVector StartLoc = Attacker->GetActorLocation() + FVector(0.f, 0.f, 75.f);
	FVector EndLoc = Defender->GetActorLocation() + FVector(0.f, 0.f, 75.f);

	FHitResult HitResult;
	FCollisionQueryParams CollisionParams;
	CollisionParams.AddIgnoredActor(Attacker);
	CollisionParams.AddIgnoredActor(Defender);
	if (Attacker->CurrentTile) CollisionParams.AddIgnoredActor(Attacker->CurrentTile);
	if (Defender->CurrentTile) CollisionParams.AddIgnoredActor(Defender->CurrentTile);

	// Perform visibility trace; if an obstacle is intercepted, the melee attack is blocked
	bool bHit = GetWorld()->LineTraceSingleByChannel(HitResult, StartLoc, EndLoc, ECC_Visibility, CollisionParams);
	if (bHit)
	{
		return false;
	}

	return true;
}

bool ATGameMode::HasAnyValidTarget(AUnit* Attacker) const
{
	if (!Attacker) return false;

	TArray<AActor*> AllUnits;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AUnit::StaticClass(), AllUnits);

	for (AActor* Actor : AllUnits)
	{
		AUnit* Enemy = Cast<AUnit>(Actor);
		// Check all living enemies on the opposing team
		if (IsValid(Enemy) && Enemy->TeamID != Attacker->TeamID && Enemy->CurrentTile)
		{
			if (IsAttackValid(Attacker, Enemy))
			{
				return true; // At least one valid target found; no need to continue
			}
		}
	}
	return false;
}

void ATGameMode::CheckWinCondition(AUnit* UnitToIgnore)
{
	// Victory is solely handled by Tower Domination in EndTurn().
}

void ATGameMode::CheckAutoEndTurn()
{
	if (bIsGameOver) return;

	TArray<AActor*> AllUnits;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AUnit::StaticClass(), AllUnits);

	bool bCanStillAct = false;
	int32 ActiveTeam = (CurrentTurn == ETurnState::Player0Turn) ? 0 : 1;

	for (AActor* Actor : AllUnits)
	{
		AUnit* Unit = Cast<AUnit>(Actor);
		if (!IsValid(Unit) || Unit->TeamID != ActiveTeam) continue;

		// Movement is always a valid action if the unit hasn't moved yet
		if (!Unit->bHasMoved)
		{
			bCanStillAct = true;
			break;
		}

		// Attack is only meaningful if the unit hasn't acted AND
		// at least one enemy is within valid attack range
		if (!Unit->bHasAttacked && HasAnyValidTarget(Unit))
		{
			bCanStillAct = true;
			break;
		}
	}

	if (!bCanStillAct)
	{
		if (ATPlayerController* PC = Cast<ATPlayerController>(GetWorld()->GetFirstPlayerController()))
		{
			if (PC->MainHUDWidget)
			{
				PC->MainHUDWidget->BP_ShowSystemMessage(TEXT("No valid actions remaining. Turn ended automatically."));
			}
		}
		EndTurn();
	}
}

void ATGameMode::RequestRespawn(TSubclassOf<class AUnit> UnitClass, int32 TeamID, ATile* OriginalTile)
{
	if (!GameField || !UnitClass || bIsGameOver) return;

	ATile* SpawnTile = OriginalTile;

	// If the original spawn tile is already occupied, locate the nearest available vacant tile
	if (SpawnTile && SpawnTile->OccupyingUnit)
	{
		FVector2D OrigPos = SpawnTile->GetGridPosition();
		SpawnTile = GameField->GetClosestValidTile(OrigPos.X, OrigPos.Y);
	}

	// Ensure the spawn tile is valid and currently unoccupied
	if (SpawnTile && !SpawnTile->OccupyingUnit)
	{
		FVector SpawnLoc = SpawnTile->GetActorLocation() + FVector(0.f, 0.f, 50.f);
		AUnit* NewUnit = GetWorld()->SpawnActor<AUnit>(UnitClass, SpawnLoc, FRotator::ZeroRotator);

		if (NewUnit)
		{
			// Initialize unit properties and update grid occupancy
			NewUnit->TeamID = TeamID;
			NewUnit->CurrentTile = SpawnTile;
			// Pass the original spawn point's memory to the new unit!
			NewUnit->InitialSpawnTile = OriginalTile;
			SpawnTile->OccupyingUnit = NewUnit;

			// Set initial facing direction and apply team-specific visuals
			NewUnit->SetActorRotation(FRotator(0.0f, (TeamID == 0) ? 180.0f : 0.0f, 0.0f));
			NewUnit->BP_ApplyTeamColor();
		}
	}
}

void ATGameMode::StartCoinToss()
{
	// Ensure unit archetypes are assigned in the Blueprint before proceeding
	if (!SniperClass || !BrawlerClass)
	{
		//if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Red, TEXT("ERROR: Sniper and Brawler classes must be assigned in BP_GameMode!"));
		return;
	}

	// Prevent the coin toss from executing multiple times
	if (P0_DeployQueue.Num() > 0 || P1_DeployQueue.Num() > 0 || MatchPhase != EMatchPhase::Setup)
	{
		return;
	}

	// Initialize deployment queues (1 Sniper, 1 Brawler per team)
	P0_DeployQueue.Add(SniperClass);
	P0_DeployQueue.Add(BrawlerClass);
	P1_DeployQueue.Add(SniperClass);
	P1_DeployQueue.Add(BrawlerClass);

	// Execute coin toss to determine initial turn order (0 or 1)
	FirstPlayerToAct = FMath::RandRange(0, 1);
	DeployingTeam = FirstPlayerToAct;
	MatchPhase = EMatchPhase::Setup;

	//  Highlight the deployment zone for the starting player
	if (GameField && DeployingTeam == 0)
	{
		GameField->HighlightDeploymentZone(DeployingTeam);
	}

	FString WinnerName = (FirstPlayerToAct == 0) ? TEXT("PLAYER 0 (HUMAN)") : TEXT("PLAYER 1 (AI)");

	if (ATPlayerController* PC = Cast<ATPlayerController>(GetWorld()->GetFirstPlayerController()))
	{
		if (PC->MainHUDWidget)
		{
			FString CoinTossMsg = FString::Printf(TEXT("COIN TOSS: %s starts deployment!"), *WinnerName);
			PC->MainHUDWidget->BP_ShowSystemMessage(CoinTossMsg);
		}
	}

	// If the AI wins the toss, immediately delegate deployment logic to the AI Controller
	if (DeployingTeam == 1)
	{
		if (ATAIController* AI = Cast<ATAIController>(UGameplayStatics::GetActorOfClass(GetWorld(), ATAIController::StaticClass())))
		{
			AI->ExecuteAIDeployment();
		}
	}
}

bool ATGameMode::RequestDeployment(ATile* TargetTile, int32 TeamID)
{
	// Strict safety validation: verify match phase, turn authority, tile validity, and obstructions
	if (MatchPhase != EMatchPhase::Setup || TeamID != DeployingTeam || !TargetTile || TargetTile->OccupyingUnit || TargetTile->bIsObstacle || TargetTile->GetElevation() == ETileElevation::Water) return false;

	// Deployment Zone Validation (As per GDD specifications)
	FVector2D Pos = TargetTile->GetGridPosition();
	if (TeamID == 0 && Pos.X < 22)
	{
		if (ATPlayerController* PC = Cast<ATPlayerController>(GetWorld()->GetFirstPlayerController()))
		{
			if (PC->MainHUDWidget)
			{
				PC->MainHUDWidget->BP_ShowSystemMessage(TEXT("WARNING: Human forces must be deployed within your starting zone!"));
			}
		}
		return false;
	}

	if (TeamID == 1 && Pos.X > 2) return false;

	// Reference the appropriate queue and extract the next unit class for instantiation
	TArray<TSubclassOf<AUnit>>* Queue = (TeamID == 0) ? &P0_DeployQueue : &P1_DeployQueue;
	if (Queue->Num() == 0) return false;

	TSubclassOf<AUnit> ClassToSpawn = (*Queue)[0];

	// Calculate spawn transform: adjust height for tile surface and rotate units to face the opponent
	FVector SpawnLoc = TargetTile->GetActorLocation() + FVector(0.f, 0.f, 50.f);
	AUnit* NewUnit = GetWorld()->SpawnActor<AUnit>(ClassToSpawn, SpawnLoc, FRotator(0.f, (TeamID == 0) ? 180.f : 0.f, 0.f));

	if (NewUnit)
	{
		// Initialize Unit state and persist the spawn tile reference for future respawn logic
		NewUnit->TeamID = TeamID;
		NewUnit->CurrentTile = TargetTile;
		NewUnit->InitialSpawnTile = TargetTile;
		TargetTile->OccupyingUnit = NewUnit;
		NewUnit->BP_ApplyTeamColor();

		Queue->RemoveAt(0); // Pop the unit from the deployment queue
		AdvanceDeployment();
		return true;
	}
	return false;
}

void ATGameMode::AdvanceDeployment()
{
	// Check if both players have exhausted their deployment queues
	if (P0_DeployQueue.Num() == 0 && P1_DeployQueue.Num() == 0)
	{
		// Transition to Battle phase and synchronize the initial turn with the coin toss winner
		MatchPhase = EMatchPhase::Battle;

		// Clear deployment highlights when battle starts
		if (GameField)
		{
			GameField->ClearHighlights();
		}

		CurrentTurn = (FirstPlayerToAct == 0) ? ETurnState::Player0Turn : ETurnState::Player1Turn;

		if (ATPlayerController* PC = Cast<ATPlayerController>(GetWorld()->GetFirstPlayerController()))
		{
			if (PC->MainHUDWidget)
			{
				PC->MainHUDWidget->BP_ShowGlobalState();

				FString TurnName = (CurrentTurn == ETurnState::Player0Turn) ? TEXT("HUMAN PLAYER") : TEXT("AI PLAYER");
				PC->MainHUDWidget->BP_UpdateGlobalStatus(TurnName, 0, 0);

				PC->MainHUDWidget->BP_ShowSystemMessage(TEXT("DEPLOYMENT COMPLETE! BATTLE HAS BEGUN."));
			}
		}

		// If it is the AI's turn to act first in battle, trigger the AI logic
		if (CurrentTurn == ETurnState::Player1Turn)
		{
			if (ATAIController* AI = Cast<ATAIController>(UGameplayStatics::GetActorOfClass(GetWorld(), ATAIController::StaticClass())))
			{
				AI->ExecuteAITurn();
			}
		}
		return;
	}

	// Toggle turn authority between players for the next deployment step
	DeployingTeam = (DeployingTeam == 0) ? 1 : 0;

	// Highlight the deployment zone for the next player
	if (GameField && DeployingTeam == 0)
	{
		GameField->HighlightDeploymentZone(DeployingTeam);
	}

	if (DeployingTeam == 1)
	{
		if (ATAIController* AI = Cast<ATAIController>(UGameplayStatics::GetActorOfClass(GetWorld(), ATAIController::StaticClass())))
		{
			// Apply a 0.5s delay to simulate AI processing time and prevent instantaneous spawning
			FTimerHandle AITimer;
			GetWorldTimerManager().SetTimer(AITimer, AI, &ATAIController::ExecuteAIDeployment, 0.5f, false);
		}
	}
}

void ATGameMode::LogAction(const FString& ActionString)
{
	MatchLog.Add(ActionString);

	// Update the UI if the PlayerController is valid
	if (ATPlayerController* PC = Cast<ATPlayerController>(GetWorld()->GetFirstPlayerController()))
	{
		if (PC->MainHUDWidget)
		{
			// Push the new log string to the UI widget
			PC->MainHUDWidget->BP_AddLogEntry(ActionString);
		}
	}

	// GEngine removed to maintain a production-ready UI.
	// This prevents debug strings from cluttering the viewport during gameplay loops.
	// if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Cyan, ActionString);
}