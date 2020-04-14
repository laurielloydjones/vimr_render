// Fill out your copyright notice in the Description page of Project Settings.

#include "VoxelUDPSourceComponent.h"
#include "Engine.h"
#include <chrono>
#include "VoxelRenderSubComponent.h"
//#include <exception>

using namespace VIMR;
using namespace std::placeholders;

// Sets default values for this component's properties
UVoxelUDPSourceComponent::UVoxelUDPSourceComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = false;
	
}

// Called when the game starts
void UVoxelUDPSourceComponent::BeginPlay()
{
	Super::BeginPlay();
	SetComponentTickEnabled(false);
	std::map<string, map<string, string>> NetworkConfig;
	if (!VIMRconfig->get<std::map<string, map<string, string>>>("Components", NetworkConfig)) {
		FString msg = FString("Failed to get config key 'NetworkConfig'");
		UE_LOG(VoxLog, Warning, TEXT("%s"), *msg);
		UKismetSystemLibrary::QuitGame(GetWorld(), GetWorld()->GetFirstPlayerController(), EQuitPreference::Quit);
	}
	if (NetworkConfig.count(TCHAR_TO_ANSI(*ClientConfigID)) == 0) {
		FString msg = FString("Network Config doesn't contain clientID key");
		UE_LOG(VoxLog, Warning, TEXT("%s"), *msg);
		UKismetSystemLibrary::QuitGame(GetWorld(), GetWorld()->GetFirstPlayerController(), EQuitPreference::Quit);
	}
	clientConfig = NetworkConfig[TCHAR_TO_ANSI(*ClientConfigID)];
	consumer = new VIMR::Async::RingbufferConsumer<VIMR::VoxelGrid, 8>(
		std::bind(&UVoxelSourceBaseComponent::CopyVoxelData, this, std::placeholders::_1)
		);
	deserializer = new Deserializer(std::bind(&VIMR::Async::RingbufferConsumer<VIMR::VoxelGrid, 8>::Consume, consumer));
	if (!deserializer->AddReceiver(TCHAR_TO_ANSI(*ClientConfigID), clientConfig["Address"].c_str(), clientConfig["Port"].c_str())) {
		UE_LOG(VoxLog, Error, TEXT("Adding receiver for client %s failed. Check configuration."), *ClientConfigID);
		UKismetSystemLibrary::QuitGame(GetWorld(), GetWorld()->GetFirstPlayerController(), EQuitPreference::Quit);
	}
	else {
		SetComponentTickEnabled(true);
	}
}

void UVoxelUDPSourceComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	deserializer->Stop();
	consumer->Stop();
	Super::EndPlay(EndPlayReason);
	//deserializer->EndThread();
	//delete deserializer;
	//voxelcallback->EndThread();// FIXME: SHould delete this.
}

// Called every frame
void UVoxelUDPSourceComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

