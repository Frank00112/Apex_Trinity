// Fill out your copyright notice in the Description page of Project Settings.

#include "Tower.h"
#include "Tile.h"
#include "Unit.h"
#include "Kismet/GameplayStatics.h"

ATower::ATower()
{
	PrimaryActorTick.bCanEverTick = false;

	Scene = CreateDefaultSubobject<USceneComponent>(TEXT("Scene"));
	SetRootComponent(Scene);

	StaticMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StaticMeshComponent"));
	StaticMeshComponent->SetupAttachment(RootComponent);

	CurrentState = ETowerState::Neutral;
	ControllingTeam = -1;
}

void ATower::EvaluateControlState()
{
	if (!OwningTile) return;

	TArray<AActor*> AllUnits;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AUnit::StaticClass(), AllUnits);

	FVector2D TowerPos = OwningTile->GetGridPosition();

	int32 UnitsP0 = 0;
	int32 UnitsP1 = 0;

	for (AActor* Actor : AllUnits)
	{
		AUnit* Unit = Cast<AUnit>(Actor);
		if (IsValid(Unit) && Unit->CurrentTile)
		{
			FVector2D UnitPos = Unit->CurrentTile->GetGridPosition();
			int32 DistX = FMath::Abs(TowerPos.X - UnitPos.X);
			int32 DistY = FMath::Abs(TowerPos.Y - UnitPos.Y);

			if (FMath::Max(DistX, DistY) <= 2)
			{
				if (Unit->TeamID == 0) UnitsP0++;
				if (Unit->TeamID == 1) UnitsP1++;
			}
		}
	}

	ETowerState OldState = CurrentState;
	int32 OldTeam = ControllingTeam;

	if (UnitsP0 == 0 && UnitsP1 == 0)
	{
		if (CurrentState == ETowerState::Contested)
		{
			CurrentState = ETowerState::Neutral;
			ControllingTeam = -1;
		}
	}
	else if (UnitsP0 > 0 && UnitsP1 > 0)
	{
		CurrentState = ETowerState::Contested;
		ControllingTeam = -1;
	}
	else if (UnitsP0 > 0 && UnitsP1 == 0)
	{
		CurrentState = ETowerState::Captured;
		ControllingTeam = 0;
	}
	else if (UnitsP1 > 0 && UnitsP0 == 0)
	{
		CurrentState = ETowerState::Captured;
		ControllingTeam = 1;
	}

	if (OldState != CurrentState || OldTeam != ControllingTeam)
	{
		GetWorldTimerManager().ClearTimer(ContestedBlinkTimer);

		if (CurrentState == ETowerState::Neutral)
		{
			BP_UpdateTowerColor(-1);
		}
		else if (CurrentState == ETowerState::Captured)
		{
			BP_UpdateTowerColor(ControllingTeam);
		}
		else if (CurrentState == ETowerState::Contested)
		{
			bBlinkToggle = false;

			// Enforce the initial color (0 = Player) upon contest start
			BP_UpdateTowerColor(0);

			// Initialize the metronome timer to trigger every 1.0 seconds
			GetWorldTimerManager().SetTimer(ContestedBlinkTimer, this, &ATower::ToggleContestedColor, 1.0f, true);
		}
	}
}

void ATower::ToggleContestedColor()
{
	bBlinkToggle = !bBlinkToggle;
	BP_UpdateTowerColor(bBlinkToggle ? 0 : 1);
}