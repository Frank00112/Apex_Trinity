// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TMainHUD.generated.h"

UCLASS()
class APEX_TRINITY_API UTMainHUD : public UUserWidget
{
	GENERATED_BODY()

public:
	// Update the selection panel with unit data; pass 'false' to bHasSelection to hide the panel when clicking empty space
    UFUNCTION(BlueprintImplementableEvent, Category = "HUD")
    void BP_UpdateSelectionInfo(bool bHasSelection, const FString& UnitName, int32 Health, int32 MaxHealth, int32 MinAttackDamage, int32 MaxAttackDamage, int32 AttackRange);

    // Update global game state
    // Pass current turn string and tower counts
    UFUNCTION(BlueprintImplementableEvent, Category = "HUD")
    void BP_UpdateGlobalStatus(const FString& CurrentTurn, int32 P0Towers, int32 P1Towers);

    // Visual move history
    UFUNCTION(BlueprintImplementableEvent, Category = "HUD")
    void BP_AddLogEntry(const FString& NewLog);

    UFUNCTION(BlueprintImplementableEvent, Category = "HUD")
    void BP_ShowGlobalState();

    UFUNCTION(BlueprintImplementableEvent, Category = "HUD")
    void BP_ShowSystemMessage(const FString& Message);
};
