// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "eos_connect_types.h"
#include "SiegeBreakersNetworkSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSiegeLoginComplete, bool, bWasSuccessful);

/**
 *  Owns everything online for the game: EOS login now, lobbies and join codes later.
 *  Lives as long as the GameInstance, so it survives map changes.
 *
 *  Players never sign in to an Epic account. Each PC gets an anonymous EOS "Device ID",
 *  which is enough for lobbies and the peer-to-peer relay.
 */
UCLASS()
class USiegeBreakersNetworkSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** Logs this PC in to EOS with its Device ID. Safe to call more than once. */
	UFUNCTION(BlueprintCallable, Category="Networking")
	void Login();

	/** True once EOS login has succeeded */
	UFUNCTION(BlueprintPure, Category="Networking")
	bool IsLoggedIn() const { return bLoggedIn; }

	/** Called when a login attempt finishes */
	UPROPERTY(BlueprintAssignable, Category="Networking")
	FOnSiegeLoginComplete OnLoginComplete;

protected:

	/** Step 1: make sure this PC has an EOS Device ID (created once, then reused) */
	void CreateDeviceId();

	/** EOS is a C library, so it reports back through a plain static function */
	static void EOS_CALL HandleCreateDeviceIdComplete(const EOS_Connect_CreateDeviceIdCallbackInfo* Data);

	/** Step 2: log in to EOS Connect using that Device ID */
	void LoginWithDeviceId();

	void FinishLogin(bool bWasSuccessful, const FString& Message);

	bool bLoggedIn = false;
	bool bLoginInProgress = false;
};
