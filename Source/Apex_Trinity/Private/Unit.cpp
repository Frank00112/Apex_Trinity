// Fill out your copyright notice in the Description page of Project Settings.


#include "Unit.h"
#include "Tile.h"
#include "TGameMode.h"
#include "GameField.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/GameplayStatics.h"

AUnit::AUnit()
{
	// Ensure Tick is enabled to process the frame-by-frame movement animation
	PrimaryActorTick.bCanEverTick = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
	MeshComponent->SetupAttachment(RootComponent);

	// Generic default values (to be overridden by subclasses)
	Health = 100;
	MovementRange = 610; // Note: Ensure this aligns with your grid logic
	AttackRange = 1;
	MinAttackDamage = 1;
	MaxAttackDamage = 10;

	// Construct the Widget Component and attach it to the root
	HealthWidgetComp = CreateDefaultSubobject<UWidgetComponent>(TEXT("HealthWidgetComp"));
	HealthWidgetComp->SetupAttachment(RootComponent);

	// Set space to 'Screen' so the health bar always faces the player's camera (Billboard effect)
	HealthWidgetComp->SetWidgetSpace(EWidgetSpace::Screen);
	HealthWidgetComp->SetDrawSize(FVector2D(30.f, 7.f));

	// Position the widget at a vertical offset to hover above the unit's head
	HealthWidgetComp->SetRelativeLocation(FVector(0.f, 0.f, -20.f));
}

void AUnit::BeginPlay()
{
	Super::BeginPlay();

	// Synchronize current health with the maximum health value defined in the Blueprint
	Health = MaxHealth;

	// Initialize the health bar at 100% (1.0) immediately upon unit spawning
	BP_UpdateHealthBar(1.0f);
}

void AUnit::MoveToTile(ATile* TargetTile, bool bUseGreedyAlgorithm)
{
	// 1. Safety check for the target tile
	if (!IsValid(TargetTile))
	{
		return;
	}

	// 2. Access the GameField to utilize its pathfinding algorithms
	AGameField* Field = Cast<AGameField>(UGameplayStatics::GetActorOfClass(GetWorld(), AGameField::StaticClass()));
	if (IsValid(Field))
	{
		// 3. Selection of the algorithm (A* for humans, Greedy for AI) to satisfy Requirement 10
		TArray<ATile*> PathTiles;
		if (bUseGreedyAlgorithm)
		{
			PathTiles = Field->FindPathGreedy(CurrentTile, TargetTile);
		}
		else
		{
			PathTiles = Field->FindPath(CurrentTile, TargetTile);
		}

		// 4. Fallback logic: if the path is empty but the target is a direct neighbor
		if (PathTiles.Num() == 0)
		{
			PathTiles.Add(TargetTile);
		}

		// 5. Reset the waypoint system for the 3D animation
		Waypoints.Empty();
		FVector LastPos = GetActorLocation();

		// 6. Construct the "staircase" movement sequence to handle elevation changes (Z-axis)
		for (ATile* T : PathTiles)
		{
			// Calculate the center point on the tile surface with an offset for the pawn base
			FVector TileSurfaceLoc = T->GetActorLocation() + FVector(0.f, 0.f, 50.f);

			// Logic to prevent clipping through terrain during elevation changes [cite: 920]
			if (TileSurfaceLoc.Z > LastPos.Z + 10.f)
			{
				// Step Up: Move vertically first to clear the ledge, then translate forward
				Waypoints.Add(FVector(LastPos.X, LastPos.Y, TileSurfaceLoc.Z));
				Waypoints.Add(TileSurfaceLoc);
			}
			else if (TileSurfaceLoc.Z < LastPos.Z - 10.f)
			{
				// Step Down: Move forward to the edge first, then descend vertically
				Waypoints.Add(FVector(TileSurfaceLoc.X, TileSurfaceLoc.Y, LastPos.Z));
				Waypoints.Add(TileSurfaceLoc);
			}
			else
			{
				// Flat Terrain: Direct horizontal translation to the next tile center
				Waypoints.Add(TileSurfaceLoc);
			}

			// Update LastPos for the next segment calculation
			LastPos = TileSurfaceLoc;
		}

		// 7. Initialize movement state variables
		FinalTarget = TargetTile->GetActorLocation() + FVector(0.f, 0.f, 50.f);
		CurrentWaypointIndex = 0;
		bIsMoving = true;

		// Update the logical reference to the current tile
		CurrentTile = TargetTile;
	}
}

void AUnit::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (bIsMoving)
	{
		// Check if there are remaining segments (Waypoints) in the path sequence
		if (Waypoints.IsValidIndex(CurrentWaypointIndex))
		{
			FVector TargetWP = Waypoints[CurrentWaypointIndex];

			// Linearly interpolate the actor's position towards the current waypoint target
			SetActorLocation(FMath::VInterpConstantTo(GetActorLocation(), TargetWP, DeltaTime, MovementSpeed));

			// If the distance to the current waypoint is within tolerance (< 5.f), proceed to the next node
			if (FVector::Dist(GetActorLocation(), TargetWP) < 5.f)
			{
				CurrentWaypointIndex++;
			}
		}
		else
		{
			// All waypoints reached
			SetActorLocation(FinalTarget);


			// Coordinate logging
			if (AGameField* Field = Cast<AGameField>(UGameplayStatics::GetActorOfClass(GetWorld(), AGameField::StaticClass())))
			{
				FString Coord = Field->GetAlphanumericCoordinate(CurrentTile->GetGridPosition());
				//if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Emerald, FString::Printf(TEXT("Unità arrivata in: %s"), *Coord));
			}

			bIsMoving = false;
		}
	}
}

int32 AUnit::GetRandomAttackDamage() const
{
	// Returns a random integer between MinAttackDamage and MaxAttackDamage
	return FMath::RandRange(MinAttackDamage, MaxAttackDamage);
}

void AUnit::ResetActions()
{
	bHasMoved = false;
	bHasAttacked = false;
}

void AUnit::ReceiveDamage(int32 DamageAmount)
{
	Health -= DamageAmount;

	// Calculate the health percentage and dispatch the update signal to the UI.
	// The (MaxHealth > 0) check is a safety guard to prevent division-by-zero crashes.
	if (MaxHealth > 0)
	{
		float HealthPercent = (float)Health / (float)MaxHealth;
		// Clamp the percentage between 0.0 and 1.0 to ensure UI progress bar stability
		BP_UpdateHealthBar(FMath::Clamp(HealthPercent, 0.f, 1.f));
	}

	// Check for lethal damage
	if (Health <= 0)
	{
		Health = 0; // Ensure health doesn't drop below zero for UI consistency
		Die();
	}
}

void AUnit::Die()
{
	// 1. Release the tile occupancy
	if (CurrentTile)
	{
		CurrentTile->OccupyingUnit = nullptr;
	}

	// 2. Visual feedback event
	BP_OnDeath();

	// 3. Get the unit class and team to facilitate the respawn process
	TSubclassOf<AUnit> MyClass = this->GetClass();
	int32 MyTeam = TeamID;
	ATile* MyOriginalTile = InitialSpawnTile; // Retrieve the original spawn location from memory

	// 4. Notify the GameMode
	if (ATGameMode* GameMode = Cast<ATGameMode>(GetWorld()->GetAuthGameMode()))
	{

		// Request a replacement unit to be spawned at the original location
		GameMode->RequestRespawn(MyClass, MyTeam, MyOriginalTile);
	}

	// 5. Destroy actor
	Destroy();
}

void AUnit::FaceTargetLocation(FVector LookAtTarget)
{
	FVector StartLoc = GetActorLocation();

	// Calculate the necessary rotation to face the target using the updated parameter name
	FRotator LookAtRotation = UKismetMathLibrary::FindLookAtRotation(StartLoc, LookAtTarget);

	// Constrain the rotation to the Z-axis (Yaw) only. 
	FRotator FlatRotation = FRotator(0.0f, LookAtRotation.Yaw, 0.0f);

	// Apply the final rotation to the pawn
	SetActorRotation(FlatRotation);
}