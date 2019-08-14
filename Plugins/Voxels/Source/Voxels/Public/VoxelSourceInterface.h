#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "VoxelSourceInterface.generated.h"

UINTERFACE()
class VOXELS_API UVoxelSourceInterface : public UInterface
{
	GENERATED_BODY()
};

class VOXELS_API IVoxelSourceInterface
{
	GENERATED_BODY()

public:
	virtual void GetFramePointers(int& VoxelCount, uint8*& CoarsePositionData, uint8*& PositionData, uint8*& ColourData, uint8& voxelmm) = 0;

	UFUNCTION()
	virtual int GetSourceType() = 0;
};
