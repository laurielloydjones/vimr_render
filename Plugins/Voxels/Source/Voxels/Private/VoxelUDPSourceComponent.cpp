// Fill out your copyright notice in the Description page of Project Settings.

#include "VoxelUDPSourceComponent.h"
#include "Engine.h"
#include "IXRTrackingSystem.h"
#include "IHeadMountedDisplay.h"
#include <chrono>
#include <sstream>
#include "VoxelRenderSubComponent.h"

using namespace std::placeholders;

// Sets default values for this component's properties
UVoxelUDPSourceComponent::UVoxelUDPSourceComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;
	
}

// Called when the game starts
void UVoxelUDPSourceComponent::BeginPlay()
{
	Super::BeginPlay();
	SetComponentTickEnabled(false);

	consumer = new VIMR::Async::RingbufferConsumer<VIMR::Octree, 8>(std::bind(&UVoxelSourceBaseComponent::CopyVoxelData, this, std::placeholders::_1));

	sendPoses = false;
	char* cliAddr, *cliPort, *posePort, *poseAddr;
	char* posedests;
	size_t sln;
	if (!VIMRconfig->GetComponentConfigVal(TCHAR_TO_ANSI(*ClientConfigID), "Addr", &cliAddr, sln)) {
		UE_LOG(VoxLog, Log, TEXT("Failed to get config key %s:Addr"), *ClientConfigID);
	}
	else if (!VIMRconfig->GetComponentConfigVal(TCHAR_TO_ANSI(*ClientConfigID), "Port", &cliPort, sln)) {
		UE_LOG(VoxLog, Log, TEXT("Failed to get config key %s:Port"), *ClientConfigID);
	}
	else {
		deserializer = new VIMR::Deserializer(std::bind(&VIMR::Async::RingbufferConsumer<VIMR::Octree, 8>::Consume, consumer));
		UE_LOG(VoxLog, Log, TEXT("Adding receiver %s  %s:%s"), *ClientConfigID, ANSI_TO_TCHAR(cliAddr), ANSI_TO_TCHAR(cliPort));
		if (!deserializer->AddReceiver(TCHAR_TO_ANSI(*ClientConfigID), cliAddr, cliPort)) {
			UE_LOG(VoxLog, Error, TEXT("Adding receiver %s  %s:%s"), *ClientConfigID, ANSI_TO_TCHAR(cliAddr), ANSI_TO_TCHAR(cliPort));
		}
	}

	if (VIMRconfig->GetComponentConfigVal(TCHAR_TO_ANSI(*ClientConfigID), "PoseDests", &posedests, sln)) {
		std::stringstream strmdsts_csv(posedests);
		UE_LOG(VoxLog, Log, TEXT("PoseDests: %s"), ANSI_TO_TCHAR(posedests));
		while (strmdsts_csv.good()) {
			string* destID = new string();
			std::getline(strmdsts_csv, *destID, ',');
			if (*destID == "") continue;
			if (VIMRconfig->GetComponentConfigVal(destID->c_str(), "Addr", &poseAddr, sln) && VIMRconfig->GetComponentConfigVal(destID->c_str(), "PosePort", &posePort, sln)) {
				pose_senders[*destID] = new VIMR::Network::UDPSenderAsync();
				if (pose_senders[*destID]->Open(TCHAR_TO_ANSI(*ClientConfigID), poseAddr, posePort)) {
					UE_LOG(VoxLog, Log, TEXT("Sending poses to %s:%s"), ANSI_TO_TCHAR(poseAddr), ANSI_TO_TCHAR(posePort));
					char* fppath;
					VIMRconfig->GetString("SharedDataPath", &fppath, sln);
					std::string hmdoutpath = std::string(fppath) + "hmdpose.csv";
					sendPoses = true;
				}
			}
		}
	} else {
		UE_LOG(VoxLog, Log, TEXT("Not sending poses: Missing key %s:PoseDestinations"), *ClientConfigID);
	}
	SetComponentTickEnabled(true);
}

void UVoxelUDPSourceComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if(deserializer)
		deserializer->Stop();
	for (auto ps : pose_senders)
		ps.second->Close();
	consumer->Stop();
	Super::EndPlay(EndPlayReason);
}

// Called every frame
void UVoxelUDPSourceComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	FQuat q;
	FVector p;
	
	if (sendPoses) {
		if (GEngine && GEngine->XRSystem.IsValid()) {
			if (GEngine->XRSystem->CountTrackedDevices(EXRTrackedDeviceType::HeadMountedDisplay) == 1) {
				GEngine->XRSystem->GetCurrentPose(GEngine->XRSystem->HMDDeviceId, q, p);
				FRotator fr(q);
				FString pose_str = p.ToString() + " -- " + fr.ToString();
				pose_buf.Reset();
				hmd_pose.timestamp_ms = VIMR::Utils::Utils::getms();
				hmd_pose.quat[0] = q.W;
				hmd_pose.quat[1] = q.X;
				hmd_pose.quat[2] = q.Y;
				hmd_pose.quat[3] = q.Z;
				hmd_pose.tran[0] = p.X / 100.0;
				hmd_pose.tran[1] = p.Y / 100.0;
				hmd_pose.tran[2] = p.Z / 100.0;
				hmd_pose.ToBytes(pose_buf);
				for (auto ps : pose_senders) {
					ps.second->Send(&pose_buf);
				}
			}
		}
	}
}