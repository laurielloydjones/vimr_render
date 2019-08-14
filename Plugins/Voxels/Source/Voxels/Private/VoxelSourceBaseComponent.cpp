// Fill out your copyright notice in the Description page of Project Settings.

#include "VoxelSourceBaseComponent.h"
#include "Engine.h"
#include "VoxelRenderSubComponent.h"
#include"Runtime/Engine/Classes/Kismet/KismetSystemLibrary.h"
#include "VIMR/semanticlabel.hpp"
#include <chrono>
using namespace VIMR;
using namespace std::placeholders;



DEFINE_LOG_CATEGORY(VoxLog);

const uint8 colors[][3]  =
{
	// These are RGB but are converted to BGR when applied to a voxel

{255,255,128}, // head
{255,128,128}, // left hand
{128,128,255}, // right hand
{255,128,128}, // left foot
{128,128,255}, // right foot
{128,255,128}, // neck
{128,128,255}, // left shoulder
{255,128,128}, // right shoulder
{128,255,128}, // left arm
{128,255,128}, // right arm
{212,170,212}, // left forearm
{212,170,255}, // right forearm
{128,192,128}, // upper torso
{192,255,192}, // lower torso
{128,128,255}, // left hip
{255,128,128}, // right hip
{255,128,128}, // left thigh
{128,128,255}, // right thigh
{128,255,128}, // left calf
{128,255,128} // right calf

};

void UVoxelSourceBaseComponent::CopyVoxelData(Octree* voxels) {
	if (inProgress) {
		FString failLogMessage = FString("Received more voxels before copying last frame finished. ID: ") + ClientConfigID;
		UE_LOG(VoxLog, Log, TEXT("%s"), *failLogMessage);
		return;
	}
	
	inProgress = true;
	VoxelCount[buffIdx] = 0;
	int pOffset = 0;
	int hasNegativeVoxels = 0;
	long voxOffset = 0;
	if (SerialOctreeFmtVersion == 2)
		voxOffset = voxels->Width() / 2;
	else {
		voxOffset = voxels->Width() / 4;
	}
	
	int numleaves = voxels->NumLeaves();
	
	while (voxels->GetNextLeafNode(&node)) {
		
		uint16_t pY = node->pos.y - voxOffset;
		uint16_t pX = node->pos.x - voxOffset;
		uint16_t pZ = node->pos.z;
		
		uint8 voxLabel = GetMiscLabel(node->data[0]);
		if (voxLabel == VOXLABEL_REGULARVOXEL) {
			if (!ShowBodyVoxelsOnly) {
				CoarsePositionData[buffIdx][pOffset + 0] = (pZ >> 8) + 128;
				CoarsePositionData[buffIdx][pOffset + 1] = (pY >> 8) + 128;
				CoarsePositionData[buffIdx][pOffset + 2] = (pX >> 8) + 128;


				PositionData[buffIdx][pOffset + 0] = pZ & 0xFF;
				PositionData[buffIdx][pOffset + 1] = pY & 0xFF;
				PositionData[buffIdx][pOffset + 2] = pX & 0xFF;

				ColourData[buffIdx][pOffset + 0] = node->data[1];
				ColourData[buffIdx][pOffset + 1] = node->data[2];
				ColourData[buffIdx][pOffset + 2] = node->data[3];
				pOffset += VOXEL_TEXTURE_BPP;
				VoxelCount[buffIdx]++;
			}
		}
		else if (voxLabel < VOXLABEL_BODYPARTOFFSET) {
			if (ShowSpecialVoxels) {
				CoarsePositionData[buffIdx][pOffset + 0] = (pZ >> 8) + 128;
				CoarsePositionData[buffIdx][pOffset + 1] = (pY >> 8) + 128;
				CoarsePositionData[buffIdx][pOffset + 2] = (pX >> 8) + 128;

				PositionData[buffIdx][pOffset + 0] = pZ & 0xFF;
				PositionData[buffIdx][pOffset + 1] = pY & 0xFF;
				PositionData[buffIdx][pOffset + 2] = pX & 0xFF;

				ColourData[buffIdx][pOffset + 0] = 0;
				ColourData[buffIdx][pOffset + 1] = 0xff;
				ColourData[buffIdx][pOffset + 2] = 0;
				pOffset += VOXEL_TEXTURE_BPP;
				VoxelCount[buffIdx]++;
			}
		}
		else if (voxLabel >= VOXLABEL_BODYPARTOFFSET) {
			CoarsePositionData[buffIdx][pOffset + 0] = (pZ >> 8) + 128;
			CoarsePositionData[buffIdx][pOffset + 1] = (pY >> 8) + 128;
			CoarsePositionData[buffIdx][pOffset + 2] = (pX >> 8) + 128;

			PositionData[buffIdx][pOffset + 0] = pZ & 0xFF;
			PositionData[buffIdx][pOffset + 1] = pY & 0xFF;
			PositionData[buffIdx][pOffset + 2] = pX & 0xFF;

			if (ColorBodyVoxels) {
				uint8 bodyPartLabel = voxLabel - VOXLABEL_BODYPARTOFFSET;
				ColourData[buffIdx][pOffset + 0] = colors[bodyPartLabel][2];
				ColourData[buffIdx][pOffset + 1] = colors[bodyPartLabel][1];
				ColourData[buffIdx][pOffset + 2] = colors[bodyPartLabel][0];
			}
			else {
				ColourData[buffIdx][pOffset + 0] = node->data[1];
				ColourData[buffIdx][pOffset + 1] = node->data[2];
				ColourData[buffIdx][pOffset + 2] = node->data[3];
			}
			pOffset += VOXEL_TEXTURE_BPP;
			VoxelCount[buffIdx]++;
		}
		// This case is for normal voxels which only have colour data 
		// plus camera-source label
		else {
			UE_LOG(VoxLog, Log, TEXT("Unknown Voxel label %d"), voxLabel);
		}

		VoxelSizemm[buffIdx] = (uint8)voxels->meta.voxSize_mm;
		
		if (VoxelCount[buffIdx] >= MaxVoxels) {
			FString failLogMessage = FString("Too Many Voxels! ID: ") + ClientConfigID;
			UE_LOG(VoxLog, Log, TEXT("%s"), *failLogMessage);
			break;
		}
	}
	if (hasNegativeVoxels > 0)
		UE_LOG(VoxLog, Log, TEXT("NegativeVoxels: %d"), hasNegativeVoxels);
	//UE_LOG(VoxLog, Log, TEXT("Received voxels: %d"), voxels->LeafCount());
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
	FString startLogMsg = FString("Starting voxel source component with ID: ") + ClientConfigID;
	UE_LOG(VoxLog, Log, TEXT("%s"), *startLogMsg);
	VIMRconfig = new Stu::Config::ConfigFile();
	FString localConfigFilePath = FPaths::ProjectDir() + FString(LOCAL_CONFIG_FILENAME);
	
	if (!VIMRconfig->Load(TCHAR_TO_ANSI(*localConfigFilePath))) {
		FString msg = FString("Failed to load config file: ") + localConfigFilePath;

		UE_LOG(VoxLog, Warning, TEXT("%s"), *msg);
		UKismetSystemLibrary::QuitGame(GetWorld(), GetWorld()->GetFirstPlayerController(), EQuitPreference::Quit);
	}
	string loadedLocalPath;
	VIMRconfig->get<string>("LocalConfigFilePath", loadedLocalPath);
	UE_LOG(VoxLog, Log, TEXT("Loaded %s"), ANSI_TO_TCHAR(loadedLocalPath.c_str()));
	VIMRconfig->get<string>("SharedConfigPath", loadedLocalPath);
	UE_LOG(VoxLog, Log, TEXT("Loaded %s"), ANSI_TO_TCHAR(loadedLocalPath.c_str()));
	VIMRconfig->get<string>("SharedDataPath", loadedLocalPath);
	UE_LOG(VoxLog, Log, TEXT("Shared Data at: %s"), ANSI_TO_TCHAR(loadedLocalPath.c_str()));
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

