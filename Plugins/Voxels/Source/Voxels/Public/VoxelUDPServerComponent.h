// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "VoxelSourceBaseComponent.h"
#include "VIMR/VoxelServer.hpp"
#include "AsyncBuffer.hpp"
#include "VoxelUDPServerComponent.generated.h"

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class VOXELS_API UVoxelUDPServerComponent : public UVoxelSourceBaseComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UVoxelUDPServerComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	
	// Called when the game starts
	virtual void BeginPlay() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	Stu::AsyncQueuCallback<VIMR::Octree, MESSAGE_BUFFER_COUNT>* copyVoxelDataCallback;
	VIMR::VoxelServer* server;
};
