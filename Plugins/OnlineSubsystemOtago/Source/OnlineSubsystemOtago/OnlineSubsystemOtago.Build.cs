using UnrealBuildTool;

public class OnlineSubsystemOtago : ModuleRules
{
	public OnlineSubsystemOtago(ReadOnlyTargetRules Target) : base(Target)
	{
		Definitions.Add("ONLINESUBSYSTEMOTAGO_PACKAGE=1");

		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicIncludePaths.AddRange(
			new string[] {
				"OnlineSubsystemOtago/Public"
			}
			);

		PrivateIncludePaths.AddRange(
			new string[] {
				"OnlineSubsystemOtago/Private",
			}
			);

		PublicDependencyModuleNames.AddRange(
			new string[]
            {
                "Core",
                "OnlineSubsystem",
                "OnlineSubsystemUtils",
            }
			);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"Sockets",
				"Networking",
				"OnlineSubsystem",
				"OnlineSubsystemUtils",
                "Slate",
                "SlateCore",
                "JsonUtilities",
                "Voice",
				"Http",
				"Json"
            }
			);


		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
			}
			);
	}
}
