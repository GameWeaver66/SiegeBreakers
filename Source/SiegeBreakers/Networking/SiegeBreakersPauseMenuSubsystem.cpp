// Copyright Epic Games, Inc. All Rights Reserved.

#include "Networking/SiegeBreakersPauseMenuSubsystem.h"
#include "Networking/SiegeBreakersMenuPlayerController.h"
#include "Networking/SiegeBreakersPauseMenuWidget.h"
#include "Components/InputComponent.h"
#include "Engine/GameInstance.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"

void USiegeBreakersPauseMenuSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// Fires on the host and on clients whenever a character gets a (new) controller, including our own spawn
	GetGameInstance()->GetOnPawnControllerChanged().AddDynamic(this, &USiegeBreakersPauseMenuSubsystem::HandlePawnControllerChanged);
}

void USiegeBreakersPauseMenuSubsystem::Deinitialize()
{
	GetGameInstance()->GetOnPawnControllerChanged().RemoveDynamic(this, &USiegeBreakersPauseMenuSubsystem::HandlePawnControllerChanged);

	Super::Deinitialize();
}

void USiegeBreakersPauseMenuSubsystem::HandlePawnControllerChanged(APawn* Pawn, AController* Controller)
{
	APlayerController* PlayerController = Cast<APlayerController>(Controller);

	// Only the player sitting at this PC (the host also has controllers for every remote player), not on the main menu,
	// and only once per controller
	if (!PlayerController || !PlayerController->IsLocalController() || PlayerController->IsA<ASiegeBreakersMenuPlayerController>()
		|| HookedController.Get() == PlayerController)
	{
		return;
	}

	HookPlayerController(PlayerController);
}

void USiegeBreakersPauseMenuSubsystem::HookPlayerController(APlayerController* PlayerController)
{
	// A small input component of our own, added on top of the controller's input stack.
	// It's registered as a component of the controller, so it goes away with the controller when the map changes.
	UInputComponent* PauseInput = NewObject<UInputComponent>(PlayerController, TEXT("SiegePauseMenuInput"));
	PauseInput->bBlockInput = false;
	PauseInput->BindKey(EKeys::Escape, IE_Pressed, this, &USiegeBreakersPauseMenuSubsystem::TogglePauseMenu).bExecuteWhenPaused = true;

	PlayerController->AddInstanceComponent(PauseInput);
	PauseInput->RegisterComponent();
	PlayerController->PushInputComponent(PauseInput);

	HookedController = PlayerController;
}

bool USiegeBreakersPauseMenuSubsystem::IsPauseMenuOpen() const
{
	return PauseMenu.IsValid() && PauseMenu->IsInViewport();
}

void USiegeBreakersPauseMenuSubsystem::TogglePauseMenu()
{
	if (IsPauseMenuOpen())
	{
		ClosePauseMenu();
	}
	else
	{
		OpenPauseMenu();
	}
}

void USiegeBreakersPauseMenuSubsystem::OpenPauseMenu()
{
	APlayerController* PlayerController = HookedController.Get();
	if (!PlayerController || IsPauseMenuOpen())
	{
		return;
	}

	USiegeBreakersPauseMenuWidget* Menu = CreateWidget<USiegeBreakersPauseMenuWidget>(PlayerController, USiegeBreakersPauseMenuWidget::StaticClass());
	if (!Menu)
	{
		return;
	}

	PauseMenu = Menu;
	Menu->SetIsFocusable(true);
	Menu->AddToViewport(100); // above the HUD

	// Mouse on for clicking buttons, but the game keeps running and still gets input the menu doesn't use
	PlayerController->bShowMouseCursor = true;
	FInputModeGameAndUI InputMode;
	InputMode.SetWidgetToFocus(Menu->TakeWidget());
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	InputMode.SetHideCursorDuringCapture(false);
	PlayerController->SetInputMode(InputMode);

	// Stop our character walking and the camera spinning while we use the menu
	PlayerController->SetIgnoreMoveInput(true);
	PlayerController->SetIgnoreLookInput(true);
}

void USiegeBreakersPauseMenuSubsystem::ClosePauseMenu()
{
	if (!IsPauseMenuOpen())
	{
		return;
	}

	PauseMenu->RemoveFromParent();
	PauseMenu.Reset();

	if (APlayerController* PlayerController = HookedController.Get())
	{
		PlayerController->bShowMouseCursor = false;
		PlayerController->SetInputMode(FInputModeGameOnly());
		PlayerController->SetIgnoreMoveInput(false);
		PlayerController->SetIgnoreLookInput(false);
	}
}
