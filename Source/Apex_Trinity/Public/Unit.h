// Fill out your copyright notice in the Description page of Project Settings.

/* * BASE CLASS FOR UNITS - POLYMORPHISM APPROACH
 * This class defines the shared interface (virtual functions like MoveToTile, Attack)
 * and common attributes (Health, Damage).
 * Derived classes (e.g., ASniper, ABrawler) will inherit from this and override
 * specific stats or behaviors, ensuring scalable and maintainable OOP design.
 */

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/WidgetComponent.h"
#include "Unit.generated.h"

class ATile;

UCLASS()
class APEX_TRINITY_API AUnit : public AActor
{
	GENERATED_BODY()

public:
	AUnit();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Location")
	ATile* CurrentTile;

	// Identifies the owner of the unit (0 = Player 1, 1 = Player 2)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	int32 TeamID = 0;

	// Base unit stats
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	int32 Health = 10;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stats")
	int32 MaxHealth;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	int32 MovementRange = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	int32 AttackRange = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	int32 MinAttackDamage = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	int32 MaxAttackDamage = 6;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State")
	bool bHasMoved = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State")
	bool bHasAttacked = false;

	int32 GetRandomAttackDamage() const;

	// Function to handle damage intake and unit death logic
	void ReceiveDamage(int32 DamageAmount);

	// Virtual functions: can be overridden by derived classes if necessary
	UFUNCTION(BlueprintCallable, Category = "Actions")
	virtual void MoveToTile(ATile* TargetTile, bool bUseGreedyAlgorithm = false);

	// Visual event to trigger a death animation or sound effect in Blueprint
	UFUNCTION(BlueprintImplementableEvent, Category = "Combat")
	void BP_OnDeath();

	virtual void Tick(float DeltaTime) override;

	// Visual movement speed of the pawn (Adjusted for VInterpConstantTo: Unreal Units per second)
	UPROPERTY(EditAnywhere, Category = "Movement")
	float MovementSpeed = 100.f;

	// Event triggered after spawning to visually differentiate teams
	UFUNCTION(BlueprintImplementableEvent, Category = "Visuals")
	void BP_ApplyTeamColor();

	// Logical death handling and cleanup
	void Die();

	// Function called by the Referee when a new turn starts
	void ResetActions();

	void FaceTargetLocation(FVector LookAtTarget);

	// Cache the tile where the unit was originally deployed at match start for respawn logic
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Location")
	ATile* InitialSpawnTile;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UI")
	UWidgetComponent* HealthWidgetComp;
    
	//Blueprint event triggered to update HUD
	UFUNCTION(BlueprintImplementableEvent, Category = "UI")
	void BP_UpdateHealthBar(float HealthPercent);

protected:
	// Visual components
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USceneComponent* SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* MeshComponent;

	virtual void BeginPlay() override;

private:
	// Tracks if the unit is currently executing the 3D movement animation
	bool bIsMoving = false;

	// Tracks the current step of the L-shaped jump (1 = Ascent, 2 = Translation, 3 = Descent)
	int32 CurrentMovePhase = 0;

	// 3D waypoint system for orthogonal movement
	// Array of exact 3D coordinates the pawn must follow to hug the terrain
	TArray<FVector> Waypoints;
	// Current target point in the array
	int32 CurrentWaypointIndex = 0;
	// Final landing point to guarantee perfect alignment
	FVector FinalTarget;
};