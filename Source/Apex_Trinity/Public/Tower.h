// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Tower.generated.h"

class ATile;

// Represents the possible control states of a Tower actor
UENUM(BlueprintType)
enum class ETowerState : uint8
{
	Neutral,
	Captured,
	Contested
};

UCLASS()
class APEX_TRINITY_API ATower : public AActor
{
	GENERATED_BODY()

public:
	ATower();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Location")
	ATile* OwningTile;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State")
	ETowerState CurrentState;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State")
	int32 ControllingTeam;

	void EvaluateControlState();

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USceneComponent* Scene;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* StaticMeshComponent;

	// Updates the tower's visual representation 
	UFUNCTION(BlueprintImplementableEvent, Category = "Visuals")
	void BP_UpdateTowerColor(int32 TeamColorID);

private:
	FTimerHandle ContestedBlinkTimer;
	bool bBlinkToggle;

	// UFUNCTION macro ensures the timer delegate can locate the function, preventing crashes or failed calls
	UFUNCTION()
	void ToggleContestedColor();
};