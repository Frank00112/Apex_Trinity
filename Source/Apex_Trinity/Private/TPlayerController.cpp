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

			// Initialize the selection panel in a hidden state.
			MainHUDWidget->BP_UpdateSelectionInfo(false, TEXT(""), 0, 0, 0, 0, 0);

			// 3. Configure input mode only after the HUD is created
			FInputModeGameAndUI InputMode;
			// This line forces the game to immediately select the UI, preventing the double-click issue
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

// Function to force-clear the selection when the turn ends
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

	// If GameMode is invalid or the match has concluded, ignore mouse input
	if (!GameMode || GameMode->bIsGameOver) return;

	// PHASE 1: Deployment
	if (GameMode->MatchPhase == EMatchPhase::Setup)
	{
		// Check if it is currently the human player's turn to deploy a unit
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
		// Instantly abort the function: combat and movement logic do not exist in the setup phase
		return;
	}

	// PHASE 2: Battle 
	FHitResult HitResult;
	bool bHit = GetHitResultUnderCursor(ECC_Visibility, false, HitResult);

	if (bHit && HitResult.GetActor())
	{
		AActor* ClickedActor = HitResult.GetActor();

		// CHECK A: Did the player click on a Unit?
		AUnit* ClickedUnit = Cast<AUnit>(ClickedActor);
		if (ClickedUnit)
		{
			bIsPlayer0Turn = (GameMode->CurrentTurn == ETurnState::Player0Turn);
			bool bIsPlayer0Unit = (ClickedUnit->TeamID == 0);

			// CASE A1: Select a friendly unit
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

				if (SelectedUnit == ClickedUnit)
				{
					return;
				}

				CurrentActionState = EActionState::Idle;
				GameMode->GameField->ClearHighlights();
				SelectedUnit = ClickedUnit;

				// HUD update: dispatch the clicked unit's statistics to the interface
				if (MainHUDWidget)
				{
					// NOTE: determine unit type for display and pass all tactical attributes to the Blueprint event
					FString UName = SelectedUnit->IsA(ASniper::StaticClass()) ? TEXT("SNIPER") : TEXT("BRAWLER");

					MainHUDWidget->BP_UpdateSelectionInfo(true, UName, SelectedUnit->Health, SelectedUnit->MaxHealth, SelectedUnit->MinAttackDamage, SelectedUnit->MaxAttackDamage, SelectedUnit->AttackRange);
				}

				if (SelectedUnit->CurrentTile)
				{
					SelectedUnit->CurrentTile->BP_SetSelectionHighlight(true, SelectedUnit->TeamID);
				}
			}

			// CASE A2: Attack execution 
			else if (SelectedUnit) 
			{
				// 1. Check if the unit has already attacked
				if (SelectedUnit->bHasAttacked)
				{
					if (MainHUDWidget) MainHUDWidget->BP_ShowSystemMessage(TEXT("This unit has already attacked this turn!"));
					return;
				}

				if (CurrentActionState != EActionState::PreparingAttack) return;

				// 2. Evaluate if the attack is valid via backend GameMode
				if (GameMode->IsAttackValid(SelectedUnit, ClickedUnit))
				{
					// Rotate unit to face the opponent before applying damage
					SelectedUnit->FaceTargetLocation(ClickedUnit->GetActorLocation());

					int32 DamageDealt = SelectedUnit->GetRandomAttackDamage();
					ClickedUnit->ReceiveDamage(DamageDealt);

					// GDD logging: attack
					FString PlayerTag = (SelectedUnit->TeamID == 0) ? TEXT("HP") : TEXT("AI");
					FString UnitTag = SelectedUnit->IsA(ASniper::StaticClass()) ? TEXT("S") : TEXT("B");
					FString TargetCoord = GameMode->GameField->GetAlphanumericCoordinate(ClickedUnit->CurrentTile->GetGridPosition());

					// Format: [Player] [Unit] [TargetCoord] [Damage]
					FString LogStr = FString::Printf(TEXT("%s: %s %s %d"), *PlayerTag, *UnitTag, *TargetCoord, DamageDealt);
					GameMode->LogAction(LogStr);

					// CounterAttack logic
					if (SelectedUnit->IsA(ASniper::StaticClass()))
					{
						bool bTargetIsSniper = ClickedUnit->IsA(ASniper::StaticClass());
						bool bTargetIsBrawler = ClickedUnit->IsA(ABrawler::StaticClass());
						int32 Dist = GameMode->GameField->GetManhattanDistance(SelectedUnit->CurrentTile->GetGridPosition(), ClickedUnit->CurrentTile->GetGridPosition());

						// Execute counter-damage if the target is a Sniper or an adjacent Brawler
						if (bTargetIsSniper || (bTargetIsBrawler && Dist == 1))
						{
							int32 CounterDamage = FMath::RandRange(1, 3);
							SelectedUnit->ReceiveDamage(CounterDamage);

							// Log the counter-attack in a separate line as instructed
							FString CounterLog = FString::Printf(TEXT("%s: %s Counterattack %d"),
								(SelectedUnit->TeamID == 0) ? TEXT("HP") : TEXT("AI"),
								SelectedUnit->IsA(ASniper::StaticClass()) ? TEXT("S") : TEXT("B"),
								CounterDamage);

							GameMode->LogAction(CounterLog);
						}
					}

					// Update unit flags if it survived the counter-engagement
					if (IsValid(SelectedUnit))
					{
						SelectedUnit->bHasAttacked = true;
						SelectedUnit->bHasMoved = true;
					}

					// Cleanup UI highlights and reset selection state
					GameMode->GameField->ClearHighlights();
					SelectedUnit = nullptr;
					CurrentActionState = EActionState::Idle;

					// Evaluate if the turn should automatically conclude
					GameMode->CheckAutoEndTurn();
				}
				else
				{
					// 3. Diagnose WHY the attack failed to provide specific UI feedback
					if (ClickedUnit->CurrentTile->GetElevation() > SelectedUnit->CurrentTile->GetElevation())
					{
						if (MainHUDWidget) MainHUDWidget->BP_ShowSystemMessage(TEXT("Target is on higher ground. Attack impossible!"));
					}
					else
					{
						int32 Distance = GameMode->GameField->GetManhattanDistance(SelectedUnit->CurrentTile->GetGridPosition(), ClickedUnit->CurrentTile->GetGridPosition());
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

		// CHECK B: Did the player click on a Tile to Move?
		ATile* ClickedTile = Cast<ATile>(ClickedActor);
		if (ClickedTile)
		{
			// Prevent interacting with physical obstacles (Towers, Borders) or Water
			if (ClickedTile->bIsObstacle || ClickedTile->GetElevation() == ETileElevation::Water)
			{
				if (MainHUDWidget) MainHUDWidget->BP_ShowSystemMessage(TEXT("WARNING: Cannot move onto Towers, Borders, or Water!"));
				return;
			}

			if (SelectedUnit)
			{
				// 1. Check if the unit has already moved
				if (SelectedUnit->bHasMoved)
				{
					if (MainHUDWidget) MainHUDWidget->BP_ShowSystemMessage(TEXT("This unit has already moved this turn!"));
					return;
				}

				if (CurrentActionState != EActionState::PreparingMove)
				{
					if (MainHUDWidget) MainHUDWidget->BP_ShowSystemMessage(TEXT("WARNING: Select the MOVE action before moving!"));
					return;
				}

				// 2. Validate the movement via GameMode
				if (GameMode->IsMoveValid(SelectedUnit, ClickedTile))
				{
					GameMode->GameField->ClearHighlights();

					// GDD logging: pre-movement
					FVector2D OriginPos = SelectedUnit->CurrentTile->GetGridPosition();

					if (SelectedUnit->CurrentTile)
					{
						SelectedUnit->CurrentTile->OccupyingUnit = nullptr;
					}

					SelectedUnit->FaceTargetLocation(ClickedTile->GetActorLocation());
					SelectedUnit->MoveToTile(ClickedTile);

					// GDD logging: post-movement
					FVector2D DestPos = ClickedTile->GetGridPosition();

					FString PlayerTag = (SelectedUnit->TeamID == 0) ? TEXT("HP") : TEXT("AI");
					FString UnitTag = SelectedUnit->IsA(ASniper::StaticClass()) ? TEXT("S") : TEXT("B");
					FString OriginStr = GameMode->GameField->GetAlphanumericCoordinate(OriginPos);
					FString DestStr = GameMode->GameField->GetAlphanumericCoordinate(DestPos);

					FString LogStr = FString::Printf(TEXT("%s: %s %s -> %s"), *PlayerTag, *UnitTag, *OriginStr, *DestStr);
					GameMode->LogAction(LogStr);

					// Forcing towers to check their state immediately after a movement
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
						SelectedUnit = nullptr;
					}
					else
					{
						SelectedUnit->CurrentTile->BP_SetSelectionHighlight(true, SelectedUnit->TeamID);
					}
				}
				else
				{
					// 3. Feedback for invalid movement clicks
					if (MainHUDWidget) MainHUDWidget->BP_ShowSystemMessage(TEXT("Target tile is out of movement range!"));
				}
			}
			return;
		}
		GameMode->CheckAutoEndTurn();
	}

	// CHECK C: Clicked in the void
	if (SelectedUnit)
	{
		GameMode->GameField->ClearHighlights();
		SelectedUnit = nullptr;
		CurrentActionState = EActionState::Idle;

		// HUD update: hide data
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

	// Toggle off if already on
	if (CurrentActionState == EActionState::PreparingMove)
	{
		GameMode->GameField->ClearHighlights();
		SelectedUnit->CurrentTile->BP_SetSelectionHighlight(true, SelectedUnit->TeamID);
		CurrentActionState = EActionState::Idle;
		return;
	}

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

	// Toggle off if already on
	if (CurrentActionState == EActionState::PreparingAttack)
	{
		GameMode->GameField->ClearHighlights();
		SelectedUnit->CurrentTile->BP_SetSelectionHighlight(true, SelectedUnit->TeamID);
		CurrentActionState = EActionState::Idle;
		return;
	}

	if (SelectedUnit->bHasAttacked) return;

	if (GameMode && GameMode->GameField && SelectedUnit->CurrentTile)
	{
		GameMode->GameField->ClearHighlights();
		SelectedUnit->CurrentTile->BP_SetSelectionHighlight(true, SelectedUnit->TeamID);
		GameMode->GameField->HighlightAttackableTiles(SelectedUnit);
	}
	CurrentActionState = EActionState::PreparingAttack;
}