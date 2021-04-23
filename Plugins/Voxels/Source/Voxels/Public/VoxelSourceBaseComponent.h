// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "VoxelSourceInterface.h"
#include "VIMR/VoxGrid.hpp"
#include "VIMR/Octree.hpp"
#include "Voxels.h"
#include "VoxelRenderComponent.h"
#include "AllowWindowsPlatformTypes.h"
#include "VIMR/cfg_unreal.hpp"
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
		bool ColourVoxelSource = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
		bool RenderSkeleton = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
		bool ShowBodyVoxelsOnly = false;
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
		TArray<FVector> SpecialVoxelPos;
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
		TArray<FRotator> SpecialVoxelRotation;
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
		TArray<FVector> JointPositions;
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
		TArray<FVector> Bone_dir;
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
		TArray<FVector> Bone_pos;
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
		float VoxelSize_mm = 8.0;
protected:
	// Called when the game starts
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	VIMR::Config::UnrealConfigWrapper* VIMRconfig = nullptr;
	VIMR::Voxel *node;

	const int BODY_COUNT = 6;

	uint8 src_rgb[3 * 6] = {
		255,0,128,
	255,128,0,
	128,0,255,
		0,128,255,
		0,255,0
	};
	uint8 body_rgb[3 * 20]{
	  255,0,0,    //Head
	  255,128,0,  // Neck
	  0,255,0,    //LH
	  0,255,0,    //RH
	  0,255,0,    //LFoot
	  0,255,0,    //RFoot
	  128,255,0,  //LShoulder
	  128,0,255,  //RShoulder
	  255,128,0,  //LUpperArm
	  255,128,0,  //RUpperArm
	  0,128,255,  //LForearm
	  0,128,255,  //RForearm
	  128,255,0,  //LHip
	  128,0,255,  //RHip
	  128,255,0,  //LThigh
	  128,0,255,  //RThigh
	  255,128,0,  //LCalf
	  255,128,0,  //RCalf
	  64,64,64,   //RCalf
	  128,128,128 //RCalf
	};

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
