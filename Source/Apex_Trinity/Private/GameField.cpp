// Fill out your copyright notice in the Description page of Project Settings.

#include "GameField.h"
#include "Tower.h"
#include "Unit.h"
#include "Tile.h"
#include "TGameMode.h"
#include "TGameMode3D.h"
#include "TPlayerController.h"
#include "Math/UnrealMathUtility.h"
#include "Kismet/GameplayStatics.h"

AGameField::AGameField()
{
	PrimaryActorTick.bCanEverTick = false;
	Size = 25;
	TileSize = 100.f;
	CellPadding = 0.05f;
}

void AGameField::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	NextCellPositionMultiplier = (TileSize + (TileSize * CellPadding)) / TileSize;
}

void AGameField::BeginPlay()
{
	Super::BeginPlay();
	GenerateField();
}

void AGameField::ClearField()
{
	// 1. Locate and destroy every tile actor present in the world
	TArray<AActor*> OldTiles;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ATile::StaticClass(), OldTiles);
	for (AActor* OldTile : OldTiles)
	{
		if (IsValid(OldTile))
		{
			OldTile->Destroy();
		}
	}

	// 2. Locate and destroy any remaining tower actors
	TArray<AActor*> OldTowers;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ATower::StaticClass(), OldTowers);
	for (AActor* OldTower : OldTowers)
	{
		if (IsValid(OldTower))
		{
			OldTower->Destroy();
		}
	}

	// 3. Reset the internal state of the class to prepare for a fresh generation cycle
	TileArray.Empty();
	TileMap.Empty();
}

void AGameField::GenerateField()
{
	// Completely clear the existing map
	ClearField();

	if (!TileClass)
	{
		//UE_LOG(LogTemp, Error, TEXT("TileClass not assigned in AGameField!"));
		return;
	}

	const float RandomSeedX = FMath::RandRange(-10000.f, 10000.f);
	const float RandomSeedY = FMath::RandRange(-10000.f, 10000.f);

	// Check if the current game mode is 3D to determine border scaling later
	bool bIs3D = GetWorld()->GetAuthGameMode()->IsA(ATGameMode3D::StaticClass());

	// Extended loop to include outer bounds (-1 and Size) to create a protective border
	for (int32 X = -1; X <= Size; X++)
	{
		for (int32 Y = -1; Y <= Size; Y++)
		{
			// 1. Check if we are on the mathematical border
			if (X == -1 || X == Size || Y == -1 || Y == Size)
			{
				// Spawn a single tile for the border at base elevation (Z=0)
				FVector BorderLocation = GetRelativeLocationByXYPosition(X, Y, 0);

				ATile* BorderTile = GetWorld()->SpawnActor<ATile>(TileClass, BorderLocation, FRotator::ZeroRotator);
				if (IsValid(BorderTile))
				{
					BorderTile->SetGridPosition(X, Y);
					BorderTile->SetElevation(ETileElevation::Mountain3);
					BorderTile->bIsObstacle = true;
					
					// Disable dynamic shadows for the outer border to save GPU resources
					UStaticMeshComponent* Mesh = BorderTile->FindComponentByClass<UStaticMeshComponent>();
					if (Mesh) Mesh->SetCastShadow(false);
					
					BorderTile->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
					BorderTile->BP_ApplyBorderMaterial();

					// If in 3D mode, scale the single tile vertically to create a wall
					if (bIs3D)
					{
						BorderTile->SetActorScale3D(FVector(1.0f, 1.0f, 1.75f));

						// Local offset adjusts the Z position
						BorderTile->AddActorLocalOffset(FVector(0.f, 0.f, 80.f)); 
					}
				}
			}
			else
			{
				// 2. Standard playable tile generation

				// Sanitization: prevent the NoiseScale slider from collapsing the noise generator by avoiding zero or negative values
				// A scale of 0.0 would result in a flat, uniform map; FMath::Max ensures a functional minimum
				float SafeScale = FMath::Max(0.1f, NoiseScale);

				// Utilize SafeScale to calculate the Perlin Noise value for the current grid coordinates (X, Y).
				// The RandomSeed offset ensures unique map generation patterns for every execution.
				float NoiseVal = FMath::PerlinNoise2D(FVector2D((X * SafeScale) + RandomSeedX, (Y * SafeScale) + RandomSeedY));
				int32 ElevationStep = QuantizePerlinNoise(NoiseVal);

				// Prevent water lvl or high mountains lvl in spawn areas
				// Assuming Player0 deploys on X >= Size - 3 and Player1 on X 0,1,2
				if (X <= 2 || X >= Size - 3)
				{
					ElevationStep = FMath::Clamp(ElevationStep, 1, 2);
				}

				FVector SpawnLocation = GetRelativeLocationByXYPosition(X, Y, ElevationStep);

				ATile* NewTile = GetWorld()->SpawnActor<ATile>(TileClass, SpawnLocation, FRotator::ZeroRotator);
				if (IsValid(NewTile))
				{
					NewTile->SetGridPosition(X, Y);
					NewTile->SetElevation(static_cast<ETileElevation>(ElevationStep));

					// Only water lvl is a logical obstacle
					if (ElevationStep == 0)
					{
						NewTile->bIsObstacle = true;
					}

					TileArray.Add(NewTile);
					TileMap.Add(FVector2D(X, Y), NewTile);

					NewTile->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
				}
			}
		}
	}

	SpawnTowers();
}

void AGameField::SpawnTowers()
{
	if (!TowerClass) return;

	TArray<FVector2D> IdealPositions = {
		FVector2D(12, 12),
		FVector2D(12, 5),
		FVector2D(12, 19)
	};

	for (FVector2D TargetPos : IdealPositions)
	{
		ATile* BestTile = GetClosestValidTile(TargetPos.X, TargetPos.Y);

		if (BestTile)
		{
			FVector SpawnLoc = BestTile->GetActorLocation();
			SpawnLoc.Z += 50.f;

			ATower* NewTower = GetWorld()->SpawnActor<ATower>(TowerClass, SpawnLoc, FRotator::ZeroRotator);
			if (NewTower)
			{
				NewTower->OwningTile = BestTile;
				BestTile->bIsObstacle = true;
				NewTower->AttachToActor(BestTile, FAttachmentTransformRules::KeepWorldTransform);
			}
		}
	}
}

ATile* AGameField::GetClosestValidTile(int32 TargetX, int32 TargetY)
{
	ATile* BestTile = nullptr;
	int32 MinDist = 9999;

	for (ATile* Tile : TileArray)
	{
		if (IsValid(Tile))
		{
			if (Tile->GetElevation() != ETileElevation::Water && !Tile->bIsObstacle)
			{
				FVector2D Pos = Tile->GetGridPosition();
				int32 Dist = FMath::Abs(Pos.X - TargetX) + FMath::Abs(Pos.Y - TargetY);

				if (Dist < MinDist)
				{
					MinDist = Dist;
					BestTile = Tile;

					if (Dist == 0) break;
				}
			}
		}
	}
	return BestTile;
}

void AGameField::HighlightMovementTiles(AUnit* SelectedUnit)
{
	if (!SelectedUnit) return;
	TArray<ATile*> ValidTiles = GetReachableTiles(SelectedUnit);

	for (ATile* Tile : ValidTiles)
	{
		if (Tile == SelectedUnit->CurrentTile) continue;
		Tile->BP_SetMovementHighlight(true);
	}
}

void AGameField::HighlightAttackableTiles(AUnit* SelectedUnit)
{
	if (!SelectedUnit || !SelectedUnit->CurrentTile) return;

	FVector2D UnitPos = SelectedUnit->CurrentTile->GetGridPosition();
	int32 Range = SelectedUnit->AttackRange;
	ETileElevation AttackerElev = SelectedUnit->CurrentTile->GetElevation();

	for (ATile* Tile : TileArray)
	{
		if (Tile == SelectedUnit->CurrentTile) continue;
		if (!Tile->OccupyingUnit) continue; // Only highlight tiles with an enemy on them

		// Enemy check
		if(Tile->OccupyingUnit->TeamID == SelectedUnit->TeamID) continue;

		// Elevation constraint: cannot highlight tiles on higher ground
		if (Tile->GetElevation() > AttackerElev) continue;

		int32 Distance = FMath::Abs(UnitPos.X - Tile->GetGridPosition().X) + FMath::Abs(UnitPos.Y - Tile->GetGridPosition().Y);

		if (Distance > 0 && Distance <= Range)
		{
			Tile->BP_SetAttackHighlight(true);
		}
	}
}

void AGameField::HighlightDeploymentZone(int32 TeamID)
{
	ClearHighlights();

	for (ATile* Tile : TileArray)
	{
		if (!Tile || Tile->bIsObstacle || Tile->GetElevation() == ETileElevation::Water || Tile->OccupyingUnit) continue;

		FVector2D Pos = Tile->GetGridPosition();

		// Player0 (human player) allowed in rows 22, 23, 24
		if (TeamID == 0 && Pos.X >= 22)
		{
			Tile->BP_SetMovementHighlight(true);
		}
		// Player1 (AI player) allowed in rows 0, 1, 2
		else if (TeamID == 1 && Pos.X <= 2)
		{
			Tile->BP_SetMovementHighlight(true);
		}
	}
}

void AGameField::ClearHighlights()
{
	for (ATile* Tile : TileArray)
	{
		Tile->BP_SetMovementHighlight(false);
		Tile->BP_SetAttackHighlight(false);
		Tile->BP_SetSelectionHighlight(false, -1);
	}
}

FVector AGameField::GetRelativeLocationByXYPosition(const int32 InX, const int32 InY, const int32 InZ) const
{
	float PosX = TileSize * NextCellPositionMultiplier * InX;
	float PosY = TileSize * NextCellPositionMultiplier * InY;
	// Default 3D elevation
	float PosZ = InZ * 40.f;

	// If the game is in 2D mode, override Z to 0 to ensure a flat plane
	if (GetWorld() && !GetWorld()->GetAuthGameMode()->IsA(ATGameMode3D::StaticClass()))
	{
		PosZ = 0.f;
	}

	return GetActorLocation() + FVector(PosX, PosY, PosZ);
}

int32 AGameField::GetManhattanDistance(const FVector2D& Start, const FVector2D& End) const
{
	int32 DiffX = FMath::Abs(FMath::RoundToInt(End.X - Start.X));
	int32 DiffY = FMath::Abs(FMath::RoundToInt(End.Y - Start.Y));
	return DiffX + DiffY;
}

TArray<ATile*> AGameField::GetAdjacentTiles(ATile* CenterTile) const
{
	TArray<ATile*> Neighbors;
	if (!CenterTile) return Neighbors;

	FVector2D CenterPos = CenterTile->GetGridPosition();

	for (ATile* Tile : TileArray)
	{
		FVector2D Pos = Tile->GetGridPosition();
		if (FMath::Abs(Pos.X - CenterPos.X) + FMath::Abs(Pos.Y - CenterPos.Y) == 1)
		{
			Neighbors.Add(Tile);
		}
	}
	return Neighbors;
}

TArray<ATile*> AGameField::GetReachableTiles(AUnit* Unit) const
{
	TArray<ATile*> Reachable;
	if (!Unit || !Unit->CurrentTile) return Reachable;

	TMap<ATile*, int32> CostMap;
	TArray<ATile*> Queue;

	CostMap.Add(Unit->CurrentTile, 0);
	Queue.Add(Unit->CurrentTile);
	int32 MaxCost = Unit->MovementRange;

	while (Queue.Num() > 0)
	{
		ATile* Current = Queue[0];
		Queue.RemoveAt(0);

		TArray<ATile*> Neighbors = GetAdjacentTiles(Current);

		for (ATile* Neighbor : Neighbors)
		{
			if (!Neighbor) continue;

			// Prevent completely crossing Water tiles
			if (Neighbor->GetElevation() == ETileElevation::Water) continue;

			// Prevent crossing Physical Obstacles (Towers and Borders)
			if (Neighbor->bIsObstacle) continue;

			// Cannot walk through occupied tiles
			if (Neighbor->OccupyingUnit && Neighbor->OccupyingUnit != Unit) continue;

			int32 StepCost = (Neighbor->GetElevation() > Current->GetElevation()) ? 2 : 1;
			int32 TotalCost = CostMap[Current] + StepCost;

			if (TotalCost <= MaxCost)
			{
				if (!CostMap.Contains(Neighbor) || TotalCost < CostMap[Neighbor])
				{
					CostMap.Add(Neighbor, TotalCost);
					Queue.Add(Neighbor);
				}
			}
		}
	}

	CostMap.GenerateKeyArray(Reachable);
	return Reachable;
}

TArray<ATile*> AGameField::GeneratePathRoute(ATile* StartNode, ATile* EndNode)
{
	TArray<ATile*> Path;

	if (!StartNode || !EndNode) return Path;

	// Logical branching based on the SelectedAlgorithm variable (bound to UI ComboBox)
	switch (SelectedAlgorithm)
	{
	case EPathfindingAlgorithm::AStar:
		Path = FindPath(StartNode, EndNode);       // Executes A*
		break;

	case EPathfindingAlgorithm::Greedy:
		Path = FindPathGreedy(StartNode, EndNode); // Executes Greedy
		break;

	default:
		Path = FindPath(StartNode, EndNode);       // Safety fallback to A*
		break;
	}

	return Path;
}

TArray<ATile*> AGameField::FindPath(ATile* StartNode, ATile* EndNode)
{
	TArray<ATile*> Path;
	if (!StartNode || !EndNode) return Path;

	TArray<ATile*> OpenSet;      // Frontier nodes to be evaluated
	TSet<ATile*> ClosedSet;      // Nodes already processed
	TMap<ATile*, ATile*> CameFrom;
	TMap<ATile*, int32> GCost;   // Real movement cost from start to current node
	TMap<ATile*, int32> FCost;   // Total estimated cost (G + H)

	OpenSet.Add(StartNode);
	GCost.Add(StartNode, 0);
	FCost.Add(StartNode, GetManhattanDistance(StartNode->GetGridPosition(), EndNode->GetGridPosition()));

	while (OpenSet.Num() > 0)
	{
		// Node selection: find the node in OpenSet with the lowest total FCost
		ATile* Current = OpenSet[0];
		for (ATile* Node : OpenSet)
		{
			if (FCost.Contains(Node) && FCost[Node] < FCost[Current])
			{
				Current = Node;
			}
		}

		// Destination reached
		if (Current == EndNode)
		{
			ATile* PathNode = EndNode;
			while (PathNode != StartNode)
			{
				Path.Insert(PathNode, 0);
				PathNode = CameFrom[PathNode];
			}
			return Path;
		}

		OpenSet.Remove(Current);
		ClosedSet.Add(Current);

		// Expand search to adjacent tiles
		for (ATile* Neighbor : GetAdjacentTiles(Current))
		{
			// Impediment check: skip null, visited, water, or physical obstacles
			if (!Neighbor || ClosedSet.Contains(Neighbor) || Neighbor->GetElevation() == ETileElevation::Water || Neighbor->bIsObstacle)
				continue;

			// Occupancy rule: units act as dynamic obstacles (unless it's the target tile)
			if (Neighbor->OccupyingUnit && Neighbor != EndNode) continue;

			// Weighted Movement: uphill traversal incurs a higher cost (2) compared to level or downhill (1)
			int32 StepCost = (Neighbor->GetElevation() > Current->GetElevation()) ? 2 : 1;
			int32 TentativeGCost = GCost[Current] + StepCost;

			// Path optimization: update neighbor if a cheaper path is discovered
			if (!GCost.Contains(Neighbor) || TentativeGCost < GCost[Neighbor])
			{
				CameFrom.Add(Neighbor, Current);
				GCost.Add(Neighbor, TentativeGCost);
				int32 Heuristic = GetManhattanDistance(Neighbor->GetGridPosition(), EndNode->GetGridPosition());
				FCost.Add(Neighbor, TentativeGCost + Heuristic);

				if (!OpenSet.Contains(Neighbor))
				{
					OpenSet.Add(Neighbor);
				}
			}
		}
	}
	return Path; // Return empty if no path is found
}

FString AGameField::GetAlphanumericCoordinate(FVector2D GridPosition) const
{
	// Ensure X and Y coordinates remain within the defined grid boundaries (0-24)
	int32 X_Int = FMath::Clamp(FMath::RoundToInt(GridPosition.X), 0, 24);
	int32 Y_Int = FMath::Clamp(FMath::RoundToInt(GridPosition.Y), 0, 24);

	// Convert the X integer into its corresponding ASCII character (65 = 'A', 66 = 'B', etc.)
	char Letter = 65 + X_Int;

	// Format and return the string by concatenating the Character and the Integer
	return FString::Printf(TEXT("%c%d"), Letter, Y_Int);
}

TArray<ATile*> AGameField::FindPathGreedy(ATile* StartNode, ATile* EndNode)
{
	TArray<ATile*> Path;
	if (!StartNode || !EndNode) return Path;

	TArray<ATile*> OpenSet;
	TSet<ATile*> ClosedSet;
	TMap<ATile*, ATile*> CameFrom;

	OpenSet.Add(StartNode);

	while (OpenSet.Num() > 0)
	{
		// Greedy Best-First Search: Select the node with the lowest heuristic value (Manhattan Distance)
		ATile* Current = OpenSet[0];
		int32 LowestH = GetManhattanDistance(Current->GetGridPosition(), EndNode->GetGridPosition());

		for (int32 i = 1; i < OpenSet.Num(); ++i)
		{
			int32 H = GetManhattanDistance(OpenSet[i]->GetGridPosition(), EndNode->GetGridPosition());
			if (H < LowestH)
			{
				Current = OpenSet[i];
				LowestH = H;
			}
		}

		// Destination reached: Backtrack through CameFrom map to reconstruct the traversal path
		if (Current == EndNode)
		{
			ATile* PathNode = EndNode;
			while (PathNode != StartNode)
			{
				Path.Insert(PathNode, 0);
				PathNode = CameFrom[PathNode];
			}
			return Path;
		}

		OpenSet.Remove(Current);
		ClosedSet.Add(Current);

		// Evaluate neighbors and expand the search frontier
		for (ATile* Neighbor : GetAdjacentTiles(Current))
		{
			// Skip invalid, visited, or impassable terrain (Water/Obstacles)
			if (!Neighbor || ClosedSet.Contains(Neighbor) || Neighbor->GetElevation() == ETileElevation::Water || Neighbor->bIsObstacle)
				continue;

			// Verify tile occupancy (only allow the destination tile to be "occupied" for targeting)
			if (Neighbor->OccupyingUnit && Neighbor != EndNode) continue;

			if (!OpenSet.Contains(Neighbor))
			{
				CameFrom.Add(Neighbor, Current);
				OpenSet.Add(Neighbor);
			}
		}
	}
	return Path; // Return an empty array if no valid path exists
}

int32 AGameField::QuantizePerlinNoise(float NoiseValue) const
{
	// 1. Normalize Perlin output from [-1, 1] to [0, 1]
	float N = FMath::Clamp((NoiseValue + 1.0f) * 0.5f, 0.0f, 1.0f);

	N = FMath::Pow(N, 1.05f);

	// Water: bottom band [0, T0]. 
	float T0 = FMath::Lerp(0.15f, 0.45f, WaterThreshold);

	// Grass: starts at T0
	float GrassWidth = FMath::Lerp(0.18f, 0.40f, GrassThreshold);
	float T1 = FMath::Min(T0 + GrassWidth, 0.72f);

	// Mountain1 (yellow)
	float M1Width = FMath::Lerp(0.10f, 0.28f, Mountain1Threshold);
	float T2 = FMath::Min(T1 + M1Width, 0.84f);

	// Mountain2 (orange)
	float M2Width = FMath::Lerp(0.08f, 0.16f, Mountain2Threshold);
	float T3 = FMath::Min(T2 + M2Width, 0.94f);

	if (N < T0) return 0; // Water     (Blue)
	if (N < T1) return 1; // Flat      (Green)
	if (N < T2) return 2; // Mountain  (Yellow)
	if (N < T3) return 3; // Peak      (Orange)
	return 4;             // Summit    (Red)
}