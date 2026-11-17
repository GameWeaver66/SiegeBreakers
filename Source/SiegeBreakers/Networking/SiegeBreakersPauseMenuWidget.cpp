// Copyright Epic Games, Inc. All Rights Reserved.

#include "Networking/SiegeBreakersPauseMenuWidget.h"
#include "Networking/SiegeBreakersNetworkSubsystem.h"
#include "Networking/SiegeBreakersPauseMenuSubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "InputCoreTypes.h"

namespace SiegePauseMenu
{
	/** Same look as the main menu's text */
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

	/** Same look as the main menu's buttons */
	UButton* MakeButton(UWidgetTree* Tree, const FString& Text)
	{
		UButton* Button = Tree->ConstructWidget<UButton>(UButton::StaticClass());
		Button->SetContent(MakeText(Tree, Text, 20));
		Button->SetBackgroundColor(FLinearColor(0.15f, 0.2f, 0.3f, 1.0f));
		return Button;
	}
}

TSharedRef<SWidget> USiegeBreakersPauseMenuWidget::RebuildWidget()
{
	// No widget asset, so we make the widget tree ourselves (same trick as the main menu)
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

void USiegeBreakersPauseMenuWidget::BuildLayout()
{
	// See-through dark layer over the whole screen: the game stays visible (and running) behind it.
	// It also catches mouse clicks, so clicking next to the buttons doesn't fire your weapon.
	UBorder* Background = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Background"));
	Background->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.6f));
	Background->SetHorizontalAlignment(HAlign_Center);
	Background->SetVerticalAlignment(VAlign_Center);
	WidgetTree->RootWidget = Background;

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Column"));
	Background->SetContent(Column);

	auto AddRow = [Column](UWidget* Row, float SpaceAbove)
	{
		UVerticalBoxSlot* Slot = Column->AddChildToVerticalBox(Row);
		Slot->SetPadding(FMargin(0.0f, SpaceAbove, 0.0f, 0.0f));
		Slot->SetHorizontalAlignment(HAlign_Fill);
	};

	AddRow(SiegePauseMenu::MakeText(WidgetTree, TEXT("Paused"), 48), 0.0f);

	ResumeButton = SiegePauseMenu::MakeButton(WidgetTree, TEXT("Resume"));
	ResumeButton->OnClicked.AddDynamic(this, &USiegeBreakersPauseMenuWidget::HandleResumeClicked);
	AddRow(ResumeButton, 32.0f);

	QuitButton = SiegePauseMenu::MakeButton(WidgetTree, TEXT("Quit to Main Menu"));
	QuitButton->OnClicked.AddDynamic(this, &USiegeBreakersPauseMenuWidget::HandleQuitClicked);
	AddRow(QuitButton, 16.0f);

	// The host's game IS the server, so the host leaving ends the match for everyone. Say so.
	const UWorld* World = GetWorld();
	if (World && World->GetNetMode() == NM_ListenServer)
	{
		AddRow(SiegePauseMenu::MakeText(WidgetTree, TEXT("Warning: Quitting as Host ends the game for everyone."), 14), 12.0f);
	}
}

FReply USiegeBreakersPauseMenuWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		HandleResumeClicked();
		return FReply::Handled();
	}

	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void USiegeBreakersPauseMenuWidget::HandleResumeClicked()
{
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		GameInstance->GetSubsystem<USiegeBreakersPauseMenuSubsystem>()->ClosePauseMenu();
	}
}

void USiegeBreakersPauseMenuWidget::HandleQuitClicked()
{
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		GameInstance->GetSubsystem<USiegeBreakersPauseMenuSubsystem>()->ClosePauseMenu();
		GameInstance->GetSubsystem<USiegeBreakersNetworkSubsystem>()->ReturnToMainMenu();
	}
}
