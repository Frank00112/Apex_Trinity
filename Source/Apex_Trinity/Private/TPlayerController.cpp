// Fill out your copyright notice in the Description page of Project Settings.

#include "TPlayerController.h"
#include "Unit.h" 
#include "Tile.h" 
#include "Tower.h"
#include "TGameMode.h" 
#include "GameField.h"
#include "Sniper.h"
#include "Brawler.h"
#include "Kismet/GameplayStatics.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"

ATPlayerController::ATPlayerController()
{
	bShowMouseCursor = true;
	bEnableClickEvents = true;
	bEnableMouseOverEvents = true;
	bIsPlayer0Turn = true;
}

void ATPlayerController::BeginPlay()
{
	Super::BeginPlay();

	// 1. Input configuration: initialize the Enhanced Input subsystem and Mapping Context
	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		if (DefaultMappingContext)
		{
			Subsystem->AddMappingContext(DefaultMappingContext, 0);
		}
	}

	// 2. HUD initialization
	if (MainHUDClass)
	{
		// Instantiate the HUD widget and anchor it to the player's viewport
		MainHUDWidget = CreateWidget<UTMainHUD>(this, MainHUDClass);
		if (MainHUDWidget)
		{
			MainHUDWidget->AddToViewport();

			// Initialize the selection panel in a hidden state
			MainHUDWidget->BP_UpdateSelectionInfo(false, TEXT(""), 0, 0, 0, 0, 0);

			// 3. Configure input mode only after the HUD is created
			FInputModeGameAndUI InputMode;
			// Forces the game to immediately select the UI, preventing the double-click issue
			InputMode.SetWidgetToFocus(MainHUDWidget->TakeWidget());
			InputMode.SetHideCursorDuringCapture(false);
			SetInputMode(InputMode);
		}
	}
}

void ATPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(InputComponent))
	{
		if (ClickAction)
		{
			EnhancedInputComponent->BindAction(ClickAction, ETriggerEvent::Started, this, &ATPlayerController::OnLeftMouseClick);
		}
	}
}

// Forces a clean slate when the turn ends — prevents stale selection state across turns
void ATPlayerController::ClearSelectionState()
{
	ATGameMode* GameMode = Cast<ATGameMode>(UGameplayStatics::GetGameMode(GetWorld()));
	if (SelectedUnit && GameMode && GameMode->GameField)
	{
		GameMode->GameField->ClearHighlights();
		SelectedUnit = nullptr;
	}
	CurrentActionState = EActionState::Idle;
}

void ATPlayerController::OnLeftMouseClick()
{
	ATGameMode* GameMode = Cast<ATGameMode>(UGameplayStatics::GetGameMode(GetWorld()));

	// If GameMode is invalid or the match has concluded, ignore all mouse input
	if (!GameMode || GameMode->bIsGameOver) return;

	// Block all human input while the AI is executing its turn —
	// prevents accidental CheckAutoEndTurn calls that corrupt turn sequencing
	if (GameMode->MatchPhase == EMatchPhase::Battle && GameMode->CurrentTurn == ETurnState::Player1Turn)
	{
		return;
	}

	// PHASE 1: deployment
	if (GameMode->MatchPhase == EMatchPhase::Setup)
	{
		// Only process clicks when it is the human player's deployment turn
		if (GameMode->DeployingTeam == 0)
		{
			FHitResult Hit;
			GetHitResultUnderCursor(ECC_Visibility, false, Hit);
			ATile* ClickedTile = Cast<ATile>(Hit.GetActor());

			if (ClickedTile)
			{
				GameMode->RequestDeployment(ClickedTile, 0);
			}
		}
		// Abort immediately — combat and movement logic do not exist in the setup phase
		return;
	}

	// PHASE 2: battle
	FHitResult HitResult;
	bool bHit = GetHitResultUnderCursor(ECC_Visibility, false, HitResult);

	if (bHit && HitResult.GetActor())
	{
		AActor* ClickedActor = HitResult.GetActor();

		// CHECK A: did the player click on a Unit?
		AUnit* ClickedUnit = Cast<AUnit>(ClickedActor);
		if (ClickedUnit)
		{
			bIsPlayer0Turn = (GameMode->CurrentTurn == ETurnState::Player0Turn);
			bool bIsPlayer0Unit = (ClickedUnit->TeamID == 0);

			// CASE A1: select a friendly unit
			if ((bIsPlayer0Turn && bIsPlayer0Unit) || (!bIsPlayer0Turn && !bIsPlayer0Unit))
			{
				if (ClickedUnit->bHasMoved && ClickedUnit->bHasAttacked)
				{
					if (MainHUDWidget)
					{
						MainHUDWidget->BP_ShowSystemMessage(TEXT("WARNING: This unit has exhausted all actions."));
					}
					return;
				}

				// Clicking the already-selected unit does nothing
				if (SelectedUnit == ClickedUnit)
				{
					return;
				}

				CurrentActionState = EActionState::Idle;
				GameMode->GameField->ClearHighlights();
				SelectedUnit = ClickedUnit;

				// Dispatch the clicked unit's statistics to the HUD
				if (MainHUDWidget)
				{
					FString UName = SelectedUnit->IsA(ASniper::StaticClass()) ? TEXT("SNIPER") : TEXT("BRAWLER");
					MainHUDWidget->BP_UpdateSelectionInfo(true, UName, SelectedUnit->Health, SelectedUnit->MaxHealth,
						SelectedUnit->MinAttackDamage, SelectedUnit->MaxAttackDamage, SelectedUnit->AttackRange);
				}

				if (SelectedUnit->CurrentTile)
				{
					SelectedUnit->CurrentTile->BP_SetSelectionHighlight(true, SelectedUnit->TeamID);
				}
			}

			// CASE A2: attack execution — clicked an enemy while a friendly is selected
			else if (SelectedUnit)
			{
				// Guard: unit already spent its attack this turn
				if (SelectedUnit->bHasAttacked)
				{
					if (MainHUDWidget) MainHUDWidget->BP_ShowSystemMessage(TEXT("This unit has already attacked this turn!"));
					return;
				}

				// Guard: player must explicitly enter attack mode before clicking a target
				if (CurrentActionState != EActionState::PreparingAttack) return;

				if (GameMode->IsAttackValid(SelectedUnit, ClickedUnit))
				{
					// Face the target before resolving damage
					SelectedUnit->FaceTargetLocation(ClickedUnit->GetActorLocation());

					int32 DamageDealt = SelectedUnit->GetRandomAttackDamage();
					ClickedUnit->ReceiveDamage(DamageDealt);

					// GDD log: attack entry
					FString PlayerTag = (SelectedUnit->TeamID == 0) ? TEXT("HP") : TEXT("AI");
					FString UnitTag = SelectedUnit->IsA(ASniper::StaticClass()) ? TEXT("S") : TEXT("B");
					FString TargetCoord = GameMode->GameField->GetAlphanumericCoordinate(ClickedUnit->CurrentTile->GetGridPosition());
					FString LogStr = FString::Printf(TEXT("%s: %s %s %d"), *PlayerTag, *UnitTag, *TargetCoord, DamageDealt);
					GameMode->LogAction(LogStr);

					// Counterattack: only the Sniper receives return fire per GDD spec
					if (SelectedUnit->IsA(ASniper::StaticClass()))
					{
						bool bTargetIsSniper = ClickedUnit->IsA(ASniper::StaticClass());
						bool bTargetIsBrawler = ClickedUnit->IsA(ABrawler::StaticClass());
						int32 Dist = GameMode->GameField->GetManhattanDistance(
							SelectedUnit->CurrentTile->GetGridPosition(),
							ClickedUnit->CurrentTile->GetGridPosition());

						// Conditions: target is a Sniper, or a Brawler at distance 1
						if (bTargetIsSniper || (bTargetIsBrawler && Dist == 1))
						{
							int32 CounterDamage = FMath::RandRange(1, 3);
							SelectedUnit->ReceiveDamage(CounterDamage);

							// Notify the player that their Sniper was hit back
							if (MainHUDWidget)
							{
								FString CounterMsg = FString::Printf(
									TEXT("COUNTERATTACK! Your Sniper took %d damage!"), CounterDamage);
								MainHUDWidget->BP_ShowSystemMessage(CounterMsg);
							}

							// Log only if the Sniper survived — ReceiveDamage may destroy it
							if (IsValid(SelectedUnit) && SelectedUnit->CurrentTile)
							{
								FString CounterCoord = GameMode->GameField->GetAlphanumericCoordinate(
									SelectedUnit->CurrentTile->GetGridPosition());
								FString CounterLog = FString::Printf(
									TEXT("%s: %s %s Counterattack %d"),
									(ClickedUnit->TeamID == 0) ? TEXT("HP") : TEXT("AI"),
									ClickedUnit->IsA(ASniper::StaticClass()) ? TEXT("S") : TEXT("B"),
									*CounterCoord,
									CounterDamage);
								GameMode->LogAction(CounterLog);
							}
						}
					}

					// Update action flags if the unit survived the exchange
					if (IsValid(SelectedUnit))
					{
						SelectedUnit->bHasAttacked = true;
						SelectedUnit->bHasMoved = true;
					}

					// Clean up UI state and check if the turn should end
					GameMode->GameField->ClearHighlights();
					SelectedUnit = nullptr;
					CurrentActionState = EActionState::Idle;
					GameMode->CheckAutoEndTurn();
				}
				else
				{
					// Provide specific feedback explaining why the attack was rejected
					if (ClickedUnit->CurrentTile->GetElevation() > SelectedUnit->CurrentTile->GetElevation())
					{
						if (MainHUDWidget) MainHUDWidget->BP_ShowSystemMessage(TEXT("Target is on higher ground. Attack impossible!"));
					}
					else
					{
						int32 Distance = GameMode->GameField->GetManhattanDistance(
							SelectedUnit->CurrentTile->GetGridPosition(),
							ClickedUnit->CurrentTile->GetGridPosition());

						if (Distance > SelectedUnit->AttackRange)
						{
							if (MainHUDWidget) MainHUDWidget->BP_ShowSystemMessage(TEXT("Target is out of range!"));
						}
						else
						{
							if (MainHUDWidget) MainHUDWidget->BP_ShowSystemMessage(TEXT("Line of sight obstructed by an obstacle or unit!"));
						}
					}
				}
			}
			return;
		}

		// CHECK B: did the player click on a Tile to move?
		ATile* ClickedTile = Cast<ATile>(ClickedActor);
		if (ClickedTile)
		{
			// Reject clicks on impassable terrain immediately
			if (ClickedTile->bIsObstacle || ClickedTile->GetElevation() == ETileElevation::Water)
			{
				if (MainHUDWidget) MainHUDWidget->BP_ShowSystemMessage(TEXT("WARNING: Cannot move onto Towers, Borders, or Water!"));
				return;
			}

			if (SelectedUnit)
			{
				// Guard: movement already used this turn
				if (SelectedUnit->bHasMoved)
				{
					if (MainHUDWidget) MainHUDWidget->BP_ShowSystemMessage(TEXT("This unit has already moved this turn!"));
					return;
				}

				// Guard: player must explicitly enter move mode before clicking a destination
				if (CurrentActionState != EActionState::PreparingMove)
				{
					if (MainHUDWidget) MainHUDWidget->BP_ShowSystemMessage(TEXT("WARNING: Select the MOVE action before moving!"));
					return;
				}

				if (GameMode->IsMoveValid(SelectedUnit, ClickedTile))
				{
					GameMode->GameField->ClearHighlights();

					// Cache the origin tile coordinate before the unit is moved
					FVector2D OriginPos = SelectedUnit->CurrentTile->GetGridPosition();

					if (SelectedUnit->CurrentTile)
					{
						SelectedUnit->CurrentTile->OccupyingUnit = nullptr;
					}

					SelectedUnit->FaceTargetLocation(ClickedTile->GetActorLocation());
					SelectedUnit->MoveToTile(ClickedTile);

					// GDD log: movement entry
					FVector2D DestPos = ClickedTile->GetGridPosition();
					FString PlayerTag = (SelectedUnit->TeamID == 0) ? TEXT("HP") : TEXT("AI");
					FString UnitTag = SelectedUnit->IsA(ASniper::StaticClass()) ? TEXT("S") : TEXT("B");
					FString OriginStr = GameMode->GameField->GetAlphanumericCoordinate(OriginPos);
					FString DestStr = GameMode->GameField->GetAlphanumericCoordinate(DestPos);
					FString LogStr = FString::Printf(TEXT("%s: %s %s -> %s"), *PlayerTag, *UnitTag, *OriginStr, *DestStr);
					GameMode->LogAction(LogStr);

					// Re-evaluate tower control immediately after every movement
					TArray<AActor*> FoundTowers;
					UGameplayStatics::GetAllActorsOfClass(GetWorld(), ATower::StaticClass(), FoundTowers);
					for (AActor* T : FoundTowers)
					{
						if (ATower* TowerActor = Cast<ATower>(T)) TowerActor->EvaluateControlState();
					}

					ClickedTile->OccupyingUnit = SelectedUnit;
					SelectedUnit->bHasMoved = true;
					CurrentActionState = EActionState::Idle;

					if (SelectedUnit->bHasAttacked)
					{
						// Unit attacked before moving — no actions left
						SelectedUnit = nullptr;
					}
					else if (!GameMode->HasAnyValidTarget(SelectedUnit))
					{
						// Moved into a position with no valid targets: auto-skip the attack slot
						SelectedUnit->bHasAttacked = true;
						if (MainHUDWidget)
						{
							MainHUDWidget->BP_ShowSystemMessage(TEXT("No enemies in attack range. Attack skipped automatically."));
						}
						SelectedUnit = nullptr;
						GameMode->CheckAutoEndTurn();
					}
					else
					{
						// Valid targets exist — keep the unit selected for the attack step
						SelectedUnit->CurrentTile->BP_SetSelectionHighlight(true, SelectedUnit->TeamID);
					}
				}
				else
				{
					if (MainHUDWidget) MainHUDWidget->BP_ShowSystemMessage(TEXT("Target tile is out of movement range!"));
				}
			}
			return;
		}

		GameMode->CheckAutoEndTurn();
	}

	// CHECK C: player clicked empty space — deselect the current unit
	if (SelectedUnit)
	{
		GameMode->GameField->ClearHighlights();
		SelectedUnit = nullptr;
		CurrentActionState = EActionState::Idle;

		if (MainHUDWidget)
		{
			MainHUDWidget->BP_UpdateSelectionInfo(false, TEXT(""), 0, 0, 0, 0, 0);
		}
	}
}

void ATPlayerController::PrepareMove()
{
	ATGameMode* GameMode = Cast<ATGameMode>(GetWorld()->GetAuthGameMode());
	if (!SelectedUnit) return;

	// Toggle off if move mode is already active
	if (CurrentActionState == EActionState::PreparingMove)
	{
		GameMode->GameField->ClearHighlights();
		SelectedUnit->CurrentTile->BP_SetSelectionHighlight(true, SelectedUnit->TeamID);
		CurrentActionState = EActionState::Idle;
		return;
	}

	// Silently ignore the request if the unit has already moved
	if (SelectedUnit->bHasMoved) return;

	if (GameMode && GameMode->GameField && SelectedUnit->CurrentTile)
	{
		GameMode->GameField->ClearHighlights();
		SelectedUnit->CurrentTile->BP_SetSelectionHighlight(true, SelectedUnit->TeamID);
		GameMode->GameField->HighlightMovementTiles(SelectedUnit);
	}
	CurrentActionState = EActionState::PreparingMove;
}

void ATPlayerController::PrepareAttack()
{
	ATGameMode* GameMode = Cast<ATGameMode>(GetWorld()->GetAuthGameMode());
	if (!SelectedUnit) return;

	// Toggle off if attack mode is already active
	if (CurrentActionState == EActionState::PreparingAttack)
	{
		GameMode->GameField->ClearHighlights();
		SelectedUnit->CurrentTile->BP_SetSelectionHighlight(true, SelectedUnit->TeamID);
		CurrentActionState = EActionState::Idle;
		return;
	}

	// Silently ignore the request if the unit has already attacked
	if (SelectedUnit->bHasAttacked) return;

	if (GameMode && GameMode->GameField && SelectedUnit->CurrentTile)
	{
		if (!GameMode->HasAnyValidTarget(SelectedUnit))
		{
			if (MainHUDWidget)
			{
				MainHUDWidget->BP_ShowSystemMessage(TEXT("No enemy units in attack range or line of sight!"));
			}
			// Keep the selection highlight visible since we're staying in Idle state
			SelectedUnit->CurrentTile->BP_SetSelectionHighlight(true, SelectedUnit->TeamID);
			return;
		}

		GameMode->GameField->ClearHighlights();
		SelectedUnit->CurrentTile->BP_SetSelectionHighlight(true, SelectedUnit->TeamID);
		GameMode->GameField->HighlightAttackableTiles(SelectedUnit);
	}
	CurrentActionState = EActionState::PreparingAttack;
}