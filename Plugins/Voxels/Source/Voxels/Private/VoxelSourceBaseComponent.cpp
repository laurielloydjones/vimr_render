// Fill out your copyright notice in the Description page of Project Settings.

#include "VoxelSourceBaseComponent.h"
#include "Engine.h"
#include "VoxelRenderSubComponent.h"
#include"Runtime/Engine/Classes/Kismet/KismetSystemLibrary.h"
#include <chrono>
#include <functional>
using namespace std::placeholders;

DEFINE_LOG_CATEGORY(VoxLog);

void UVoxelSourceBaseComponent::CopyVoxelData(VIMR::VoxelGrid* voxels) {
	if (inProgress) {
		FString failLogMessage = FString("Received more voxels before copying last frame finished. ID: ") + ClientConfigID;
		UE_LOG(VoxLog, Log, TEXT("%s"), *failLogMessage);
		return;
	}
	
	inProgress = true;
	VoxelCount[buffIdx] = 0;
	VoxelSizemm[buffIdx] = (uint8)voxels->VoxSize_mm();
	
	
	int pOffset = 0;
	while (voxels->GetNextVoxel(&node)) {
		//uint8_t label = VIMR::GetLabel(node->data[3]);
		//if (label > 22)
		//	continue;

		int16_t pY = node->pos.Y;
		int16_t pX = node->pos.X;
		int16_t pZ = node->pos.Z;


		ColourData[buffIdx][pOffset + 0] = node->data[1];
		ColourData[buffIdx][pOffset + 1] = node->data[2];
		ColourData[buffIdx][pOffset + 2] = node->data[3];

		CoarsePositionData[buffIdx][pOffset + 0] = (pZ >> 8) + 128;
		CoarsePositionData[buffIdx][pOffset + 1] = (pY >> 8) + 128;
		CoarsePositionData[buffIdx][pOffset + 2] = (pX >> 8) + 128;
		PositionData[buffIdx][pOffset + 0] = pZ & 0xFF;
		PositionData[buffIdx][pOffset + 1] = pY & 0xFF;
		PositionData[buffIdx][pOffset + 2] = pX & 0xFF;
		pOffset += VOXEL_TEXTURE_BPP;
		VoxelCount[buffIdx]++;

		if (VoxelCount[buffIdx] >= MaxVoxels) {
			FString failLogMessage = FString("Too Many Voxels! ID: ") + ClientConfigID;
			UE_LOG(VoxLog, Log, TEXT("%s"), *failLogMessage);
			break;
		}
	}
	
	while (inDisplay)
		;

	buffIdx = (buffIdx + 1) % BufferSize;
	dispIdx = (dispIdx + 1) % BufferSize;
	inProgress = false;
	voxels->Reset();
}

// Sets default values for this component's properties
UVoxelSourceBaseComponent::UVoxelSourceBaseComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;
	
}

// Called when the game starts
void UVoxelSourceBaseComponent::BeginPlay()
{
	Super::BeginPlay();
	for (int i = 0; i < BufferSize; i++) {
		CoarsePositionData[i] = new uint8[MaxVoxels * VOXEL_TEXTURE_BPP]();
		PositionData[i] = new uint8[MaxVoxels * VOXEL_TEXTURE_BPP]();
		ColourData[i] = new uint8[MaxVoxels * VOXEL_TEXTURE_BPP]();
		VoxelCount[i] = 0;
		VoxelSizemm[i] = 0;
	}
	buffIdx = 1;
	dispIdx = 0;
	while (SpecialVoxelPos.Num() < 65) {
		SpecialVoxelPos.Add(FVector(0, 0, 0));
		SpecialVoxelRotation.Add(FRotator(0,0,0));
	}
	FString startLogMsg = FString("Starting voxel source component with ID: ") + ClientConfigID;
	UE_LOG(VoxLog, Log, TEXT("%s"), *startLogMsg);

	VIMRconfig = new VIMR::Config::UnrealConfigWrapper();
	FString localConfigFilePath = FPaths::ProjectDir() + FString("Local_Config.json");
	
	if (!VIMRconfig->Load(TCHAR_TO_ANSI(*localConfigFilePath))) {
		FString msg = FString("Failed to load config file: ") + localConfigFilePath;
		UE_LOG(VoxLog, Warning, TEXT("%s"), *msg);
		//UKismetSystemLibrary::QuitGame(GetWorld(), GetWorld()->GetFirstPlayerController(), EQuitPreference::Quit);
	}

	char* local_conf_file, *shared_conf_file, *shared_data_path;
	size_t ln;
	
	if(VIMRconfig->GetString("LocalConfigFilePath", &local_conf_file, ln)){
		UE_LOG(VoxLog, Log, TEXT("Loaded %s"), ANSI_TO_TCHAR(local_conf_file));
	}
	
	if(VIMRconfig->GetString("SharedConfigPath", &shared_conf_file, ln)){
		UE_LOG(VoxLog, Log, TEXT("Loaded %s"), ANSI_TO_TCHAR(shared_conf_file));
	}
	
	if(VIMRconfig->GetString("SharedDataPath", &shared_data_path, ln)){
		UE_LOG(VoxLog, Log, TEXT("Shared Data at: %s"), ANSI_TO_TCHAR(shared_data_path));
	}
}

void UVoxelSourceBaseComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
	for (int i = 0; i < BufferSize; i++) {
		delete CoarsePositionData[i];
		delete PositionData[i];
		delete ColourData[i];
		VoxelCount[i] = 0;
		VoxelSizemm[i] = 0;
	}
	inProgress = false;
}


void UVoxelSourceBaseComponent::GetFramePointers(int & VoxelCount, uint8 *& CoarsePositionData, uint8 *& PositionData, uint8 *& ColourData, uint8 & Voxelmm)
{
	inDisplay = true;
	Voxelmm = this->VoxelSizemm[dispIdx];
	VoxelCount = this->VoxelCount[dispIdx];
	CoarsePositionData = this->CoarsePositionData[dispIdx];
	PositionData = this->PositionData[dispIdx];
	ColourData = this->ColourData[dispIdx];
	inDisplay = false;
}

// Called every frame
void UVoxelSourceBaseComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

