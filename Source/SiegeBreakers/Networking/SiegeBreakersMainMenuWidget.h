// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SiegeBreakersMainMenuWidget.generated.h"

class UButton;
class UEditableTextBox;
class UTextBlock;
class USiegeBreakersNetworkSubsystem;

/**
 *  The Host/Join main menu, built entirely in C++ (no widget asset to open in the editor).
 *
 *  Layout, top to bottom:
 *    "Siege Breakers" title
 *    [Host Game] button
 *    join code box (6 digits) + [Join Game] button
 *    status line ("Signing in...", errors, ...)
 *    [Quit] button
 *
 *  The buttons just call USiegeBreakersNetworkSubsystem, which does the real online work.
 *  The menu listens to the subsystem's "complete" events to update the status line and the buttons.
 */
UCLASS()
class USiegeBreakersMainMenuWidget : public UUserWidget
{
	GENERATED_BODY()

protected:

	/** Called when the widget is about to be shown: we build the layout here because there is no widget asset */
	virtual TSharedRef<SWidget> RebuildWidget() override;

	/** Called once the widget is on screen: hook up to the network subsystem and start logging in */
	virtual void NativeConstruct() override;

	/** Called when the widget is removed (for example when the level changes): unhook from the subsystem */
	virtual void NativeDestruct() override;

	/** Creates every button and text, and wires the button clicks */
	void BuildLayout();

	/** Turns the buttons on or off depending on whether we're logged in, busy, or have a full join code */
	void RefreshButtons();

	/** Shows a line of text under the buttons. Errors are red. */
	void SetStatus(const FString& Message, bool bIsError = false);

	USiegeBreakersNetworkSubsystem* GetNetworkSubsystem() const;

	// Button and text box handlers. They must be UFUNCTIONs because UMG events are "dynamic" delegates.

	UFUNCTION()
	void HandleHostClicked();

	UFUNCTION()
	void HandleJoinClicked();

	UFUNCTION()
	void HandleQuitClicked();

	/** Keeps the join code box to digits only, at most 6 */
	UFUNCTION()
	void HandleJoinCodeChanged(const FText& Text);

	/** Pressing Enter in the join code box acts like clicking Join */
	UFUNCTION()
	void HandleJoinCodeCommitted(const FText& Text, ETextCommit::Type CommitMethod);

	// Network subsystem event handlers

	UFUNCTION()
	void HandleLoginComplete(bool bWasSuccessful);

	UFUNCTION()
	void HandleHostComplete(bool bWasSuccessful, const FString& JoinCode);

	UFUNCTION()
	void HandleJoinComplete(bool bWasSuccessful);

	// The pieces of the menu we need to change later. UPROPERTY keeps them from being garbage collected.

	UPROPERTY()
	TObjectPtr<UButton> HostButton;

	UPROPERTY()
	TObjectPtr<UButton> JoinButton;

	UPROPERTY()
	TObjectPtr<UButton> QuitButton;

	UPROPERTY()
	TObjectPtr<UEditableTextBox> JoinCodeBox;

	UPROPERTY()
	TObjectPtr<UTextBlock> StatusText;

	/** True between clicking Host/Join and hearing back, so buttons can't be spammed */
	bool bWaitingForResult = false;
};
