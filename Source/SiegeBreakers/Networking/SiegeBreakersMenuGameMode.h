// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "SiegeBreakersMenuGameMode.generated.h"

/**
 *  Game mode for the main menu map (Content/SiegeBreakers/Maps/MainMenu).
 *  No character is spawned; the player controller just shows the Host/Join menu.
 *  The map picks this game mode in its World Settings (GameMode Override).
 */
UCLASS()
class ASiegeBreakersMenuGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:

	ASiegeBreakersMenuGameMode();
};
