#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "TMainHUD.h"
#include "Blueprint/UserWidget.h"
#include "TPlayerController.generated.h"

// Forward declarations per l'Enhanced Input
class UInputMappingContext;
class UInputAction;

UENUM(BlueprintType)
enum class EActionState : uint8
{
	Idle,
	PreparingMove,
	PreparingAttack
};

UCLASS()
class APEX_TRINITY_API ATPlayerController : public APlayerController // T=Trinity
{
	GENERATED_BODY()

public:
	ATPlayerController();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	UInputMappingContext* DefaultMappingContext;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	UInputAction* ClickAction;

	// Pointer to the player-selected unit
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Selection")
	class AUnit* SelectedUnit;

	UPROPERTY(BlueprintReadWrite, Category = "Turn")
	bool bIsPlayer0Turn;

	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UTMainHUD> MainHUDClass;

	UPROPERTY(BlueprintReadOnly, Category = "UI")
	UTMainHUD* MainHUDWidget;

	UFUNCTION(BlueprintCallable, Category = "Action")
	void PrepareMove();

	UFUNCTION(BlueprintCallable, Category = "Action")
	void PrepareAttack();

	EActionState CurrentActionState = EActionState::Idle;

	// Resets the selection and state machine (Call this when the turn ends!)
	void ClearSelectionState();

protected:
	virtual void BeginPlay() override;

	virtual void SetupInputComponent() override;

	// Function called on every left mouse click
	void OnLeftMouseClick();
};