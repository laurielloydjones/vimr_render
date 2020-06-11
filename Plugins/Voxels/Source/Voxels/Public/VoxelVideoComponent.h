// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Delegates/Delegate.h"
#include "RuntimeAudioSource.h"
#include "Voxels.h"
#include "VoxelSourceInterface.h"
#include "VoxelSourceBaseComponent.h"

#include "VIMR/VoxelType.hpp"
#include "VIMR/VideoPlayer.hpp"

#include <functional>
#include <map>
#include <stack>
#include <string>

#include "VoxelVideoComponent.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(VoxVidLog, All, All);

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPlaybackFinished); // Macro for setting up dispatcher event. 

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class VOXELS_API UVoxelVideoComponent : public UVoxelSourceBaseComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UVoxelVideoComponent();

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
		void Pause();
	UFUNCTION(BlueprintCallable, Category = "PlaybackControl")
		void Play();
	UFUNCTION(BlueprintCallable, Category = "PlaybackControl")
		void Stop();
	UFUNCTION(BlueprintCallable, Category = "PlaybackControl")
		void Restart();

	UPROPERTY(BlueprintAssignable, Category = "EventDispatchers")
		FOnPlaybackFinished OnPlaybackFinished;
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
		FString RecordingPath;
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
		bool PlaybackFinished = false;
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
		bool IsPaused = false;
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
		bool Looping = true;

protected:

	// Called when the game starts
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	void OnPlaybackFinishedEvent();

	uint32_t MaxVoxels = 196608;
	FString FullRecordingPath;

	typedef std::function<void(void)> PlaybackControlFnPtr;

	std::map<std::string, URuntimeAudioSource*> AudioStreams;

	// DON'T instantate these things here. UBT can't handle it. Do it in BeginPlay
	VIMR::VoxVidPlayer *VoxelVideoReader = nullptr;
	VIMR::VoxelGrid* CurrentFrame = nullptr;
	VIMR::VoxelGrid::Voxel* voxel = nullptr;

	std::stack<PlaybackControlFnPtr> cmdStack;
	std::stack<FVector> audioVoxStack;

	// GAME-THREAD FUNCTIONS WHICH ARE INVOKED FOR VARIOUS PLAYBACK CONTROL ACTIONS
	void _pause();
	void _play();
	void _stop();
	void _restart();

};