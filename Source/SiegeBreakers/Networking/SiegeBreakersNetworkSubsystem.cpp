// Copyright Epic Games, Inc. All Rights Reserved.

#include "Networking/SiegeBreakersNetworkSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "IEOSSDKManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Online/Auth.h"
#include "Online/Lobbies.h"
#include "Online/OnlineServices.h"
#include "Online/OnlineServicesEpicCommon.h"
#include "eos_connect.h"
#include "eos_sdk.h"

DEFINE_LOG_CATEGORY_STATIC(LogSiegeNet, Log, All);

using namespace UE::Online;

namespace SiegeNet
{
	/** Lobby schema and attribute names, declared in Config/DefaultEngine.ini under [OnlineServices.Lobbies] */
	const FName LobbySchema = TEXT("SiegeLobby");
	const FName JoinCodeAttribute = TEXT("JoinCode");
	const FName LobbyLocalName = TEXT("GameLobby");

	/** On-screen message slot for the join code, so it stays up instead of scrolling away */
	constexpr int32 JoinCodeMessageKey = 4242;

	USiegeBreakersNetworkSubsystem* GetSubsystem(UWorld* World)
	{
		UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
		return GameInstance ? GameInstance->GetSubsystem<USiegeBreakersNetworkSubsystem>() : nullptr;
	}
}

// Console commands for testing: open the console with ~ and type one of these
static FAutoConsoleCommandWithWorld GSiegeLoginCommand(
	TEXT("sb.Login"),
	TEXT("Log in to EOS with this PC's Device ID"),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		if (USiegeBreakersNetworkSubsystem* Subsystem = SiegeNet::GetSubsystem(World))
		{
			Subsystem->Login();
		}
	}));

static FAutoConsoleCommandWithWorld GSiegeHostCommand(
	TEXT("sb.Host"),
	TEXT("Host a game and show its join code"),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		if (USiegeBreakersNetworkSubsystem* Subsystem = SiegeNet::GetSubsystem(World))
		{
			Subsystem->HostGame();
		}
	}));

static FAutoConsoleCommandWithWorldAndArgs GSiegeJoinCommand(
	TEXT("sb.Join"),
	TEXT("Join a game: sb.Join 123456"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (USiegeBreakersNetworkSubsystem* Subsystem = SiegeNet::GetSubsystem(World))
		{
			Subsystem->JoinGame(Args.Num() > 0 ? Args[0] : FString());
		}
	}));

void USiegeBreakersNetworkSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// In the editor, PIE runs every player in one process, so skip the automatic login there
	if (!GIsEditor)
	{
		Login();
	}
}

void USiegeBreakersNetworkSubsystem::Deinitialize()
{
	LeaveLobby();

	Super::Deinitialize();
}

// ---------------------------------------------------------------------------------------------------------------------
// Login

void USiegeBreakersNetworkSubsystem::Login()
{
	if (bLoggedIn || bLoginInProgress)
	{
		return;
	}

	bLoginInProgress = true;
	UE_LOG(LogSiegeNet, Log, TEXT("EOS login started"));
	CreateDeviceId();
}

void USiegeBreakersNetworkSubsystem::CreateDeviceId()
{
	TSharedPtr<FOnlineServicesEpicCommon> Services = StaticCastSharedPtr<FOnlineServicesEpicCommon>(GetServices(EOnlineServices::Epic));
	if (!Services || !Services->GetEOSPlatformHandle())
	{
		FinishLogin(false, TEXT("EOS is not available. Check the [EOSSDK.Platform.SiegeBreakers] config and Config/UserEngine.ini"));
		return;
	}

	EOS_HConnect ConnectHandle = EOS_Platform_GetConnectInterface(*Services->GetEOSPlatformHandle());

	// Testing aid: two copies of the game on one PC share a Device ID, so they would be the same player.
	// Start the second copy with -SBNewDeviceId to throw its Device ID away and get a new player identity.
	if (FParse::Param(FCommandLine::Get(), TEXT("SBNewDeviceId")))
	{
		EOS_Connect_DeleteDeviceIdOptions DeleteOptions = {};
		DeleteOptions.ApiVersion = EOS_CONNECT_DELETEDEVICEID_API_LATEST;
		EOS_Connect_DeleteDeviceId(ConnectHandle, &DeleteOptions, this, &USiegeBreakersNetworkSubsystem::HandleDeleteDeviceIdComplete);
		return;
	}

	EOS_Connect_CreateDeviceIdOptions Options = {};
	Options.ApiVersion = EOS_CONNECT_CREATEDEVICEID_API_LATEST;
	Options.DeviceModel = "PC";

	EOS_Connect_CreateDeviceId(ConnectHandle, &Options, this, &USiegeBreakersNetworkSubsystem::HandleCreateDeviceIdComplete);
}

void EOS_CALL USiegeBreakersNetworkSubsystem::HandleDeleteDeviceIdComplete(const EOS_Connect_DeleteDeviceIdCallbackInfo* Data)
{
	USiegeBreakersNetworkSubsystem* Subsystem = static_cast<USiegeBreakersNetworkSubsystem*>(Data->ClientData);
	UE_LOG(LogSiegeNet, Log, TEXT("Deleted old Device ID: %hs"), EOS_EResult_ToString(Data->ResultCode));

	TSharedPtr<FOnlineServicesEpicCommon> Services = StaticCastSharedPtr<FOnlineServicesEpicCommon>(GetServices(EOnlineServices::Epic));
	EOS_HConnect ConnectHandle = EOS_Platform_GetConnectInterface(*Services->GetEOSPlatformHandle());

	EOS_Connect_CreateDeviceIdOptions Options = {};
	Options.ApiVersion = EOS_CONNECT_CREATEDEVICEID_API_LATEST;
	Options.DeviceModel = "PC";
	EOS_Connect_CreateDeviceId(ConnectHandle, &Options, Subsystem, &USiegeBreakersNetworkSubsystem::HandleCreateDeviceIdComplete);
}

void EOS_CALL USiegeBreakersNetworkSubsystem::HandleCreateDeviceIdComplete(const EOS_Connect_CreateDeviceIdCallbackInfo* Data)
{
	USiegeBreakersNetworkSubsystem* Subsystem = static_cast<USiegeBreakersNetworkSubsystem*>(Data->ClientData);

	// DuplicateNotAllowed just means this PC already made a Device ID on an earlier run
	if (Data->ResultCode == EOS_EResult::EOS_Success || Data->ResultCode == EOS_EResult::EOS_DuplicateNotAllowed)
	{
		Subsystem->LoginWithDeviceId();
	}
	else
	{
		Subsystem->FinishLogin(false, FString::Printf(TEXT("Creating the Device ID failed: %hs"), EOS_EResult_ToString(Data->ResultCode)));
	}
}

void USiegeBreakersNetworkSubsystem::LoginWithDeviceId()
{
	IAuthPtr Auth = GetServices(EOnlineServices::Epic)->GetAuthInterface();

	// The Device ID login has no token: EOS finds the ID saved on this PC by CreateDeviceId.
	// The first time a PC logs in, the engine also creates its EOS player account for us.
	FAuthLogin::Params Params;
	Params.PlatformUserId = FPlatformMisc::GetPlatformUserForUserIndex(0);
	Params.CredentialsType = LoginCredentialsType::ExternalAuth;
	Params.CredentialsToken.Emplace<FExternalAuthToken>(FExternalAuthToken{ ExternalLoginType::DeviceIdAccessToken, FString() });

	Auth->Login(MoveTemp(Params)).OnComplete(this, [this](const TOnlineResult<FAuthLogin>& Result)
	{
		if (Result.IsOk())
		{
			LocalAccountId = Result.GetOkValue().AccountInfo->AccountId;
			FinishLogin(true, FString::Printf(TEXT("Logged in as %s"), *ToLogString(LocalAccountId)));
		}
		else
		{
			FinishLogin(false, Result.GetErrorValue().GetLogString());
		}
	});
}

void USiegeBreakersNetworkSubsystem::FinishLogin(bool bWasSuccessful, const FString& Message)
{
	bLoginInProgress = false;
	bLoggedIn = bWasSuccessful;

	if (bWasSuccessful)
	{
		ShowMessage(FString::Printf(TEXT("EOS login succeeded. %s"), *Message));
	}
	else
	{
		ShowMessage(FString::Printf(TEXT("EOS login failed. %s"), *Message), true);
	}

	OnLoginComplete.Broadcast(bWasSuccessful);

	// Launch options for testing and demos: -SBHost hosts right away, -SBJoin=123456 joins that code
	FString AutoJoinCode;
	if (bWasSuccessful && FParse::Param(FCommandLine::Get(), TEXT("SBHost")))
	{
		HostGame();
	}
	else if (bWasSuccessful && FParse::Value(FCommandLine::Get(), TEXT("SBJoin="), AutoJoinCode))
	{
		JoinGame(AutoJoinCode);
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// Hosting

void USiegeBreakersNetworkSubsystem::HostGame()
{
	if (!bLoggedIn || bBusy)
	{
		ShowMessage(bBusy ? TEXT("Still working on the last request") : TEXT("Can't host: not logged in to EOS yet"), true);
		OnHostComplete.Broadcast(false, FString());
		return;
	}

	LeaveLobby();
	bBusy = true;

	// 6 digits, never starting with 0 so it reads the same as a number
	const int64 Code = FMath::RandRange(100000, 999999);

	FCreateLobby::Params Params;
	Params.LocalAccountId = LocalAccountId;
	Params.LocalName = SiegeNet::LobbyLocalName;
	Params.SchemaId = SiegeNet::LobbySchema;
	Params.MaxMembers = MaxPlayers;
	Params.JoinPolicy = ELobbyJoinPolicy::PublicAdvertised;
	Params.Attributes.Emplace(SiegeNet::JoinCodeAttribute, FSchemaVariant(Code));

	ShowMessage(TEXT("Creating lobby..."));

	GetServices(EOnlineServices::Epic)->GetLobbiesInterface()->CreateLobby(MoveTemp(Params)).OnComplete(this, [this, Code](const TOnlineResult<FCreateLobby>& Result)
	{
		bBusy = false;

		if (Result.IsError())
		{
			ShowMessage(FString::Printf(TEXT("Creating the lobby failed: %s"), *Result.GetErrorValue().GetLogString()), true);
			OnHostComplete.Broadcast(false, FString());
			return;
		}

		CurrentLobbyId = Result.GetOkValue().Lobby->LobbyId;
		CurrentJoinCode = FString::Printf(TEXT("%lld"), Code);

		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(SiegeNet::JoinCodeMessageKey, 3600.0f, FColor::Yellow, FString::Printf(TEXT("JOIN CODE: %s"), *CurrentJoinCode), true, FVector2D(2.0f, 2.0f));
		}
		UE_LOG(LogSiegeNet, Log, TEXT("Hosting with join code %s"), *CurrentJoinCode);

		// Reopen the map as a listen server: our game becomes the host the other players connect to
		UseEOSNetDriver();
		UGameplayStatics::OpenLevel(GetGameInstance(), FName(*GameMapPath), true, TEXT("listen"));

		OnHostComplete.Broadcast(true, CurrentJoinCode);
	});
}

// ---------------------------------------------------------------------------------------------------------------------
// Joining

void USiegeBreakersNetworkSubsystem::JoinGame(const FString& JoinCode)
{
	const FString Code = JoinCode.TrimStartAndEnd();

	if (!bLoggedIn || bBusy)
	{
		ShowMessage(bBusy ? TEXT("Still working on the last request") : TEXT("Can't join: not logged in to EOS yet"), true);
		OnJoinComplete.Broadcast(false);
		return;
	}

	if (Code.Len() != 6 || !Code.IsNumeric())
	{
		ShowMessage(TEXT("Join codes are 6 digits"), true);
		OnJoinComplete.Broadcast(false);
		return;
	}

	LeaveLobby();
	bBusy = true;

	FFindLobbies::Params Params;
	Params.LocalAccountId = LocalAccountId;
	Params.MaxResults = 1;
	Params.Filters.Add(FFindLobbySearchFilter{ SiegeNet::JoinCodeAttribute, ESchemaAttributeComparisonOp::Equals, FSchemaVariant(FCString::Atoi64(*Code)) });

	ShowMessage(FString::Printf(TEXT("Looking for game %s..."), *Code));

	ILobbiesPtr Lobbies = GetServices(EOnlineServices::Epic)->GetLobbiesInterface();
	Lobbies->FindLobbies(MoveTemp(Params)).OnComplete(this, [this, Code](const TOnlineResult<FFindLobbies>& FindResult)
	{
		if (FindResult.IsError() || FindResult.GetOkValue().Lobbies.IsEmpty())
		{
			bBusy = false;
			ShowMessage(FString::Printf(TEXT("No game found with code %s"), *Code), true);
			OnJoinComplete.Broadcast(false);
			return;
		}

		FJoinLobby::Params JoinParams;
		JoinParams.LocalAccountId = LocalAccountId;
		JoinParams.LocalName = SiegeNet::LobbyLocalName;
		JoinParams.LobbyId = FindResult.GetOkValue().Lobbies[0]->LobbyId;

		GetServices(EOnlineServices::Epic)->GetLobbiesInterface()->JoinLobby(MoveTemp(JoinParams)).OnComplete(this, [this, Code](const TOnlineResult<FJoinLobby>& JoinResult)
		{
			bBusy = false;

			if (JoinResult.IsError())
			{
				ShowMessage(FString::Printf(TEXT("Joining the lobby failed: %s"), *JoinResult.GetErrorValue().GetLogString()), true);
				OnJoinComplete.Broadcast(false);
				return;
			}

			CurrentLobbyId = JoinResult.GetOkValue().Lobby->LobbyId;
			ShowMessage(FString::Printf(TEXT("Joined lobby %s, connecting to the host..."), *Code));
			TravelToLobbyHost(CurrentLobbyId);
		});
	});
}

void USiegeBreakersNetworkSubsystem::TravelToLobbyHost(FLobbyId LobbyId)
{
	// The engine turns the lobby owner's EOS id into an address like "EOS:0002aeeb..." for the network driver
	FGetResolvedConnectString::Params Params;
	Params.LocalAccountId = LocalAccountId;
	Params.LobbyId = LobbyId;

	TOnlineResult<FGetResolvedConnectString> Result = GetServices(EOnlineServices::Epic)->GetResolvedConnectString(MoveTemp(Params));
	APlayerController* PlayerController = GetGameInstance()->GetFirstLocalPlayerController();

	if (Result.IsError() || !PlayerController)
	{
		ShowMessage(TEXT("Couldn't get the host's address"), true);
		OnJoinComplete.Broadcast(false);
		return;
	}

	const FString& Address = Result.GetOkValue().ResolvedConnectString;
	UE_LOG(LogSiegeNet, Log, TEXT("Connecting to host at %s"), *Address);

	UseEOSNetDriver();
	PlayerController->ClientTravel(Address, TRAVEL_Absolute);

	OnJoinComplete.Broadcast(true);
}

// ---------------------------------------------------------------------------------------------------------------------
// Helpers

void USiegeBreakersNetworkSubsystem::LeaveLobby()
{
	if (!CurrentLobbyId.IsValid())
	{
		return;
	}

	if (IOnlineServicesPtr Services = GetServices(EOnlineServices::Epic))
	{
		FLeaveLobby::Params Params;
		Params.LocalAccountId = LocalAccountId;
		Params.LobbyId = CurrentLobbyId;
		Services->GetLobbiesInterface()->LeaveLobby(MoveTemp(Params));
	}

	CurrentLobbyId = FLobbyId();
	CurrentJoinCode.Empty();

	if (GEngine)
	{
		GEngine->RemoveOnScreenDebugMessage(SiegeNet::JoinCodeMessageKey);
	}
}

void USiegeBreakersNetworkSubsystem::UseEOSNetDriver()
{
	// The default network driver (set in BaseEngine.ini) is plain IP. Swapping it here, at runtime, means only
	// games hosted or joined through EOS use the peer-to-peer relay. Unreal creates the driver when the map opens.
	for (FNetDriverDefinition& Definition : GEngine->NetDriverDefinitions)
	{
		if (Definition.DefName == NAME_GameNetDriver)
		{
			Definition.DriverClassName = TEXT("/Script/SocketSubsystemEOS.NetDriverEOS");
			Definition.DriverClassNameFallback = TEXT("/Script/OnlineSubsystemUtils.IpNetDriver");
		}
	}
}

void USiegeBreakersNetworkSubsystem::ShowMessage(const FString& Message, bool bIsError) const
{
	if (bIsError)
	{
		UE_LOG(LogSiegeNet, Error, TEXT("%s"), *Message);
	}
	else
	{
		UE_LOG(LogSiegeNet, Log, TEXT("%s"), *Message);
	}

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 8.0f, bIsError ? FColor::Red : FColor::Green, Message);
	}
}
