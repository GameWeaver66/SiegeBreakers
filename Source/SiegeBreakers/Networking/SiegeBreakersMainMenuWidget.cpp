// Copyright Epic Games, Inc. All Rights Reserved.

#include "Networking/SiegeBreakersMainMenuWidget.h"
#include "Networking/SiegeBreakersNetworkSubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Kismet/KismetSystemLibrary.h"

namespace SiegeMenu
{
	constexpr int32 JoinCodeLength = 6;

	/** Makes a text label with the given font size */
	UTextBlock* MakeText(UWidgetTree* Tree, const FString& Text, int32 FontSize)
	{
		UTextBlock* Label = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		Label->SetText(FText::FromString(Text));
		Label->SetJustification(ETextJustify::Center);

		FSlateFontInfo Font = Label->GetFont();
		Font.Size = FontSize;
		Label->SetFont(Font);

		return Label;
	}

	/** Makes a button with a text label inside it */
	UButton* MakeButton(UWidgetTree* Tree, const FString& Text)
	{
		UButton* Button = Tree->ConstructWidget<UButton>(UButton::StaticClass());
		Button->SetContent(MakeText(Tree, Text, 20));

		// The default button is light grey; tinting it darker makes the white text easy to read
		Button->SetBackgroundColor(FLinearColor(0.15f, 0.2f, 0.3f, 1.0f));
		return Button;
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// Building the menu

TSharedRef<SWidget> USiegeBreakersMainMenuWidget::RebuildWidget()
{
	// A widget made from an asset gets its WidgetTree (the list of child widgets) from the asset.
	// We have no asset, so we make the tree ourselves the first time the widget is shown.
	if (!WidgetTree)
	{
		WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree"), RF_Transient);
	}

	if (!WidgetTree->RootWidget)
	{
		BuildLayout();
	}

	return Super::RebuildWidget();
}

void USiegeBreakersMainMenuWidget::BuildLayout()
{
	// Dark background filling the whole screen, with everything else centered on it
	UBorder* Background = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Background"));
	Background->SetBrushColor(FLinearColor(0.02f, 0.02f, 0.03f, 1.0f));
	Background->SetHorizontalAlignment(HAlign_Center);
	Background->SetVerticalAlignment(VAlign_Center);
	WidgetTree->RootWidget = Background;

	// One column; every row below is stacked in it from top to bottom
	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Column"));
	Background->SetContent(Column);

	auto AddRow = [Column](UWidget* Row, float SpaceAbove)
	{
		UVerticalBoxSlot* Slot = Column->AddChildToVerticalBox(Row);
		Slot->SetPadding(FMargin(0.0f, SpaceAbove, 0.0f, 0.0f));
		Slot->SetHorizontalAlignment(HAlign_Fill);
	};

	// Title
	AddRow(SiegeMenu::MakeText(WidgetTree, TEXT("Siege Breakers"), 56), 0.0f);

	// Host
	HostButton = SiegeMenu::MakeButton(WidgetTree, TEXT("Host Game"));
	HostButton->OnClicked.AddDynamic(this, &USiegeBreakersMainMenuWidget::HandleHostClicked);
	AddRow(HostButton, 40.0f);

	// Join: code box and button side by side
	UHorizontalBox* JoinRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("JoinRow"));

	JoinCodeBox = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(), TEXT("JoinCodeBox"));
	JoinCodeBox->SetHintText(FText::FromString(TEXT("6-digit code")));
	JoinCodeBox->SetJustification(ETextJustify::Center);

	// The default style draws typed text in faint grey on a light box; make the digits black and bigger
	JoinCodeBox->WidgetStyle.SetForegroundColor(FSlateColor(FLinearColor::Black));
	JoinCodeBox->WidgetStyle.SetFocusedForegroundColor(FSlateColor(FLinearColor::Black));
	JoinCodeBox->WidgetStyle.TextStyle.Font.Size = 18;
	JoinCodeBox->OnTextChanged.AddDynamic(this, &USiegeBreakersMainMenuWidget::HandleJoinCodeChanged);
	JoinCodeBox->OnTextCommitted.AddDynamic(this, &USiegeBreakersMainMenuWidget::HandleJoinCodeCommitted);

	// The size box just gives the code box a sensible width
	USizeBox* CodeBoxSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("JoinCodeSize"));
	CodeBoxSize->SetWidthOverride(200.0f);
	CodeBoxSize->SetContent(JoinCodeBox);

	UHorizontalBoxSlot* CodeSlot = JoinRow->AddChildToHorizontalBox(CodeBoxSize);
	CodeSlot->SetVerticalAlignment(VAlign_Center);

	JoinButton = SiegeMenu::MakeButton(WidgetTree, TEXT("Join Game"));
	JoinButton->OnClicked.AddDynamic(this, &USiegeBreakersMainMenuWidget::HandleJoinClicked);

	UHorizontalBoxSlot* JoinSlot = JoinRow->AddChildToHorizontalBox(JoinButton);
	JoinSlot->SetPadding(FMargin(12.0f, 0.0f, 0.0f, 0.0f));
	JoinSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

	AddRow(JoinRow, 16.0f);

	// Status line
	StatusText = SiegeMenu::MakeText(WidgetTree, FString(), 16);
	StatusText->SetAutoWrapText(true);
	AddRow(StatusText, 24.0f);

	// Quit
	QuitButton = SiegeMenu::MakeButton(WidgetTree, TEXT("Quit"));
	QuitButton->OnClicked.AddDynamic(this, &USiegeBreakersMainMenuWidget::HandleQuitClicked);
	AddRow(QuitButton, 40.0f);
}

// ---------------------------------------------------------------------------------------------------------------------
// Showing and hiding

void USiegeBreakersMainMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();

	USiegeBreakersNetworkSubsystem* Network = GetNetworkSubsystem();
	if (!Network)
	{
		SetStatus(TEXT("Online features are missing (no network subsystem)"), true);
		RefreshButtons();
		return;
	}

	Network->OnLoginComplete.AddUniqueDynamic(this, &USiegeBreakersMainMenuWidget::HandleLoginComplete);
	Network->OnHostComplete.AddUniqueDynamic(this, &USiegeBreakersMainMenuWidget::HandleHostComplete);
	Network->OnJoinComplete.AddUniqueDynamic(this, &USiegeBreakersMainMenuWidget::HandleJoinComplete);

	if (Network->IsLoggedIn())
	{
		SetStatus(TEXT("Signed in. Host a game or enter a join code."));
	}
	else
	{
		// The game usually starts logging in by itself at launch. Calling Login() again is safe:
		// it does nothing if a login is already running, and retries if an earlier one failed.
		SetStatus(TEXT("Signing in..."));
		Network->Login();
	}

	RefreshButtons();
}

void USiegeBreakersMainMenuWidget::NativeDestruct()
{
	// The subsystem lives longer than this menu, so stop it from calling us after we're gone
	if (USiegeBreakersNetworkSubsystem* Network = GetNetworkSubsystem())
	{
		Network->OnLoginComplete.RemoveAll(this);
		Network->OnHostComplete.RemoveAll(this);
		Network->OnJoinComplete.RemoveAll(this);
	}

	Super::NativeDestruct();
}

// ---------------------------------------------------------------------------------------------------------------------
// Buttons

void USiegeBreakersMainMenuWidget::HandleHostClicked()
{
	if (USiegeBreakersNetworkSubsystem* Network = GetNetworkSubsystem())
	{
		bWaitingForResult = true;
		SetStatus(TEXT("Creating a game..."));
		RefreshButtons();

		// On success this opens the game map, which replaces the menu
		Network->HostGame();
	}
}

void USiegeBreakersMainMenuWidget::HandleJoinClicked()
{
	const FString Code = JoinCodeBox->GetText().ToString().TrimStartAndEnd();

	if (Code.Len() != SiegeMenu::JoinCodeLength || !Code.IsNumeric())
	{
		SetStatus(TEXT("Join codes are 6 digits"), true);
		return;
	}

	if (USiegeBreakersNetworkSubsystem* Network = GetNetworkSubsystem())
	{
		bWaitingForResult = true;
		SetStatus(FString::Printf(TEXT("Looking for game %s..."), *Code));
		RefreshButtons();

		Network->JoinGame(Code);
	}
}

void USiegeBreakersMainMenuWidget::HandleQuitClicked()
{
	UKismetSystemLibrary::QuitGame(this, GetOwningPlayer(), EQuitPreference::Quit, false);
}

void USiegeBreakersMainMenuWidget::HandleJoinCodeChanged(const FText& Text)
{
	// Throw away anything that isn't a digit, and anything past 6 characters
	const FString Typed = Text.ToString();
	FString Digits;
	for (TCHAR Character : Typed)
	{
		if (FChar::IsDigit(Character) && Digits.Len() < SiegeMenu::JoinCodeLength)
		{
			Digits.AppendChar(Character);
		}
	}

	// Only write back when something was removed, otherwise we'd loop forever (SetText fires this event again)
	if (Digits != Typed)
	{
		JoinCodeBox->SetText(FText::FromString(Digits));
	}

	RefreshButtons();
}

void USiegeBreakersMainMenuWidget::HandleJoinCodeCommitted(const FText& Text, ETextCommit::Type CommitMethod)
{
	if (CommitMethod == ETextCommit::OnEnter && JoinButton->GetIsEnabled())
	{
		HandleJoinClicked();
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// Results from the network subsystem

void USiegeBreakersMainMenuWidget::HandleLoginComplete(bool bWasSuccessful)
{
	if (bWasSuccessful)
	{
		SetStatus(TEXT("Signed in. Host a game or enter a join code."));
	}
	else
	{
		SetStatus(TEXT("Couldn't sign in to the online service. Check your internet connection and restart the game."), true);
	}

	RefreshButtons();
}

void USiegeBreakersMainMenuWidget::HandleHostComplete(bool bWasSuccessful, const FString& JoinCode)
{
	if (bWasSuccessful)
	{
		// The game map is loading now; keep the buttons off until the menu disappears
		SetStatus(FString::Printf(TEXT("Hosting! Code: %s"), *JoinCode));
	}
	else
	{
		bWaitingForResult = false;
		SetStatus(TEXT("Couldn't create a game. Please try again."), true);
	}

	RefreshButtons();
}

void USiegeBreakersMainMenuWidget::HandleJoinComplete(bool bWasSuccessful)
{
	if (bWasSuccessful)
	{
		// Found the game and started connecting. If the connection fails, Unreal sends us back to this menu map.
		SetStatus(TEXT("Found the game, connecting to the host..."));
	}
	else
	{
		bWaitingForResult = false;
		SetStatus(TEXT("Couldn't join. Check the code and try again."), true);
	}

	RefreshButtons();
}

// ---------------------------------------------------------------------------------------------------------------------
// Helpers

void USiegeBreakersMainMenuWidget::RefreshButtons()
{
	USiegeBreakersNetworkSubsystem* Network = GetNetworkSubsystem();
	const bool bCanStart = Network && Network->IsLoggedIn() && !bWaitingForResult;
	const bool bCodeComplete = JoinCodeBox && JoinCodeBox->GetText().ToString().Len() == SiegeMenu::JoinCodeLength;

	if (HostButton)
	{
		HostButton->SetIsEnabled(bCanStart);
	}

	if (JoinButton)
	{
		JoinButton->SetIsEnabled(bCanStart && bCodeComplete);
	}
}

void USiegeBreakersMainMenuWidget::SetStatus(const FString& Message, bool bIsError)
{
	if (StatusText)
	{
		StatusText->SetText(FText::FromString(Message));
		StatusText->SetColorAndOpacity(FSlateColor(bIsError ? FLinearColor(1.0f, 0.35f, 0.3f) : FLinearColor::White));
	}
}

USiegeBreakersNetworkSubsystem* USiegeBreakersMainMenuWidget::GetNetworkSubsystem() const
{
	UWorld* World = GetWorld();
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<USiegeBreakersNetworkSubsystem>() : nullptr;
}
