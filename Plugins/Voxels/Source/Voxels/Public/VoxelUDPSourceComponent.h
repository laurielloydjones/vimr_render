// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "VoxelSourceBaseComponent.h"
#include "VIMR/Deserializer.hpp"
#include "DataBuffer.hpp"
#include "UDPStreamAsync.hpp"
#include "Async.hpp"
#include <map>
#include <string>
#include "VoxelUDPSourceComponent.generated.h"

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class VOXELS_API UVoxelUDPSourceComponent : public UVoxelSourceBaseComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UVoxelUDPSourceComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	
	// Called when the game starts
	virtual void BeginPlay() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	
	VIMR::Deserializer* deserializer = nullptr;
	bool sendPoses = false;
	std::map<std::string, VIMR::Network::UDPSenderAsync*> pose_senders;
	VIMR::Utils::Buffer<char, 128> pose_buf;
	VIMR::Async::RingbufferConsumer<VIMR::VoxelGrid, 8>* consumer = nullptr;
};
