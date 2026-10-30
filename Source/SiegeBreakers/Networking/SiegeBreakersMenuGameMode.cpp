// Copyright Epic Games, Inc. All Rights Reserved.

#include "Networking/SiegeBreakersMenuGameMode.h"
#include "Networking/SiegeBreakersMenuPlayerController.h"

ASiegeBreakersMenuGameMode::ASiegeBreakersMenuGameMode()
{
	// No pawn: on the menu there's nothing to walk around with
	DefaultPawnClass = nullptr;

	PlayerControllerClass = ASiegeBreakersMenuPlayerController::StaticClass();
}
