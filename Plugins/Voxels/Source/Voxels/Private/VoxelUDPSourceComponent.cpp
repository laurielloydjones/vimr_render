// Fill out your copyright notice in the Description page of Project Settings.

#include "VoxelUDPSourceComponent.h"
#include "Engine.h"
#include <chrono>
#include "VoxelRenderSubComponent.h"

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

	char* cliAddr, *cliPort;
	size_t sln;
	if(VIMRconfig->GetComponentConfigVal(TCHAR_TO_ANSI(*ClientConfigID),"Address",&cliAddr,sln)){
		// Success
	}else{/*fail*/}
	if(VIMRconfig->GetComponentConfigVal(TCHAR_TO_ANSI(*ClientConfigID),"Port",&cliPort,sln)){
		// Success
	}else{/*fail*/}
	consumer = new VIMR::Async::RingbufferConsumer<VIMR::VoxelGrid, 8>(std::bind(&UVoxelSourceBaseComponent::CopyVoxelData, this, std::placeholders::_1));

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

