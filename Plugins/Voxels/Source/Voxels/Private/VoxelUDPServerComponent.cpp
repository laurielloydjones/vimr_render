// Fill out your copyright notice in the Description page of Project Settings.

#include "VoxelUDPServerComponent.h"
#include "Engine.h"
#include <chrono>
#include "VoxelRenderSubComponent.h"
//#include <exception>

using namespace VIMR;
using namespace std::placeholders;

// Sets default values for this component's properties
UVoxelUDPServerComponent::UVoxelUDPServerComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = false;
	
}

// Called when the game starts
void UVoxelUDPServerComponent::BeginPlay()
{
	Super::BeginPlay();
	SetComponentTickEnabled(false);
	server = new VIMR::VoxelServer();
	if (!server->Initialize()) {
		FString msg = FString("Failed to initialize Voxel Server");
		UE_LOG(VoxLog, Warning, TEXT("%s"), *msg);
		UKismetSystemLibrary::QuitGame(GetWorld(), GetWorld()->GetFirstPlayerController(), EQuitPreference::Quit);
	}
	copyVoxelDataCallback = new Stu::AsyncQueuCallback<VIMR::Octree, MESSAGE_BUFFER_COUNT>(std::bind(&UVoxelSourceBaseComponent::CopyVoxelData, this, _1));
	server->RegisterVoxelCallback("rndr", copyVoxelDataCallback);
	SetComponentTickEnabled(true);
}

void UVoxelUDPServerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

}

// Called every frame
void UVoxelUDPServerComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

