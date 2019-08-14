// Fill out your copyright notice in the Description page of Project Settings.

using System;
using System.IO;
using UnrealBuildTool;

public class vimr_render : ModuleRules
{
	
	public vimr_render(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		bEnableExceptions = true;
		string VIMR_ROOT = System.Environment.GetEnvironmentVariable("VIMR_ROOT");

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore" });

		PrivateDependencyModuleNames.AddRange(new string[] {  });
		if(Target.Platform == UnrealTargetPlatform.Win64)
		{
			string ProjectRoot = Directory.GetParent(ModuleDirectory).Parent.FullName;
			RuntimeDependencies.Add(ProjectRoot + @"\Binaries\Win64\Voxels.dll");
			RuntimeDependencies.Add(ProjectRoot + @"\Binaries\Win64\NetworkUDP_Lib.dll");
			RuntimeDependencies.Add(ProjectRoot + @"\Binaries\Win64\VoxelVideo.dll");
			RuntimeDependencies.Add(ProjectRoot + @"\Binaries\Win64\VoxelsAsync.dll");
			RuntimeDependencies.Add(ProjectRoot + @"\Binaries\Win64\NetworkCallbackTCP_Lib.dll");
		}
	}
}
