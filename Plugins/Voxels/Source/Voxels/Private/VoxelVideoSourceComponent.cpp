


// Fill out your copyright notice in the Description page of Project Settings
#include "VoxelVideoSourceComponent.h"
#include "Engine.h"
#include <chrono>
#include <string>
#include "VoxelRenderSubComponent.h"
#include <exception>
#include "VIMR/vidplayer.hpp"

#include "Paths.h"
#include "HAL/FileManager.h"

using namespace std::placeholders;
using std::string;

DEFINE_LOG_CATEGORY(VoxVidLog);

// Sets default values for this component's properties
UVoxelVideoSourceComponent::UVoxelVideoSourceComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;
	//OnPlaybackFinished.AddDynamic(this, &UVoxelVideoSourceComponent::OnPlaybackFinishedEvent);
}

// Called when the game starts
void UVoxelVideoSourceComponent::BeginPlay()
{
	Super::BeginPlay();

	char* datapath;
	size_t dplen;

	voxelvideosPath = FPaths::ProjectContentDir() + FString("VoxelVideos/");

	if(VIMRconfig->GetString("SharedDataPath", &datapath, dplen))
	{
	}
	else
	{
		//Exit game?
	}

	LoadVoxelVideo(VideoFileName);
	//Play();
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

void UVoxelVideoSourceComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
	VoxelVideoReader->Close();
}

void UVoxelVideoSourceComponent::_pause()
{
	VoxelVideoReader->Pause();
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

void UVoxelVideoSourceComponent::LoadVoxelVideo(FString file)
{
	if (VoxelVideoReader != nullptr)
	{
		VoxelVideoReader->Close();
		
		for (auto i : AudioStreams) {
			i.second->Stop();
			i.second->clear();
		}

		AudioStreams.clear();
	}

	FileName = file;

	FString file_path = voxelvideosPath + FileName;

	VoxelVideoReader = new VIMR::VoxVidPlayer(std::bind(&UVoxelSourceBaseComponent::CopyVoxelData, this, _1));
	VoxelVideoReader->Load(TCHAR_TO_ANSI(*file_path));
	VoxelVideoReader->Loop = false;
	UE_LOG(VoxVidLog, Log, TEXT("Loaded file %s"), *file_path);

	VIMR::AudioStream tmp_astrm;
	while (VoxelVideoReader->GetNextAudioStream(tmp_astrm))
	{
		URuntimeAudioSource* newSource = NewObject<URuntimeAudioSource>(this);
		FString wav_path = voxelvideosPath + FString(tmp_astrm.file_name);
		FString wav_label = FString(tmp_astrm.voxel_label);

		newSource->RegisterComponent();
		newSource->AttachToComponent(this, FAttachmentTransformRules(EAttachmentRule::KeepRelative, false));
		newSource->LoadWav(wav_path);
		AudioStreams[tmp_astrm.voxel_label] = newSource;

		UE_LOG(VoxLog, Log, TEXT("Loaded wav: %s"), *wav_path);
		UE_LOG(VoxLog, Log, TEXT("Loaded wav: %s"), *wav_label);
	}
}

TArray<FString> UVoxelVideoSourceComponent::GetAllRecordings()
{
	TArray<FString> files;
	files.Empty();

	FString recordingPath = FPaths::ProjectContentDir() + "VoxelVideos/";
	FString voxelvideo_ext = "vx3";

	if (FPaths::DirectoryExists(*recordingPath))
	{
		IFileManager::Get().FindFiles(files, *recordingPath, *voxelvideo_ext);

		for (int i = 0; i < files.Num(); i++)
		{
			UE_LOG(LogTemp, Log, TEXT("These files Exists: %s"), *files[i]);
		}

		UE_LOG(LogTemp, Log, TEXT("This Path Exists: %s"), *recordingPath)
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("This path doesn't exist"));
	}

	return files;
}

void UVoxelVideoSourceComponent::SetAudioLocation(FVector Location)
{
	for (auto as : AudioStreams) 
	{
		as.second->GetAudioComponent()->SetWorldLocation(Location);
	}
}