// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SiegeBreakersPauseMenuWidget.generated.h"

class UButton;

/**
 *  The in-game Escape menu, built entirely in C++ like the main menu (no widget asset).
 *
 *  Layout, top to bottom:
 *    "Paused" title
 *    [Resume] button
 *    [Quit to Main Menu] button
 *    note for the host: quitting ends the game for everyone
 *
 *  It does NOT pause the game: this is multiplayer, so the match keeps running behind it.
 *  USiegeBreakersPauseMenuSubsystem opens and closes it.
 */
UCLASS()
class USiegeBreakersPauseMenuWidget : public UUserWidget
{
	GENERATED_BODY()

protected:

	/** Builds the layout the first time the widget is shown, because there is no widget asset */
	virtual TSharedRef<SWidget> RebuildWidget() override;

	/** Escape while the menu has keyboard focus closes it again */
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	void BuildLayout();

	UFUNCTION()
	void HandleResumeClicked();

	UFUNCTION()
	void HandleQuitClicked();

	UPROPERTY()
	TObjectPtr<UButton> ResumeButton;

	UPROPERTY()
	TObjectPtr<UButton> QuitButton;
};
