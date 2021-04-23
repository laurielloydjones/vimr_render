// Fill out your copyright notice in the Description page of Project Settings.

using System;
using System.IO;
using UnrealBuildTool;

public class vimr_render : ModuleRules
{
	public vimr_render(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		string VIMR_ROOT = System.Environment.GetEnvironmentVariable("VIMR_ROOT_DEV");

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore" });

		PrivateDependencyModuleNames.AddRange(new string[] {  });
		if(Target.Platform == UnrealTargetPlatform.Win64)
		{
			Definitions.Add("JSON_NOEXCEPTION");
			string ProjectRoot = Directory.GetParent(ModuleDirectory).Parent.FullName;
			RuntimeDependencies.Add(ProjectRoot + @"\Binaries\Win64\vimr_voxels.dll");
			RuntimeDependencies.Add(ProjectRoot + @"\Binaries\Win64\vimr_network.dll");
			RuntimeDependencies.Add(ProjectRoot + @"\Binaries\Win64\vimr_voxvid.dll");
			RuntimeDependencies.Add(ProjectRoot + @"\Binaries\Win64\vimr_async_serial.dll");
			RuntimeDependencies.Add(ProjectRoot + @"\Binaries\Win64\vimr_cfg_unreal.dll");
		}
	}
}
