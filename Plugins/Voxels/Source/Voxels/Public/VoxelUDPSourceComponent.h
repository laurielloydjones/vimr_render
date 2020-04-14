// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "VoxelSourceBaseComponent.h"
#include "VIMR/Deserializer.hpp"
#include "Async.hpp"
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
	VIMR::Async::RingbufferConsumer<VIMR::VoxelGrid, 8>* consumer = nullptr;
};
