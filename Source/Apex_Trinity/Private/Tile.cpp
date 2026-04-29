// Fill out your copyright notice in the Description page of Project Settings.


#include "Tile.h"

// Sets default values
ATile::ATile()
{
	// Turn off Tick to improve performance (static tiles do not need it) [cite: 483]
	PrimaryActorTick.bCanEverTick = false;

	// Create and set root component [cite: 467]
	Scene = CreateDefaultSubobject<USceneComponent>(TEXT("Scene"));
	SetRootComponent(Scene);

	// Create and attach static mesh component
	StaticMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StaticMeshComponent"));
	StaticMeshComponent->SetupAttachment(Scene);

	// Default initialization
	TileGridPosition = FVector2D(-1, -1);
	Elevation = ETileElevation::Water;
	bIsObstacle = false;
}

// Called when the game starts or when spawned
void ATile::BeginPlay()
{
	Super::BeginPlay(); // Mandatory call to base class [cite: 463]
}

void ATile::SetGridPosition(const int32 InX, const int32 InY)
{
	TileGridPosition.Set(InX, InY);
}

FVector2D ATile::GetGridPosition() const
{
	return TileGridPosition;
}

void ATile::SetElevation(const ETileElevation InElevation)
{
	Elevation = InElevation;
	UpdateMaterial(); // Automatically update visual when elevation is set
}

void ATile::UpdateMaterial()
{
	// Check if the map contains a material for our current elevation
	if (ElevationMaterials.Contains(Elevation))
	{
		// Safely get the material pointer
		if (UMaterialInterface* Mat = ElevationMaterials[Elevation])
		{
			// Apply it to the first material slot (index 0) of the mesh
			StaticMeshComponent->SetMaterial(0, Mat);
		}
	}
}

ETileElevation ATile::GetElevation() const
{
	return Elevation;
}