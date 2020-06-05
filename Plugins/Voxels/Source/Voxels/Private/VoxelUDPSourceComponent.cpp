// Fill out your copyright notice in the Description page of Project Settings.

#include "VoxelUDPSourceComponent.h"
#include "Engine.h"
#include "IXRTrackingSystem.h"
#include "IHeadMountedDisplay.h"
#include "SerializablePose.hpp"
#include <chrono>
#include <sstream>
#include "VoxelRenderSubComponent.h"
#include <fstream>

using namespace std::placeholders;

// Sets default values for this component's properties
UVoxelUDPSourceComponent::UVoxelUDPSourceComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;
	
}

std::ofstream* pose_f_out;

// Called when the game starts
void UVoxelUDPSourceComponent::BeginPlay()
{
	Super::BeginPlay();
	SetComponentTickEnabled(false);
	sendPoses = false;
	char* cliAddr, *cliPort, *posePort, *poseAddr;
	char* posedests;
	size_t sln;
	if(VIMRconfig->GetComponentConfigVal(TCHAR_TO_ANSI(*ClientConfigID),"Address",&cliAddr,sln)){
		// Success
	}else{/*fail*/}
	if(VIMRconfig->GetComponentConfigVal(TCHAR_TO_ANSI(*ClientConfigID),"Port",&cliPort,sln)){
		// Success
	}else{/*fail*/}
	consumer = new VIMR::Async::RingbufferConsumer<VIMR::VoxelGrid, 8>(std::bind(&UVoxelSourceBaseComponent::CopyVoxelData, this, std::placeholders::_1));

	if (VIMRconfig->GetComponentConfigVal(TCHAR_TO_ANSI(*ClientConfigID), "PoseDestinations", &posedests, sln)) {
		std::stringstream strmdsts_csv(posedests);
		while (strmdsts_csv.good()) {
			string* destID = new string();
			std::getline(strmdsts_csv, *destID, ',');
			if (*destID == "") continue; // Ignore empty stream destinations
			if (VIMRconfig->GetComponentConfigVal(destID->c_str(), "Address", &poseAddr, sln) && VIMRconfig->GetComponentConfigVal(destID->c_str(), "PosePort", &posePort, sln)) {
				pose_senders[*destID] = new VIMR::Network::UDPSenderAsync();
				if (pose_senders[*destID]->Initialize(poseAddr, posePort)) {
					if (pose_senders[*destID]->OpenConnection()) {
						UE_LOG(VoxLog, Log, TEXT("Sending poses to %s  %s:%s"), *ClientConfigID, ANSI_TO_TCHAR(poseAddr), ANSI_TO_TCHAR(posePort));
						char* fppath;
						VIMRconfig->GetString("SharedDataPath", &fppath, sln);
						std::string hmdoutpath = std::string(fppath) + "hmdpose.csv";
						/*pose_f_out = new std::ofstream(hmdoutpath.c_str());
						if (pose_f_out->is_open()) {
							UE_LOG(VoxLog, Log, TEXT("Saving poses to %s"), ANSI_TO_TCHAR(hmdoutpath.c_str()));
						}*/
						sendPoses = true;
					}
				}
			}
		}
	}

	deserializer = new VIMR::Deserializer(std::bind(&VIMR::Async::RingbufferConsumer<VIMR::VoxelGrid, 8>::Consume, consumer));
	UE_LOG(VoxLog, Log, TEXT("Adding receiver %s  %s:%s"), *ClientConfigID, ANSI_TO_TCHAR(cliAddr), ANSI_TO_TCHAR(cliPort));;
	if (!deserializer->AddReceiver(TCHAR_TO_ANSI(*ClientConfigID), cliAddr, cliPort)) {
		UE_LOG(VoxLog, Error, TEXT("Adding receiver %s  %s:%s"), *ClientConfigID, ANSI_TO_TCHAR(cliAddr), ANSI_TO_TCHAR(cliPort));;
		UKismetSystemLibrary::QuitGame(GetWorld(), GetWorld()->GetFirstPlayerController(), EQuitPreference::Quit);
	}
	else {
		SetComponentTickEnabled(true);
	}
}

void UVoxelUDPSourceComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	deserializer->Stop();
	for (auto ps : pose_senders)
		ps.second->CloseConnection();
	consumer->Stop();
	if (pose_f_out && pose_f_out->is_open())
		pose_f_out->close();
	Super::EndPlay(EndPlayReason);
	//deserializer->EndThread();
	//delete deserializer;
	//voxelcallback->EndThread();// FIXME: SHould delete this.
}


VIMR::Utils::SerializablePose sp;
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
				sp.timestamp_ms = VIMR::Utils::Utils::getms();
				sp.quat[0] = q.W;
				sp.quat[1] = q.X;
				sp.quat[2] = q.Y;
				sp.quat[3] = q.Z;
				sp.tran[0] = p.X / 100.0;
				sp.tran[1] = p.Y / 100.0;
				sp.tran[2] = p.Z / 100.0;
				if (pose_f_out && pose_f_out->is_open()){
					std::string spcsv = sp.ToCSV() + "\n";
					pose_f_out->write(spcsv.c_str(), spcsv.length());
				}
				sp.ToBytes(pose_buf);
				for (auto ps : pose_senders) {
					ps.second->Push(&pose_buf);
				}
				//GEngine->AddOnScreenDebugMessage(-1, 500, FColorList::Red, pose_str);
			}
		}
	}
}

