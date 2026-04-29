// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Tile.generated.h"

// Enum for elevation levels (0 = Water, 1 = Flat, 2-4 = Mountain)
UENUM(BlueprintType)
enum class ETileElevation : uint8
{
	Water = 0      UMETA(DisplayName = "Water (Level 0)"),
	Flat = 1       UMETA(DisplayName = "Flat (Level 1)"),
	Mountain1 = 2  UMETA(DisplayName = "Mountain (Level 2)"),
	Mountain2 = 3  UMETA(DisplayName = "High Mountain (Level 3)"),
	Mountain3 = 4  UMETA(DisplayName = "Peak (Level 4)")
};

class AUnit;

UCLASS()
class APEX_TRINITY_API ATile : public AActor // KEEP THIS LINE EXACTLY AS UNREAL GENERATED IT
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	ATile();

	// Defines if the cell is an obstacle (e.g. contains a tower)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tile Data")
	bool bIsObstacle;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tile Data")
	AUnit* OccupyingUnit;

	// Map to hold the visual materials for each elevation level
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tile Materials")
	TMap<ETileElevation, UMaterialInterface*> ElevationMaterials;

	// Updates the mesh material based on current elevation
	UFUNCTION()
	void UpdateMaterial();

	// Trigger for the unit's movement range visualization
	UFUNCTION(BlueprintImplementableEvent, Category = "Highlight")
	void BP_SetMovementHighlight(bool bIsOn);

	// Trigger for the unit's attack range visualization
	UFUNCTION(BlueprintImplementableEvent, Category = "Highlight")
	void BP_SetAttackHighlight(bool bIsOn);

	// Trigger for the highlight effect beneath the selected unit
	UFUNCTION(BlueprintImplementableEvent, Category = "Highlight")
	void BP_SetSelectionHighlight(bool bIsOn, int32 TeamID);

	// Set grid position (X, Y)
	void SetGridPosition(const int32 InX, const int32 InY);

	// Get grid position
	FVector2D GetGridPosition() const;

	// Set tile elevation level
	void SetElevation(const ETileElevation InElevation);

	// Get tile elevation level
	ETileElevation GetElevation() const;

	// Event to apply a white material to border tiles
	UFUNCTION(BlueprintImplementableEvent, Category = "Highlight")
	void BP_ApplyBorderMaterial();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USceneComponent* Scene;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* StaticMeshComponent;

	// (X, Y) position of the tile
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tile Data")
	FVector2D TileGridPosition;

	// Elevation level of the tile
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tile Data")
	ETileElevation Elevation;
};