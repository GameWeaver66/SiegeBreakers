// Copyright Epic Games, Inc. All Rights Reserved.

#include "Networking/SiegeBreakersNetworkSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "IEOSSDKManager.h"
#include "Online/Auth.h"
#include "Online/OnlineServices.h"
#include "Online/OnlineServicesEpicCommon.h"
#include "eos_connect.h"
#include "eos_sdk.h"

DEFINE_LOG_CATEGORY_STATIC(LogSiegeNet, Log, All);

using namespace UE::Online;

// Console command for testing in the editor: open the console with ~ and type sb.Login
static FAutoConsoleCommandWithWorld GSiegeLoginCommand(
	TEXT("sb.Login"),
	TEXT("Log in to EOS with this PC's Device ID"),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		if (UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr)
		{
			GameInstance->GetSubsystem<USiegeBreakersNetworkSubsystem>()->Login();
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

	EOS_Connect_CreateDeviceIdOptions Options = {};
	Options.ApiVersion = EOS_CONNECT_CREATEDEVICEID_API_LATEST;
	Options.DeviceModel = "PC";

	EOS_HConnect ConnectHandle = EOS_Platform_GetConnectInterface(*Services->GetEOSPlatformHandle());
	EOS_Connect_CreateDeviceId(ConnectHandle, &Options, this, &USiegeBreakersNetworkSubsystem::HandleCreateDeviceIdComplete);
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
			FinishLogin(true, FString::Printf(TEXT("Logged in as %s"), *ToLogString(Result.GetOkValue().AccountInfo->AccountId)));
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
		UE_LOG(LogSiegeNet, Log, TEXT("EOS login succeeded. %s"), *Message);
	}
	else
	{
		UE_LOG(LogSiegeNet, Error, TEXT("EOS login failed. %s"), *Message);
	}

	OnLoginComplete.Broadcast(bWasSuccessful);
}
