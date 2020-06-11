// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "VoxelSourceInterface.h"
#include "VIMR/VoxelType.hpp"
#include "Voxels.h"
#include "VoxelRenderComponent.h"
#include "AllowWindowsPlatformTypes.h"
#include "VIMR/UnrealConfigWrapper.hpp"
#include "HideWindowsPlatformTypes.h"

#include <map>
#include <string>

#include "VoxelSourceBaseComponent.generated.h"

using std::map;
using std::string;

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class VOXELS_API UVoxelSourceBaseComponent : public USceneComponent, public IVoxelSourceInterface
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UVoxelSourceBaseComponent();

	void GetFramePointers(int &VoxelCount, uint8*& CoarsePositionData, uint8*& PositionData, uint8*& ColourData, uint8& Voxelmm) override;
	
	int GetSourceType() { return UDPSource; }

	void CopyVoxelData(VIMR::VoxelGrid* voxels);

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
		FString ClientConfigID = "Render";

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
		bool ShowSpecialVoxels = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
		bool ColorBodyVoxels = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
		bool ShowBodyVoxelsOnly = false;
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
		TArray<FVector> SpecialVoxelPos;
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
		TArray<FRotator> SpecialVoxelRotation;
protected:

	// Called when the game starts
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	
	VIMR::Config::UnrealConfigWrapper* VIMRconfig = nullptr;
	VIMR::AVoxelGrid::Voxel *node;

	static const int BufferSize = 2;
	uint32_t MaxVoxels = TOTAL_VOXELS; //FIXME: Hard-coded stuff here
	uint8* CoarsePositionData[BufferSize];
	uint8* PositionData[BufferSize];
	uint8* ColourData[BufferSize];
	uint8 VoxelSizemm[BufferSize];
	uint32_t VoxelCount[BufferSize];

	int buffIdx;
	int dispIdx;

	bool inProgress = false;
	bool inDisplay = false;
};
