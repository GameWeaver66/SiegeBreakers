// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class SiegeBreakers : ModuleRules
{
	public SiegeBreakers(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"UMG",
			"Slate"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		// Online: EOS login, lobbies and join codes (Networking/)
		PrivateDependencyModuleNames.AddRange(new string[] {
			"CoreOnline",
			"OnlineServicesInterface",
			"OnlineServicesEpicCommon",
			"EOSShared",
			"EOSSDK"
		});

		PublicIncludePaths.AddRange(new string[] {
			"SiegeBreakers",
			"SiegeBreakers/Variant_Platforming",
			"SiegeBreakers/Variant_Platforming/Animation",
			"SiegeBreakers/Variant_Combat",
			"SiegeBreakers/Variant_Combat/AI",
			"SiegeBreakers/Variant_Combat/Animation",
			"SiegeBreakers/Variant_Combat/Gameplay",
			"SiegeBreakers/Variant_Combat/Interfaces",
			"SiegeBreakers/Variant_Combat/UI",
			"SiegeBreakers/Variant_SideScrolling",
			"SiegeBreakers/Variant_SideScrolling/AI",
			"SiegeBreakers/Variant_SideScrolling/Gameplay",
			"SiegeBreakers/Variant_SideScrolling/Interfaces",
			"SiegeBreakers/Variant_SideScrolling/UI"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
