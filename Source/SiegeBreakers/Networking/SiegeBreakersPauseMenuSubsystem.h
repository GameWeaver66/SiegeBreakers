// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "SiegeBreakersPauseMenuSubsystem.generated.h"

class APawn;
class AController;
class APlayerController;
class USiegeBreakersPauseMenuWidget;

/**
 *  Opens and closes the Escape (pause) menu during a match.
 *
 *  Works with any player controller, so no teammate Blueprint needs changing: whenever this PC's player takes control
 *  of a character, we give their player controller one extra input component that listens for Escape.
 *  The main menu map is skipped (it has its own menu).
 *
 *  The game is never actually paused (multiplayer keeps running). While the menu is open, the mouse is shown and
 *  your character ignores move/look input so it doesn't wander off.
 *
 *  In the editor, Escape is the "Stop Play" shortcut and never reaches the game. Rebind it under
 *  Editor Preferences > Keyboard Shortcuts > Stop (Play World) to test this in PIE. Packaged builds are fine.
 */
UCLASS()
class USiegeBreakersPauseMenuSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintCallable, Category="Pause Menu")
	void TogglePauseMenu();

	UFUNCTION(BlueprintCallable, Category="Pause Menu")
	void OpenPauseMenu();

	UFUNCTION(BlueprintCallable, Category="Pause Menu")
	void ClosePauseMenu();

	UFUNCTION(BlueprintPure, Category="Pause Menu")
	bool IsPauseMenuOpen() const;

protected:

	/** The engine calls this whenever any character changes controller; we look for our own local player */
	UFUNCTION()
	void HandlePawnControllerChanged(APawn* Pawn, AController* Controller);

	/** Adds the Escape listener to this player controller */
	void HookPlayerController(APlayerController* PlayerController);

	// Weak pointers: these live in the map's world, and must be free to go away when the map changes

	TWeakObjectPtr<APlayerController> HookedController;
	TWeakObjectPtr<USiegeBreakersPauseMenuWidget> PauseMenu;
};
