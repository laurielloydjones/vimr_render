// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/Engine.h"
#include "Kismet/GameplayStatics.h"
#include "Developer/DesktopPlatform/Public/IDesktopPlatform.h"
#include "Developer/DesktopPlatform/Public/DesktopPlatformModule.h"
#include "Editor/MainFrame/Public/Interfaces/IMainFrameModule.h"
#include "Runtime/SlateCore/Public/Widgets/SWindow.h"
#include "VoxelSourceBaseComponent.h"
#include "VIMR/voxgrid.hpp"
#include "VIMR/vidplayer.hpp"
#include "RuntimeAudioSource.h"
#include <functional>
#include <stack>
#include "VoxelVideoSourceComponent.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(VoxVidLog, All, All);

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPlaybackFinished); // Macro for setting up dispatcher event. 

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class VOXELS_API UVoxelVideoSourceComponent : public UVoxelSourceBaseComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UVoxelVideoSourceComponent();

	// UE4 METHODS

	/**
	*	Main update method which is called every frame automatically by Unreal Engine.
	*	
	*	@param DeltaTime
	*	@param TickType
	*	@param ThisTickFunction
	*/
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	
	UFUNCTION(BlueprintCallable, Category = "PlaybackControl")
		void Pause(){cmdStack.push((PlaybackControlFnPtr)std::bind(&UVoxelVideoSourceComponent::_pause, this));}
	UFUNCTION(BlueprintCallable, Category = "PlaybackControl")
		void Play(){cmdStack.push((PlaybackControlFnPtr)std::bind(&UVoxelVideoSourceComponent::_play, this));}
	UFUNCTION(BlueprintCallable, Category = "PlaybackControl")
		void Stop()	{cmdStack.push((PlaybackControlFnPtr)std::bind(&UVoxelVideoSourceComponent::_stop, this));}
	UFUNCTION(BlueprintCallable, Category = "PlaybackControl")
		void Restart(){cmdStack.push((PlaybackControlFnPtr)std::bind(&UVoxelVideoSourceComponent::_restart, this));}

	UFUNCTION(BlueprintCallable, Category = "FileManagement")
		TArray<FString> GetAllRecordings();
	UFUNCTION(BlueprintCallable, Category = "FileManagement")
		void LoadVoxelVideo(FString filepath);

	UFUNCTION(BlueprintCallable)
		void SetAudioLocation(FVector Location);

	UPROPERTY(BlueprintAssignable, Category = "EventDispatchers")
		FOnPlaybackFinished OnPlaybackFinished;
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
		FString VideoFileName = "voxvid0.vx3";

	UPROPERTY(BlueprintReadWrite,  EditAnywhere)
		FString FileName;

protected:

	FString baseRecordingPath;
	FString voxelvideosPath;

	virtual void BeginPlay() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	typedef std::function<void(void)> PlaybackControlFnPtr;
	std::stack<PlaybackControlFnPtr> cmdStack;

	std::map<std::string, URuntimeAudioSource*> AudioStreams;

	int NumAudioStreams = 0;

	// DON'T instantate these things here. UBT can't handle it. Do it in BeginPlay
	VIMR::VoxVidPlayer *VoxelVideoReader = nullptr;

	void _pause();
	void _play();
	void _stop();
	void _restart();
};