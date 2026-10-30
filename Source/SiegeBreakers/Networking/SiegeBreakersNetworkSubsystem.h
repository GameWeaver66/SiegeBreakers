// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Online/CoreOnline.h"
#include "eos_connect_types.h"
#include "SiegeBreakersNetworkSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSiegeLoginComplete, bool, bWasSuccessful);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSiegeHostComplete, bool, bWasSuccessful, const FString&, JoinCode);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSiegeJoinComplete, bool, bWasSuccessful);

/**
 *  Owns everything online for the game: EOS login, hosting and joining with a join code.
 *  Lives as long as the GameInstance, so it survives map changes.
 *
 *  Players never sign in to an Epic account. Each PC gets an anonymous EOS "Device ID",
 *  which is enough for lobbies and the peer-to-peer relay.
 *
 *  Hosting: create an EOS lobby tagged with a random 6 digit code, then open the map as a listen server.
 *  Joining: search for the lobby with that code, join it, then connect to the lobby owner peer-to-peer.
 */
UCLASS()
class USiegeBreakersNetworkSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** Logs this PC in to EOS with its Device ID. Safe to call more than once. */
	UFUNCTION(BlueprintCallable, Category="Networking")
	void Login();

	/** Creates a lobby with a new join code and opens the game map as the host */
	UFUNCTION(BlueprintCallable, Category="Networking")
	void HostGame();

	/** Finds the lobby with this join code and connects to its host */
	UFUNCTION(BlueprintCallable, Category="Networking")
	void JoinGame(const FString& JoinCode);

	/** True once EOS login has succeeded */
	UFUNCTION(BlueprintPure, Category="Networking")
	bool IsLoggedIn() const { return bLoggedIn; }

	/** The code other players type to join us. Empty if we are not hosting. */
	UFUNCTION(BlueprintPure, Category="Networking")
	FString GetJoinCode() const { return CurrentJoinCode; }

	UPROPERTY(BlueprintAssignable, Category="Networking")
	FOnSiegeLoginComplete OnLoginComplete;

	UPROPERTY(BlueprintAssignable, Category="Networking")
	FOnSiegeHostComplete OnHostComplete;

	UPROPERTY(BlueprintAssignable, Category="Networking")
	FOnSiegeJoinComplete OnJoinComplete;

protected:

	/** Map the host opens and everyone plays on */
	UPROPERTY(EditDefaultsOnly, Category="Networking")
	FString GameMapPath = TEXT("/Game/ThirdPerson/Lvl_ThirdPerson");

	/** Most players allowed in one game */
	UPROPERTY(EditDefaultsOnly, Category="Networking")
	int32 MaxPlayers = 8;

	/** Step 1 of login: make sure this PC has an EOS Device ID (created once, then reused) */
	void CreateDeviceId();

	/** Step 2 of login: log in to EOS Connect using that Device ID */
	void LoginWithDeviceId();

	void FinishLogin(bool bWasSuccessful, const FString& Message);

	/** EOS is a C library, so it reports back through plain static functions */
	static void EOS_CALL HandleDeleteDeviceIdComplete(const EOS_Connect_DeleteDeviceIdCallbackInfo* Data);
	static void EOS_CALL HandleCreateDeviceIdComplete(const EOS_Connect_CreateDeviceIdCallbackInfo* Data);

	/** Connects to the owner of a lobby we have joined */
	void TravelToLobbyHost(UE::Online::FLobbyId LobbyId);

	/** Leaves the lobby we are in, if any */
	void LeaveLobby();

	/**
	 *  Switches the game's network driver to EOS peer-to-peer.
	 *  Done only when hosting or joining through EOS, so Play In Editor keeps using normal IP networking.
	 */
	void UseEOSNetDriver();

	/** Shows a message on screen and in the log */
	void ShowMessage(const FString& Message, bool bIsError = false) const;

	UE::Online::FAccountId LocalAccountId;
	UE::Online::FLobbyId CurrentLobbyId;
	FString CurrentJoinCode;

	bool bLoggedIn = false;
	bool bLoginInProgress = false;
	bool bBusy = false;
};
