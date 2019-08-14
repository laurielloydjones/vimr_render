// Fill out your copyright notice in the Description page of Project Settings.

#include "VoxelVideoSourceComponent.h"
#include "Engine.h"
#include <chrono>
#include "VoxelRenderSubComponent.h"
#include <exception>
#include "VIMR/RawVoxelVideo.hpp"
#include "VIMR/ZippedVoxelVideo.hpp"

using namespace VIMR;
using namespace std::placeholders;

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
	SerialOctreeFmtVersion = 1;
	Super::BeginPlay();
	SetComponentTickEnabled(false);
	VoxelVideoReader = nullptr;
	if (CurrentFrame == nullptr)
		CurrentFrame = new VIMR::Octree();
	string tmpstr;
	VIMRconfig->get<string>("SharedDataPath", tmpstr);
	// The voxel video path is expected to be in the Recordings directory under the UE4 project contents folder.
	FullRecordingPath = FString(tmpstr.c_str()) + VideoFileName;
	if (!AVoxelVideo::Exists(TCHAR_TO_ANSI(*FullRecordingPath))) {
		UE_LOG(VoxLog, Error, TEXT("File does not exist: %s"), *FullRecordingPath);
	}
	else if (AVoxelVideo::IsZipped(TCHAR_TO_ANSI(*FullRecordingPath))) {
		UE_LOG(VoxLog, Error, TEXT("Compressed voxel video not supported yet: %s"), *FullRecordingPath);
	}
	else if(AVoxelVideo::IsRaw(TCHAR_TO_ANSI(*FullRecordingPath))) {
		UE_LOG(VoxLog, Log, TEXT("Loading raw voxel video from file: %s"), *FullRecordingPath);
		VoxelVideoReader = new RawVoxelVideo(TCHAR_TO_ANSI(*FullRecordingPath));
	}
	else {
		UE_LOG(VoxLog, Error, TEXT("Unknown file extension. File: %s"), *FullRecordingPath);
	}

	if (VoxelVideoReader != nullptr) {
		SetComponentTickEnabled(true);
		NumAudioStreams = VoxelVideoReader->GetAudioStreamCount();
		UE_LOG(VoxLog, Log, TEXT("VoxelVideo: Loading %d audio streams"), NumAudioStreams);
		AudioStreams = new URuntimeAudioSource*[NumAudioStreams];
		for (int i = 0; i < NumAudioStreams; i++) {
			URuntimeAudioSource* newSource = NewObject<URuntimeAudioSource>(this);
			newSource->RegisterComponent();
			newSource->AttachToComponent(this, FAttachmentTransformRules(EAttachmentRule::KeepRelative, false));
			VIMR::AudioStream aMap;
			VoxelVideoReader->GetAudioStream(i, aMap);
			newSource->LoadBuffer(aMap.WaveBuffer, aMap.WaveBufferSize);
			AudioStreams[i] = newSource;
			AudioStreams[i]->StartTimeSec = aMap.SyncOffsetSec;
		}
		VoxelVideoReader->SetPaused(false);
		for (int i = 0; i < NumAudioStreams; i++) {
			AudioStreams[i]->Stop();
			AudioStreams[i]->Start();
		}
	}
	else {
		UKismetSystemLibrary::QuitGame(GetWorld(), GetWorld()->GetFirstPlayerController(), EQuitPreference::Quit);
	}
}


void UVoxelVideoSourceComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
	if (VoxelVideoReader != nullptr) {
		VoxelVideoReader->Close();
	}
	
}

// Called every frame
void UVoxelVideoSourceComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!cmdStack.empty()) {
		cmdStack.top()();
		cmdStack.pop();
	}
	
	VIMR::PlaybackState playState = VoxelVideoReader->NextFrame(&CurrentFrame);
	if (playState == VIMR::PlaybackNormal) {
		CopyVoxelData(CurrentFrame);
	}
	else if (playState == VIMR::PlaybackFinished) {
		UE_LOG(VoxLog, Log, TEXT("Video Ended. Restarting Playback."));
		VoxelVideoReader->Reset();
	}
}

void UVoxelVideoSourceComponent::_pause()
{

}

void UVoxelVideoSourceComponent::_play()
{

}

void UVoxelVideoSourceComponent::_stop()
{

}
