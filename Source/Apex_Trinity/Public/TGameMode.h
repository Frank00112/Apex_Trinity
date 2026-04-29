// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "TGameMode.generated.h"

class AGameField;

// 1. Defines the active turn during the battle phase
UENUM(BlueprintType)
enum class ETurnState : uint8
{
	Player0Turn UMETA(DisplayName = "Player 0 Turn"),
	Player1Turn UMETA(DisplayName = "Player 1 Turn")
};

// 2. Defines the global game state (Deployment vs. Battle)
UENUM(BlueprintType)
enum class EMatchPhase : uint8
{
	Setup,
	Battle
};

UCLASS()
class APEX_TRINITY_API ATGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ATGameMode();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Game")
	AGameField* GameField;

	// Tracks the current state of the turn
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Turn System")
	ETurnState CurrentTurn;

	// Call this to forcefully end the current turn and pass it to the other player
	UFUNCTION(BlueprintCallable, Category = "Turn Logic")
	void EndTurn();

	//Evaluates the current game state to determine if a victory condition has been met */
	void CheckWinCondition();

	/** Counters for the victory condition: owning at least 2 towers for 2 consecutive turns */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Match")
	int32 Player0TowerTurns = 0;

	/** Counters for the victory condition: owning at least 2 towers for 2 consecutive turns */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Match")
	int32 Player1TowerTurns = 0;

	// UI Event to trigger "YOU WIN" or "YOU LOSE" screens in Blueprint */
	UFUNCTION(BlueprintImplementableEvent, Category = "Game Flow")
	void BP_OnGameOver(int32 WinningTeamID);

	// Reference to our dynamically spawned AI Manager
	UPROPERTY()
	class ATAIController* AIPlayer;

	// The referee evaluates whether a move is legal according to the game rules.
	bool IsMoveValid(class AUnit* Unit, class ATile* TargetTile) const;

	bool IsAttackValid(class AUnit* Attacker, class AUnit* Defender) const;

	// Flag used to disable player input and AI logic once the match has concluded
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State")
	bool bIsGameOver = false;

	// Updates the function signature to allow excluding a unit that has just died from the count
	void CheckWinCondition(class AUnit* UnitToIgnore = nullptr);

	// Evaluates if there are any remaining actions available for the active team
	void CheckAutoEndTurn();

	// Handles the respawn process after a unit is destroyed
	void RequestRespawn(TSubclassOf<class AUnit> UnitClass, int32 TeamID, class ATile* OriginalTile);

	// Tracks whether the game is in the positioning phase or the combat phase
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State")
	EMatchPhase MatchPhase = EMatchPhase::Setup;

	// Identifies the team currently placing a unit (0 = Human, 1 = AI)
	int32 DeployingTeam = -1;

	// Determines which player won the coin toss and will take the first action in battle
	int32 FirstPlayerToAct = -1;

	// Unit Blueprints: Assigned in the GameMode Blueprint to define specific unit archetypes
	UPROPERTY(EditDefaultsOnly, Category = "Roster")
	TSubclassOf<class AUnit> SniperClass;

	UPROPERTY(EditDefaultsOnly, Category = "Roster")
	TSubclassOf<class AUnit> BrawlerClass;

	// Virtual "Deployment Queues" containing the units yet to be placed by each player
	TArray<TSubclassOf<AUnit>> P0_DeployQueue;
	TArray<TSubclassOf<AUnit>> P1_DeployQueue;

	// Core system functions for game initialization and setup
	// Triggered by the UI button to start the match
	UFUNCTION(BlueprintCallable, Category = "Game Flow")
	void StartCoinToss();
	void AdvanceDeployment();
	bool RequestDeployment(class ATile* TargetTile, int32 TeamID);

	// Stores the formatted history of match actions based on GDD specs
	UPROPERTY(BlueprintReadOnly, Category = "Logging")
	TArray<FString> MatchLog;

	// Adds a formatted entry to the match log
	void LogAction(const FString& ActionString);

protected:
	virtual void BeginPlay() override;

	// Initializes the match logic after the map is completely generated
	void StartMatch();
};