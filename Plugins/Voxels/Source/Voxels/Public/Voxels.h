// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ModuleManager.h"

DECLARE_LOG_CATEGORY_EXTERN(VoxLog, All, All);

class FVoxelsModule : public IModuleInterface
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};

enum DebugMessageKeys
{
	VoxelCountKey,
	VoxelWarningKey,
	ReadFrameKey,
};

enum SourceTypes
{
	VoxelVideoSource,
	UDPSource,
	UDPServerSource,
	DebugSource,
	RecordingSource,
	KinectSource,
	GVDBSource,
};
