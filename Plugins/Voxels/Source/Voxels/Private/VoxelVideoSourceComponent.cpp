// Fill out your copyright notice in the Description page of Project Settings.

#include "VoxelVideoSourceComponent.h"
#include "Engine.h"
#include <chrono>
#include <string>
#include "VoxelRenderSubComponent.h"
#include <exception>
#include "VIMR/VoxelVideo.hpp"
#include "VIMR/VideoPlayer.hpp"

using namespace VIMR;
using namespace std::placeholders;
using std::string;

// Sets default values for this component's properties
UVoxelVideoSourceComponent::UVoxelVideoSourceComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;
}

// Called when the game starts
void UVoxelVideoSourceComponent::BeginPlay()
{
	Super::BeginPlay();
	string filePath;
	VIMRconfig->get<string>("SharedDataPath", filePath);
	baseRecordingPath = FString(ANSI_TO_TCHAR(filePath.c_str()));
	FString VoxelVideoFilePath = baseRecordingPath  + FString("/") + VideoFileName;
  	VoxelVideoReader = new VoxVidPlayer(TCHAR_TO_ANSI(*VoxelVideoFilePath), std::bind(&UVoxelSourceBaseComponent::CopyVoxelData, this, _1));
	VoxelVideoReader->Loop(false);
	UE_LOG(VoxLog, Log, TEXT("Loaded file %s"), ANSI_TO_TCHAR(filePath.c_str()));

	
	VIMR::AudioStream tmp_astrm;
	while(VoxelVideoReader->GetNextAudioStream(tmp_astrm)){
		URuntimeAudioSource* newSource = NewObject<URuntimeAudioSource>(this);
		newSource->RegisterComponent();
		newSource->AttachToComponent(this, FAttachmentTransformRules(EAttachmentRule::KeepRelative, false));
		FString wav_path = baseRecordingPath + FString("/") + FString(tmp_astrm.file_name);
		newSource->LoadWav(wav_path);
		AudioStreams[tmp_astrm.voxel_label] = newSource;
		FString wav_label = FString(tmp_astrm.voxel_label);
		UE_LOG(VoxLog, Log, TEXT("Loaded wav: %s"), *wav_path);
		UE_LOG(VoxLog, Log, TEXT("Loaded wav: %s"), *wav_label);
	}
	Play();
}


void UVoxelVideoSourceComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
	VoxelVideoReader->Close();
}

// Called every frame
void UVoxelVideoSourceComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
		if (VoxelVideoReader != nullptr) {
		if (VoxelVideoReader->State() == VIMR::VoxVidPlayer::PlayState::Finished) {
			//Go to next video
		}
	}

	if (!cmdStack.empty()){
		cmdStack.top()();
		cmdStack.pop();
	}
}

void UVoxelVideoSourceComponent::_pause()
{
	//VoxelVideoReader->Pause();
	for (auto i : AudioStreams) {
		i.second->Pause();
	}
}

void UVoxelVideoSourceComponent::_play()
{
	VoxelVideoReader->Play();
	for (auto i : AudioStreams) {
		i.second->Start();
	}
}

void UVoxelVideoSourceComponent::_stop()
{
	_restart();
	_pause();
}

void UVoxelVideoSourceComponent::_restart()
{
	VoxelVideoReader->Restart();
	for (auto i : AudioStreams) {
		i.second->Stop();
		i.second->Start();
	}
}