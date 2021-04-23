// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Voxels : ModuleRules
{
    public Voxels(ReadOnlyTargetRules Target) : base(Target)
	{
		Definitions.Add("JSON_NOEXCEPTION");
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		string VIMR_ROOT = System.Environment.GetEnvironmentVariable("VIMR_ROOT_DEV");

		PublicIncludePaths.AddRange(
			new string[] {
				"Voxels/Public"
			}
			);

		
		PrivateIncludePaths.AddRange(
			new string[] {
			    "Voxels/Private",
				VIMR_ROOT + "/include/"
            }     
			);

		
        PublicAdditionalLibraries.AddRange(
            new string[]
            {
                VIMR_ROOT + "/lib/vimr_network.lib",
                VIMR_ROOT + "/lib/vimr_voxels.lib",
                VIMR_ROOT + "/lib/vimr_voxvid.lib",
                VIMR_ROOT + "/lib/vimr_async_serial.lib",
                VIMR_ROOT + "/lib/vimr_config.lib",
                VIMR_ROOT + "/lib/vimr_cfg_unreal.lib"
            }
            );

        PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
                "HeadMountedDisplay",
            }
			);


		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore",
			}
			);


		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
			}
			);
	}
}
