// Copyright Epic Games, Inc. All Rights Reserved.

#include "Networking/SiegeBreakersMenuPlayerController.h"
#include "Networking/SiegeBreakersMainMenuWidget.h"

void ASiegeBreakersMenuPlayerController::BeginPlay()
{
	Super::BeginPlay();

	// Only the player sitting at this PC gets a menu (a server keeps controllers for remote players too)
	if (!IsLocalPlayerController())
	{
		return;
	}

	MenuWidget = CreateWidget<USiegeBreakersMainMenuWidget>(this, USiegeBreakersMainMenuWidget::StaticClass());
	if (!MenuWidget)
	{
		return;
	}

	// Let the menu take keyboard focus, so "UI only" input mode has somewhere to send key presses
	MenuWidget->SetIsFocusable(true);
	MenuWidget->AddToViewport();

	// Mouse on, and every click and key press goes to the menu instead of the (non-existent) character
	bShowMouseCursor = true;

	FInputModeUIOnly InputMode;
	InputMode.SetWidgetToFocus(MenuWidget->TakeWidget());
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(InputMode);
}

void ASiegeBreakersMenuPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// "UI only" input is remembered by the game window even after the map changes,
	// so switch back to normal game input before the match map loads
	if (IsLocalPlayerController())
	{
		bShowMouseCursor = false;
		SetInputMode(FInputModeGameOnly());
	}

	Super::EndPlay(EndPlayReason);
}
