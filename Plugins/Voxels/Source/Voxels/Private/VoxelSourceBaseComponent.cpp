// Fill out your copyright notice in the Description page of Project Settings.

#include "VoxelSourceBaseComponent.h"
#include "Engine.h"
#include "VoxelRenderSubComponent.h"
#include"Runtime/Engine/Classes/Kismet/KismetSystemLibrary.h"
#include <chrono>
#include <functional>
#include "VIMR/body.hpp"
using namespace std::placeholders;

DEFINE_LOG_CATEGORY(VoxLog);

using VIMR::Bone;

const VIMR::Bone VIMR::skeleton[] = {
  Bone{ VIMR::JointType_WristLeft, VIMR::JointType_HandTipLeft },
  Bone{ VIMR::JointType_WristRight, VIMR::JointType_HandTipRight },
  Bone{ VIMR::JointType_AnkleLeft, VIMR::JointType_FootLeft },
  Bone{ VIMR::JointType_AnkleRight, VIMR::JointType_FootRight },
  Bone{ VIMR::JointType_Neck, VIMR::JointType_Head },
  Bone{ VIMR::JointType_SpineShoulder, VIMR::JointType_Neck },
  Bone{ VIMR::JointType_SpineShoulder, VIMR::JointType_ShoulderLeft },
  Bone{ VIMR::JointType_SpineShoulder, VIMR::JointType_ShoulderRight },
  Bone{ VIMR::JointType_ShoulderLeft, VIMR::JointType_ElbowLeft },
  Bone{ VIMR::JointType_ShoulderRight, VIMR::JointType_ElbowRight },
  Bone{ VIMR::JointType_ElbowLeft, VIMR::JointType_WristLeft },
  Bone{ VIMR::JointType_ElbowRight, VIMR::JointType_WristRight },
  Bone{ VIMR::JointType_SpineMid, VIMR::JointType_SpineShoulder },
  Bone{ VIMR::JointType_SpineBase, VIMR::JointType_SpineMid },
  Bone{ VIMR::JointType_SpineBase, VIMR::JointType_HipLeft },
  Bone{ VIMR::JointType_SpineBase, VIMR::JointType_HipRight },
  Bone{ VIMR::JointType_HipLeft, VIMR::JointType_KneeLeft },
  Bone{ VIMR::JointType_HipRight, VIMR::JointType_KneeRight },
  Bone{ VIMR::JointType_KneeLeft, VIMR::JointType_AnkleLeft },
  Bone{ VIMR::JointType_KneeRight, VIMR::JointType_AnkleRight }
};

void UVoxelSourceBaseComponent::CopyVoxelData(VIMR::VoxelGrid* voxels) {
	if (inProgress) {
		FString failLogMessage = FString("Received more voxels before copying last frame finished. ID: ") + ClientConfigID;
		UE_LOG(VoxLog, Log, TEXT("%s"), *failLogMessage);
		return;
	}

	inProgress = true;
	VoxelCount[buffIdx] = 0;
	VoxelSizemm[buffIdx] = (uint8)voxels->VoxSize_mm();
	VoxelSize_mm = voxels->VoxSize_mm();
	//FIXME: A way to check if the octree contains nodes which have aux labels

	int pOffset = 0;
	while (voxels->GetNextVoxel(&node)) {
		/*
		if (node->GetFlag(VIMR::Voxel::Flags::Hidden) != 0){
			//continue;
		}


		int aux = node->GetAux();

		if (node->GetFlag(VIMR::Voxel::Flags::Special) != 0){
			int jIdx = aux % 20;
			JointPositions[jIdx].X = pX;
			JointPositions[jIdx].Y = pY;
			JointPositions[jIdx].Z = pZ;
		}


		if(ColourVoxelSource){
			int s=node->GetSrc();
			memcpy(&ColourData[buffIdx][pOffset], &src_rgb[3 * (s % BODY_COUNT)], 3);
		}else if(ColorBodyVoxels && aux < 100){
			memcpy(&ColourData[buffIdx][pOffset], &body_rgb[3 * (aux % 20)], 3);
		}else{
			node->read_data((char*)&ColourData[buffIdx][pOffset]);
		}*/
		node->read_data((char*)&ColourData[buffIdx][pOffset]);
		int16_t pY = node->pos.Y;
		int16_t pX = node->pos.X;
		int16_t pZ = node->pos.Z;
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

	//UE_LOG(VoxLog, Log, TEXT("VoxCount=%i"), VoxelCount[buffIdx]);
	/*for (int i = 0; i < 20; i++)
	{
		Bone_pos[i] = 0.5* (JointPositions[VIMR::skeleton[i].End] + JointPositions[VIMR::skeleton[i].Start]); // out of bounds range check 
		Bone_dir[i] = JointPositions[VIMR::skeleton[i].End] - JointPositions[VIMR::skeleton[i].Start];
	}*/

	while (inDisplay)
		;

	buffIdx = (buffIdx + 1) % BufferSize;
	dispIdx = (dispIdx + 1) % BufferSize;
	inProgress = false;
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
		SpecialVoxelRotation.Add(FRotator(0, 0, 0));
	}
	while (JointPositions.Num() < 30)  JointPositions.Add(FVector(0, 0, 0));
	while (Bone_dir.Num() < 21)  Bone_dir.Add(FVector(0, 0, 0));
	while (Bone_pos.Num() < 21)  Bone_pos.Add(FVector(0, 0, 0));

	FString startLogMsg = FString("Starting voxel source component with ID: ") + ClientConfigID;
	UE_LOG(VoxLog, Log, TEXT("%s"), *startLogMsg);

	VIMRconfig = new VIMR::Config::UnrealConfigWrapper();
	FString localConfigFilePath = FPaths::ProjectDir() + FString("LocalConfig.json");

	if (!VIMRconfig->Load(TCHAR_TO_ANSI(*localConfigFilePath))) {
		FString msg = FString("Failed to load config file: ") + localConfigFilePath;
		UE_LOG(VoxLog, Warning, TEXT("%s"), *msg);
	}
	else {
		FString msg = FString("Loaded local config file: ") + localConfigFilePath;
		UE_LOG(VoxLog, Log, TEXT("%s"), *msg);
	}

	char *shared_conf_file, *shared_data_path;
	size_t ln;

	if (VIMRconfig->GetString("SharedConfigPath", &shared_conf_file, ln)) {
		UE_LOG(VoxLog, Log, TEXT("Loaded %s"), ANSI_TO_TCHAR(shared_conf_file));
	}

	if (VIMRconfig->GetString("SharedDataPath", &shared_data_path, ln)) {
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
	SpecialVoxelPos.Empty();
	SpecialVoxelRotation.Empty();
	JointPositions.Empty();
	Bone_dir.Empty();
	Bone_pos.Empty();
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

