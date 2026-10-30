// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "SiegeBreakersMenuPlayerController.generated.h"

class USiegeBreakersMainMenuWidget;

/**
 *  Player controller used only on the main menu map.
 *  Puts the Host/Join menu on screen, shows the mouse, and sends all input to the menu.
 */
UCLASS()
class ASiegeBreakersMenuPlayerController : public APlayerController
{
	GENERATED_BODY()

protected:

	virtual void BeginPlay() override;

	/** Runs when the menu map closes (for example after Host or Join): hands input back to the game */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** The menu we put on screen */
	UPROPERTY()
	TObjectPtr<USiegeBreakersMainMenuWidget> MenuWidget;
};
